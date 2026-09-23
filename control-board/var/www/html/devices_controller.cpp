#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <signal.h>
#include <stdint.h>
#include "iic_lib.h"
#include "http_lib/httplib.h"
#include "rapidjson/document.h"
#include "rapidjson/writer.h"
#include <string>
#include <mutex>
#include "config.hpp"
#include "utility/utility.hpp"

using namespace rapidjson;
using namespace httplib;
using namespace CommonEngine;
httplib::Server svr;
Configuration m_config;

std::mutex i2c_locker;
std::mutex uart_locker;

#define MAP_SIZE 4096

#define writeReg(ip_ptr, reg, value) *((unsigned *)(((u_int8_t *)ip_ptr) + reg)) = value
#define readReg(ip_ptr, reg) *((unsigned *)(((u_int8_t *)ip_ptr) + reg))

#define RESET_FPGA_UIO_PATH "/dev/uio7"
#define FAN_UIO_PATH "/dev/uio0"
#define CHECK_HIGH_TEMP_CNT 4

#define TEMP_MAX_SAFETY     95

#define MIN_TEMP_SAFETY 1

#define MIN_TEMP_READABLE -20

/*10 20 50 70 80% 100% */
/*#define FAN_LEVEL_1 1
#define FAN_LEVEL_2 2
#define FAN_LEVEL_3 5
#define FAN_LEVEL_4 7
#define FAN_LEVEL_5 8
#define FAN_LEVEL_6 10*/

#define FAN_LEVEL_1 1
#define FAN_LEVEL_2 2
#define FAN_LEVEL_3 3
#define FAN_LEVEL_4 4
#define FAN_LEVEL_5 5
#define FAN_LEVEL_6 6
#define FAN_LEVEL_7 7
#define FAN_LEVEL_8 8
#define FAN_LEVEL_9 9
#define FAN_LEVEL_10 10

#define TEMP_LEVEL_1 0
#define TEMP_LEVEL_2 1
#define TEMP_LEVEL_3 2
#define TEMP_LEVEL_4 3
#define TEMP_LEVEL_5 4
#define TEMP_LEVEL_6 5
#define TEMP_LEVEL_7 6
#define TEMP_LEVEL_8 7
#define TEMP_LEVEL_9 8
#define TEMP_LEVEL_10 9

#define ON_FPGA 1
#define OFF_FPGA 0
#define NUMBER_FPGA 3

#define GPIO_RELEASE 0
#define GPIO_PRESS 1

#define TEMP_DEVICE_NUM         10
#define MAX_COUNT 2147483645
#define INTERVAL_SEND_NOTI 180
#define INTERVAL_CHECK_RESET_IC_SENSOR 20

typedef struct __attribute__((packed))
{
    // uint8_t temp_i2c_addr[2];
    std::vector<uint8_t> temps_i2c_addr;
    uint8_t voltage_i2c_addr;
    std::string chip_type;
    int temp_max;
    int boardTemp;
    int chipTemp;
    int old_boardTemp;
    int old_chipTemp;
    int fanLevel;
    int index;
    int check_connection;
    int power_on_fpga;
    char path_uio[30];
    int high_temp_cnt;
    int voltage_vccint;
    int voltage_hbm;
    int alert_email_temp_cnt;
    int count_reset_IC_sensor;
} FPGA_Info;

FPGA_Info fpga[NUMBER_FPGA];
int current_fan_level = FAN_LEVEL_6;



// fan
uint8_t fan_mode = 0;
int fan_manual_level = 1;
int temp_device[TEMP_DEVICE_NUM];
// email
uint8_t enable_noti = 0;
std::string desti_email_noti = "notifications@dracaena.io";
uint16_t chip_temp_noti = 70;
uint16_t frequency_noti = 1;
uint8_t enable_hash_noti = 1;
uint8_t enable_fan_noti = 1;
uint16_t over_heat_temp_noti = 1;
uint8_t enable_miner_status_noti = 1;

void init_fpga(CommonEngine::config_info &m_config_info)
{   
    memset(temp_device, 0, sizeof(TEMP_DEVICE_NUM));
    temp_device[TEMP_LEVEL_10] = 70;
    temp_device[TEMP_LEVEL_9] = 65;
    temp_device[TEMP_LEVEL_8] = 60;
    temp_device[TEMP_LEVEL_7] = 55;
    temp_device[TEMP_LEVEL_6] = 50;
    temp_device[TEMP_LEVEL_5] = 45;
    temp_device[TEMP_LEVEL_4] = 40;
    temp_device[TEMP_LEVEL_3] = 35;
    temp_device[TEMP_LEVEL_2] = 30;
    temp_device[TEMP_LEVEL_1] = 30;


    memset(fpga, 0, sizeof(fpga));
    for (int i = 0; i < NUMBER_FPGA; i++)
    {
        for (size_t j = 0; j < m_config_info.temps_i2c_addr.size(); j++)
        {
            fpga[i].temps_i2c_addr.push_back(UtilityFunc::stringToInt(m_config_info.temps_i2c_addr[j]));
        }
        
        // fpga[i].temp_i2c_addr = UtilityFunc::stringToInt(m_config_info.temp_i2c_addr);
        fpga[i].voltage_i2c_addr = UtilityFunc::stringToInt(m_config_info.voltage_i2c_addr);

        if (m_config_info.chips_type.find(i) != m_config_info.chips_type.end())
        {
            fpga[i].chip_type = m_config_info.chips_type[i];
        }

        fpga[i].temp_max = TEMP_MAX_SAFETY;
        if(m_config_info.temps_max.find(i) != m_config_info.temps_max.end()){
            if (m_config_info.temps_max[i] <= TEMP_MAX_SAFETY)
            {
                fpga[i].temp_max = m_config_info.temps_max[i];
            }
        }
        fpga[i].boardTemp = 90;
        fpga[i].chipTemp = 90;
        fpga[i].old_chipTemp = 90;
        fpga[i].old_boardTemp = 90;
        fpga[i].fanLevel = FAN_LEVEL_1;
        fpga[i].check_connection = 0;
        fpga[i].power_on_fpga = 0;
        fpga[i].high_temp_cnt = 0;
        fpga[i].alert_email_temp_cnt = 0;
        fpga[i].count_reset_IC_sensor = 0;
    }
    strcpy(fpga[0].path_uio, "/dev/uio4"); // I2C 1
    strcpy(fpga[1].path_uio, "/dev/uio5"); // I2C 1
    strcpy(fpga[2].path_uio, "/dev/uio6"); // I2C 1

    // init fan and temp
    fan_mode = m_config_info.fan_mode;
    fan_manual_level = m_config_info.fan_manual_level;
    for (size_t i = 0; i < TEMP_DEVICE_NUM; i++)
    {   
        if ( m_config_info.temp_device[i] > 0 && m_config_info.temp_device[i] < 100)
            temp_device[i] = m_config_info.temp_device[i];
    }

    // printf("******************show init : \n **************");
    // printf("printf fan_mode: %d \n", fan_mode);
    // printf("printf fan_manual_level: %d \n", fan_manual_level);
    // for (int i = 0; i < TEMP_DEVICE_NUM; ++i)
    // {
    //      printf("printf temp_device:[%d] =  %d \n",i,   temp_device[i]);
    // }
    // printf("Done init \n");
}

void init_email_config(CommonEngine::config_info &m_config_info){
    enable_noti = std::stoi(m_config_info.enable_noti);
    desti_email_noti = m_config_info.desti_email_noti;
    chip_temp_noti = std::stoi(m_config_info.chip_temp_noti);
    frequency_noti = std::stoi(m_config_info.frequency_noti);
    enable_hash_noti = std::stoi(m_config_info.enable_hash_noti);
    enable_fan_noti = std::stoi(m_config_info.enable_fan_noti);
    over_heat_temp_noti = std::stoi(m_config_info.over_heat_temp_noti);
    enable_miner_status_noti = std::stoi(m_config_info.enable_miner_status_noti);
}

static int on_off_fpga(int inputValue)
{
  //  printf("\n========================================== onboard = %02hhX\n", inputValue);
    void *ip_ptr;
    int fd;

    unsigned int rd_val;

    // printf("mmap fan\n");
    fd = open(RESET_FPGA_UIO_PATH, O_RDWR);

    if (fd < 1)
    {
        perror("Cannot mmap fan \n");
    }

    // mmap
    ip_ptr = mmap(NULL, MAP_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);

    if (ip_ptr == MAP_FAILED)
    {
        /* code */
        perror("mmap fan failed \n");
        return -1;
    }

    // write down to duty cycle 20%
    *((unsigned *)(((u_int8_t *)ip_ptr) + 0x00)) = inputValue;

    munmap(ip_ptr, MAP_SIZE);
    close(fd);
    return 0;
}

static int set_voltages(uint8_t i2c_addr, char *uiod_iic, int voltage_vccint, int voltage_hbm)
{
    void *ip_ptr;
    int fd;
    u8 slaveAddr;
    u8 Buffer[2];
    //printf("set vol_vccint_input = %d, vol_hbm_input = %d\n", voltage_vccint, voltage_hbm);

    uint8_t vol_vccint_step_cvt;
    int vol_vccint_step = (voltage_vccint - 850) / 5;

    uint8_t vol_hbm_step_cvt;
    int vol_hbm_step = (voltage_hbm - 1265) / 5;

    if ((vol_vccint_step > 20) || (vol_vccint_step < -90))
    {
        return -1;
    }

    if (vol_vccint_step < 0)
    {
        vol_vccint_step_cvt = 256 + vol_vccint_step;
    }
    else
    {
        vol_vccint_step_cvt = vol_vccint_step;
    }
    // memset(vol_step_cvt, 0, 4);
    // sprintf(vol_step_cvt, "%d", vol_step);
   // printf("set vol_vccint_step = 0x%x, vol_vccint_step_cvt = 0x%x\n", vol_vccint_step, vol_vccint_step_cvt);

    if ((vol_hbm_step > 7) || (vol_hbm_step < -53))
    {
        return -1;
    }

    if (vol_hbm_step < 0)
    {
        vol_hbm_step_cvt = 256 + vol_hbm_step;
    }
    else
    {
        vol_hbm_step_cvt = vol_hbm_step;
    }
    // memset(vol_step_cvt, 0, 4);
    // sprintf(vol_step_cvt, "%d", vol_step);
   // printf("set vol_hbm_step = 0x%x, vol_hbm_step_cvt = 0x%x\n", vol_hbm_step, vol_hbm_step_cvt);

    // open uio
    i2c_locker.lock();
    fd = open(uiod_iic, O_RDWR);

    if (fd < 1)
    {
        perror("Cannot open IIC \n");
        i2c_locker.unlock();
        return 0;
    }

    // mmap
    ip_ptr = mmap(NULL, MAP_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);

    if (ip_ptr == MAP_FAILED)
    {
        i2c_locker.unlock();
        perror("mmap IIC failed \n");
        return 0;
    }

    u8 sendBuf[3];
    XIic_DynInit(ip_ptr);

    // slaveAddr = 0x7C;
    slaveAddr = i2c_addr;
   // printf("set_voltages - slaveAddr = 0x%x\n", slaveAddr);
    // change voltage in tracking mode
    // increase volt1 +20mV: increase 4 step: 0x04
    // decrease volt2 -20mV: decrease -4 step: 0xFC

    // turn to page 2
    sendBuf[0] = 0x00;
    sendBuf[1] = 0x02;
    XIic_Send(ip_ptr, slaveAddr, sendBuf, 2, XIIC_STOP, 10000);

    usleep(500000);

    // write to VOUT_OFFSET at addr 0x1E
    sendBuf[0] = 0x1E;
    sendBuf[1] = vol_vccint_step_cvt;
    sendBuf[2] = vol_hbm_step_cvt;

    XIic_Send(ip_ptr, slaveAddr, sendBuf, 3, XIIC_STOP, 10000);
    // sleep(1);

    munmap(ip_ptr, MAP_SIZE);
    close(fd);
    i2c_locker.unlock();
    return 0;
}

static int read_Current(uint8_t i2c_addr, char *uiod_iic, int *curent_vccint, int *current_hbm)
{
    i2c_locker.lock();
    void *ip_ptr;
    int fd;
    u8 slaveAddr;
    u8 Buffer[2];

    // open uio
    fd = open(uiod_iic, O_RDWR);

    if (fd < 1)
    {
        perror("Cannot open IIC \n");
        i2c_locker.unlock();
        return 0;
    }

    // mmap
    ip_ptr = mmap(NULL, MAP_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);

    if (ip_ptr == MAP_FAILED)
    {

        perror("mmap IIC failed \n");
        i2c_locker.unlock();
        return 0;
    }

    u8 sendBuf[3];
    XIic_DynInit(ip_ptr);

    // slaveAddr = 0x7C;
    slaveAddr = i2c_addr;
    //printf("read_voltages - slaveAddr = 0x%x\n", slaveAddr);

    sendBuf[0] = 0x00;
    sendBuf[1] = 0x00;

    // float volt1, volt2;
    // turn to page 0
    if (XIic_Send(ip_ptr, slaveAddr, sendBuf, 2, XIIC_STOP, 10000) > 0)
    {
        sendBuf[0] = 0x8c;
        // write reg 8bh
        // read from reg 8Bh
        for (int i = 0; i < 1; ++i)
        {
            XIic_Send(ip_ptr, slaveAddr, sendBuf, 1, XIIC_REPEATED_START, 10000);
            XIic_Recv(ip_ptr, slaveAddr, Buffer, 2, XIIC_STOP, 10000);
            // sleep(1);

            // printf("buffer[0] = 0x%x\n", Buffer[0]);
            // printf("buffer[1] = 0x%x\n", Buffer[1]);

            int rd_val = (Buffer[1] << 8 | Buffer[0]);
            printf("current page 0 = %d APM \n", rd_val);
                 *curent_vccint = rd_val;
            
        }

        // turn to page 1
        sendBuf[0] = 0x00;
        sendBuf[1] = 0x01;
        XIic_Send(ip_ptr, slaveAddr, sendBuf, 2, XIIC_STOP, 10000);

        sendBuf[0] = 0x8c;
        // write reg 8bh
        // read from reg 8Bh
        for (int i = 0; i < 1; ++i)
        {
            XIic_Send(ip_ptr, slaveAddr, sendBuf, 1, XIIC_REPEATED_START, 10000);
            XIic_Recv(ip_ptr, slaveAddr, Buffer, 2, XIIC_STOP, 10000);
            // sleep(1);

            int rd_val = (Buffer[1] << 8 | Buffer[0]) ;
              printf("current page 1 = %d AMP \n", rd_val);
         
                *current_hbm = rd_val;
             
        }
    }
    munmap(ip_ptr, MAP_SIZE);
    close(fd);
    i2c_locker.unlock();
    return 0;
}

static int read_wattage(uint8_t i2c_addr, char *uiod_iic, int *curent_vccint, int *current_hbm)
{
    i2c_locker.lock();
    void *ip_ptr;
    int fd;
    u8 slaveAddr;
    u8 Buffer[2];

    // open uio
    fd = open(uiod_iic, O_RDWR);

    if (fd < 1)
    {
        perror("Cannot open IIC \n");
        i2c_locker.unlock();
        return 0;
    }

    // mmap
    ip_ptr = mmap(NULL, MAP_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);

    if (ip_ptr == MAP_FAILED)
    {

        perror("mmap IIC failed \n");
        i2c_locker.unlock();
        return 0;
    }

    u8 sendBuf[3];
    XIic_DynInit(ip_ptr);

    // slaveAddr = 0x7C;
    slaveAddr = i2c_addr;
    //printf("read_voltages - slaveAddr = 0x%x\n", slaveAddr);

    sendBuf[0] = 0x00;
    sendBuf[1] = 0x00;

    // float volt1, volt2;
    // turn to page 0
    if (XIic_Send(ip_ptr, slaveAddr, sendBuf, 2, XIIC_STOP, 10000) > 0)
    {
        sendBuf[0] = 0x96;
        // write reg 8bh
        // read from reg 8Bh
        for (int i = 0; i < 1; ++i)
        {
            XIic_Send(ip_ptr, slaveAddr, sendBuf, 1, XIIC_REPEATED_START, 10000);
            XIic_Recv(ip_ptr, slaveAddr, Buffer, 2, XIIC_STOP, 10000);
            // sleep(1);

            // printf("buffer[0] = 0x%x\n", Buffer[0]);
            // printf("buffer[1] = 0x%x\n", Buffer[1]);

            int rd_val = (Buffer[1] << 8 | Buffer[0]);
            printf("wattage page 0 = %d W \n", rd_val);
            
            
        }

        // turn to page 1
        sendBuf[0] = 0x00;
        sendBuf[1] = 0x01;
        XIic_Send(ip_ptr, slaveAddr, sendBuf, 2, XIIC_STOP, 10000);

        sendBuf[0] = 0x96;
        // write reg 8bh
        // read from reg 8Bh
        for (int i = 0; i < 1; ++i)
        {
            XIic_Send(ip_ptr, slaveAddr, sendBuf, 1, XIIC_REPEATED_START, 10000);
            XIic_Recv(ip_ptr, slaveAddr, Buffer, 2, XIIC_STOP, 10000);
            // sleep(1);

            int rd_val = (Buffer[1] << 8 | Buffer[0]) ;
              printf("wattage page 1 = %d W \n", rd_val);
         
              
             
        }
    }
    munmap(ip_ptr, MAP_SIZE);
    close(fd);
    i2c_locker.unlock();
    return 0;
}

static int read_voltages(uint8_t i2c_addr, char *uiod_iic, int *voltage_vccint, int *voltage_hbm)
{
    i2c_locker.lock();
    void *ip_ptr;
    int fd;
    u8 slaveAddr;
    u8 Buffer[2];


    // open uio
    fd = open(uiod_iic, O_RDWR);

    if (fd < 1)
    {
        perror("Cannot open IIC \n");
        i2c_locker.unlock();
        return 0;
    }

    // mmap
    ip_ptr = mmap(NULL, MAP_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);

    if (ip_ptr == MAP_FAILED)
    {

        perror("mmap IIC failed \n");
        i2c_locker.unlock();
        return 0;
    }

    u8 sendBuf[3];
    XIic_DynInit(ip_ptr);

    // slaveAddr = 0x7C;
    slaveAddr = i2c_addr;
    //printf("read_voltages - slaveAddr = 0x%x\n", slaveAddr);

    sendBuf[0] = 0x00;
    sendBuf[1] = 0x00;

    // float volt1, volt2;
    // turn to page 0
    if (XIic_Send(ip_ptr, slaveAddr, sendBuf, 2, XIIC_STOP, 10000) > 0)
    {
        sendBuf[0] = 0x8B;
        // write reg 8bh
        // read from reg 8Bh
        for (int i = 0; i < 1; ++i)
        {
            XIic_Send(ip_ptr, slaveAddr, sendBuf, 1, XIIC_REPEATED_START, 10000);
            XIic_Recv(ip_ptr, slaveAddr, Buffer, 2, XIIC_STOP, 10000);
            // sleep(1);

            // printf("buffer[0] = 0x%x\n", Buffer[0]);
            // printf("buffer[1] = 0x%x\n", Buffer[1]);

            int rd_val = (Buffer[1] << 8 | Buffer[0]) & 0x0FFF;
            // printf("volt page 0 = %d mV \n", rd_val);
            if((rd_val >= 400) && (rd_val <= 950)){
                *voltage_vccint = rd_val;
            }
        }

        // turn to page 1
        sendBuf[0] = 0x00;
        sendBuf[1] = 0x01;
        XIic_Send(ip_ptr, slaveAddr, sendBuf, 2, XIIC_STOP, 10000);

        sendBuf[0] = 0x8B;
        // write reg 8bh
        // read from reg 8Bh
        for (int i = 0; i < 1; ++i)
        {
            XIic_Send(ip_ptr, slaveAddr, sendBuf, 1, XIIC_REPEATED_START, 10000);
            XIic_Recv(ip_ptr, slaveAddr, Buffer, 2, XIIC_STOP, 10000);
            // sleep(1);

            int rd_val = (Buffer[1] << 8 | Buffer[0]) & 0x0FFF;
          // printf("volt page 1 = %d mV \n", rd_val);
            if ((rd_val >= 1000) && (rd_val <= 1300))
            {
                *voltage_hbm = rd_val;
            }
        }
    }
    munmap(ip_ptr, MAP_SIZE);
    close(fd);
    i2c_locker.unlock();
    return 0;
}



static int reset_i2c_funtion(uint8_t i2c_addr, char *uiod_iic)
{
    // const char *uiod_iic = "/dev/uio5";
    uart_locker.lock();
    void *ip_ptr;
    int fd;

    u8 slaveAddr = i2c_addr;

    fd = open(uiod_iic, O_RDWR);

    if (fd < 1)
    {
        perror("Cannot open IIC \n");
        uart_locker.unlock();
        return 0;
    }

    // mmap
    ip_ptr = mmap(NULL, MAP_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    XIic_DynInit(ip_ptr);

    if (ip_ptr == MAP_FAILED)
    {
        perror("mmap IIC failed \n");
        uart_locker.unlock();
        return 0;
    }

    u8 sendBuf[3];

    sendBuf[0] = 0xFC;
    sendBuf[1] = 0x01;

    // slaveAddr = 0x4E;
    slaveAddr = i2c_addr;
   // XIic_Send(ip_ptr, slaveAddr, sendBuf, 2, XIIC_STOP);
    XIic_Send(ip_ptr, slaveAddr, sendBuf, 2, XIIC_STOP, 10000);

   
    munmap(ip_ptr, MAP_SIZE);
    close(fd);
    uart_locker.unlock();

    printf("resest IC %s \n", uiod_iic);
    return 1;
}
static int read_temps(uint8_t i2c_addr, int *fan_level, int *board_temp, int *chip_temp, char *uiod_iic, int *check_connection)
{
    // const char *uiod_iic = "/dev/uio5";

    uart_locker.lock();

    void *ip_ptr;
    int fd;
    int read_oke = 0;

    unsigned int rd_val;

    int m_board_temp = -1;
    int m_chip_temp = -1;

    u8 Buffer[2];
    u8 slaveAddr;

    // printf("UIO test IIC \n");

    // open uio
    fd = open(uiod_iic, O_RDWR);

    if (fd < 1)
    {
        perror("Cannot open IIC \n");
        uart_locker.unlock();
        return 0;
    }

    // mmap
    ip_ptr = mmap(NULL, MAP_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    XIic_DynInit(ip_ptr);

    if (ip_ptr == MAP_FAILED)
    {

        perror("mmap IIC failed \n");
        uart_locker.unlock();
        return 0;
    }

    u8 sendBuf[3];

    // slaveAddr = 0x4E;
    slaveAddr = i2c_addr;
    //printf("read_temps - slaveAddr = 0x%x\n", slaveAddr);

    // read board temp

    sendBuf[0] = 0x00;
    if (XIic_Send(ip_ptr, slaveAddr, sendBuf, 1, XIIC_STOP, 1000) > 0)
    {
        // sleep(5);
        // printf("STATUS = 0x%x\n", (u8) XIic_ReadReg(ip_ptr, 0x104));

        for (int i = 0; i < 1; ++i)
        {
            memset(Buffer, 0, 2);
            if (XIic_Recv(ip_ptr, slaveAddr, Buffer, 1, XIIC_STOP, 1000) > 0)
            {
                usleep(1000);
                if (Buffer[0] > MIN_TEMP_READABLE && Buffer[0] < 150)
                {
                    read_oke = 1;
                    break;
                }
            }
        }
        if (read_oke == 1)
        {
          //  printf("debug 1\n");
            // *board_temp = Buffer[0];
            m_board_temp = Buffer[0];
            *check_connection = 1;
        }
        else
        {
            *check_connection = 0;
            // printf("PLEASE CHECK CONNECTION OF FPGA \n");
        }
        read_oke = 0;

        // change to read chip temp
        sendBuf[0] = 0x01;
        XIic_Send(ip_ptr, slaveAddr, sendBuf, 1, XIIC_STOP, 1000);
        usleep(1000);
        for (int i = 0; i < 1; ++i)
        {
            memset(Buffer, 0, 2);
            if (XIic_Recv(ip_ptr, slaveAddr, Buffer, 1, XIIC_STOP, 1000) > 0)
            {
                usleep(1000);
                if (Buffer[0] > MIN_TEMP_READABLE && Buffer[0] < 150)
                {
                    read_oke = 1;
                    break;
                }
            }
        }

        if (read_oke == 1)
        {
          //  printf("debug 2\n");
            // *chip_temp = Buffer[0];
            *check_connection = 1;
            m_chip_temp = Buffer[0];
        }
        else
        {
            *check_connection = 0;
        }

        if ((m_board_temp > MIN_TEMP_READABLE) && (m_chip_temp > MIN_TEMP_READABLE))
        {
            *chip_temp = m_chip_temp;
            *board_temp = m_board_temp;
            if (*chip_temp >= temp_device[TEMP_LEVEL_10])
            {
                *fan_level = FAN_LEVEL_10;
            }
            else if (*chip_temp >= temp_device[TEMP_LEVEL_9])
            {
                *fan_level = FAN_LEVEL_9;
            }
            else if (*chip_temp >= temp_device[TEMP_LEVEL_8])
            {
                *fan_level = FAN_LEVEL_8;
            }
            else if (*chip_temp >= temp_device[TEMP_LEVEL_7])
            {
                *fan_level = FAN_LEVEL_7;
            }
            else if (*chip_temp >= temp_device[TEMP_LEVEL_6])
            {
                *fan_level = FAN_LEVEL_6;
            }
            else if (*chip_temp >= temp_device[TEMP_LEVEL_5])
            {
                *fan_level = FAN_LEVEL_5;
            }

            else if (*chip_temp >= temp_device[TEMP_LEVEL_4])
            {
                *fan_level = FAN_LEVEL_4;
            }
            else if (*chip_temp >= temp_device[TEMP_LEVEL_3])
            {
                *fan_level = FAN_LEVEL_3;
            }

            else if (*chip_temp >= temp_device[TEMP_LEVEL_2])
            {
                *fan_level = FAN_LEVEL_2;
            }

            else 
            {
                *fan_level = FAN_LEVEL_1;
            }
        }
        else
        {
            munmap(ip_ptr, MAP_SIZE);
            *check_connection = 0;
            close(fd);
            uart_locker.unlock();
            return 0;
        }
    }
    else
    {
        munmap(ip_ptr, MAP_SIZE);
        *check_connection = 0;
        close(fd);
        uart_locker.unlock();
        return 0;
    }

    munmap(ip_ptr, MAP_SIZE);
    close(fd);
    uart_locker.unlock();
    return 1;
}

static int fans_speed_control(int fanLevel)
{

    void *ip_ptr;
    int fd;

    unsigned int rd_val;

    // printf("mmap fan\n");
    fd = open(FAN_UIO_PATH, O_RDWR);

    if (fd < 1)
    {
        perror("Cannot mmap fan \n");
    }

    // mmap
    ip_ptr = mmap(NULL, MAP_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);

    if (ip_ptr == MAP_FAILED)
    {
        /* code */
        perror("mmap fan failed \n");
        return -1;
    }

    // write down to duty cycle 20%
    *((unsigned *)(((u_int8_t *)ip_ptr) + 0x04)) = fanLevel;

    munmap(ip_ptr, MAP_SIZE);
    close(fd);
    return 0;
}

static int fans__read_speed_control(int address)
{

    void *ip_ptr;
    int fd;

    unsigned int rd_val;

    // printf("mmap fan\n");
    fd = open(FAN_UIO_PATH, O_RDWR);

    if (fd < 1)
    {
        perror("Cannot mmap fan \n");
    }

    // mmap
    ip_ptr = mmap(NULL, MAP_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);

    if (ip_ptr == MAP_FAILED)
    {
        /* code */
        perror("mmap fan failed \n");
        return -1;
    }

    // write down to duty cycle 20%
    rd_val = *((unsigned *)(((u_int8_t *)ip_ptr) + address));
    printf("=========== address: %d ; speed return fan1 : %d \n", address, rd_val);

    munmap(ip_ptr, MAP_SIZE);
    close(fd);
    return 0;
}

void show_fpga_info()
{
    for (int i = 0; i < NUMBER_FPGA; i++)
    {

        if (fpga[i].check_connection == 0)
        {
            fpga[i].power_on_fpga = 0;
        }
        printf("Index: %d ; board_temp : %d; chip_temp : %d ; path: %s ; is_plug : %d  is_power_on : %d ;  \n", i, fpga[i].boardTemp, fpga[i].chipTemp, fpga[i].path_uio, fpga[i].check_connection, fpga[i].power_on_fpga);
    }
}

int get_next_fan_level()
{

    int next_level;

    /// init fan level
    if (fpga[0].check_connection)
    {
        next_level = fpga[0].fanLevel;
    }
    else if (fpga[1].check_connection)
    {
        next_level = fpga[1].fanLevel;
    }
    else if (fpga[2].check_connection)
    {
        next_level = fpga[2].fanLevel;
    }

    // get next fan level

    if ((fpga[0].fanLevel > next_level) && fpga[0].check_connection)
    {
        next_level = fpga[0].fanLevel;
    }

    if ((fpga[1].fanLevel > next_level) && fpga[1].check_connection)
    {
        next_level = fpga[1].fanLevel;
    }

    if ((fpga[2].fanLevel > next_level) && fpga[2].check_connection)
    {
        next_level = fpga[2].fanLevel;
    }

    return next_level;
}

/* API service */
void voltage_control(void)
{
    svr.Post("/controller/setVoltage", [&](const Request &req, Response &res, const ContentReader &content_reader)
             {
        std::string body;
        std::string body_res = "{\"result\":1}";
        res.set_content(body_res, "application/json");
        content_reader([&](const char *data, size_t data_length){
            body.append(data, data_length);
            printf("api_service::voltage_control - data = %s\n", body.c_str());

            std::string snapshot_uri;
            // parse data
            Document document;
            document.Parse(body.c_str());
            if (document.HasMember("voltage_vccint") && document.HasMember("voltage_hbm") && document.HasMember("boardId"))
            {
                int voltage_vccint = document["voltage_vccint"].GetDouble();
                int voltage_hbm = document["voltage_hbm"].GetDouble();
                int boardId = document["boardId"].GetDouble();

                if(boardId == NUMBER_FPGA){
                    for (size_t i = 0; i < NUMBER_FPGA; i++)
                    {
                        if (fpga[i].check_connection)
                        {
                            if(voltage_vccint == 0){
                                voltage_vccint = fpga[i].voltage_vccint;
                            }
                            if (voltage_hbm == 0)
                            {
                                voltage_hbm = fpga[i].voltage_hbm;
                            }
                            set_voltages(fpga[i].voltage_i2c_addr, fpga[i].path_uio, voltage_vccint, voltage_hbm);
                            CommonEngine::config_info m_config_info;
                            m_config.load(m_config_info);
                            m_config_info.voltages_vccint[i] = voltage_vccint;
                            m_config_info.voltages_hbm[i] = voltage_hbm;
                            m_config.save(m_config_info);
                        }
                    }
                }
                else if (boardId >= 0 && boardId < NUMBER_FPGA)
                {
                    if (voltage_vccint == 0)
                    {
                        voltage_vccint = fpga[boardId].voltage_vccint;
                    }
                    if (voltage_hbm == 0)
                    {
                        voltage_hbm = fpga[boardId].voltage_hbm;
                    }
                    set_voltages(fpga[boardId].voltage_i2c_addr, fpga[boardId].path_uio, voltage_vccint, voltage_hbm);
                    CommonEngine::config_info m_config_info;
                    m_config.load(m_config_info);
                    m_config_info.voltages_vccint[boardId] = voltage_vccint;
                    m_config_info.voltages_hbm[boardId] = voltage_hbm;
                    m_config.save(m_config_info);
                }
                // response
                body_res = "{\"result\":1}";
                return true;
            }
            return false;
        });
        printf("api_service::voltage_control - response = %s\n", body_res.c_str());
        res.set_content(body_res, "application/json"); 
        });
}

void get_voltage(void)
{
    svr.Get("/controller/getVoltage", [](const httplib::Request &, httplib::Response &res)
            {
        // response
        std::string body_res = "{\"result\":0}";
        StringBuffer jsonBuffer;
        Writer<StringBuffer> writer(jsonBuffer);

        // get temperature

        // response
        writer.StartObject();
        writer.Key("FPGA");
        writer.StartArray();
        uint8_t fpga_num = 3;
        for (size_t i = 0; i < NUMBER_FPGA; i++)
        {
            //read_temps(&fpga[i].fanLevel, &fpga[i].boardTemp, &fpga[i].chipTemp, fpga[i].path_uio, &fpga[i].check_connection);
            writer.StartObject();
            writer.Key("boardId");
            writer.Int(i);
            writer.Key("value");
            writer.Int(100 + i*10);
            writer.EndObject();
        }
        
        writer.EndArray();
        writer.EndObject();
        
        body_res = jsonBuffer.GetString();
        printf("api_service::getVoltage - response = %s\n", body_res.c_str());
        res.set_content(body_res, "application/json"); });
}

void get_temperature(void)
{
    svr.Get("/controller/getTemperature", [](const httplib::Request &, httplib::Response &res)
            {
        // response
        std::string body_res = "{\"result\":0}";
        StringBuffer jsonBuffer;
        Writer<StringBuffer> writer(jsonBuffer);

        // get temperature

        // response
        writer.StartObject();
        writer.Key("FPGA");
        writer.StartArray();
        uint8_t fpga_num = 3;
        for (size_t i = 0; i < NUMBER_FPGA; i++)
        {
            // read_temps(&fpga[i].fanLevel, &fpga[i].boardTemp, &fpga[i].chipTemp, fpga[i].path_uio, &fpga[i].check_connection);
            writer.StartObject();
            writer.Key("boardId");
            writer.Int(i);
            writer.Key("value");
            writer.Int(fpga[i].boardTemp);
            writer.EndObject();
        }
        
        writer.EndArray();
        writer.EndObject();
        
        body_res = jsonBuffer.GetString();
        printf("api_service::getTemperature - response = %s\n", body_res.c_str());
        res.set_content(body_res, "application/json"); });
}

void get_temperature_by_id(void)
{
    svr.Post("/controller/getTemperatureById", [&](const Request &req, Response &res, const ContentReader &content_reader)
             {
        std::string body;
        std::string body_res = "{\"result\":0}";
        content_reader([&](const char *data, size_t data_length){
            body.append(data, data_length);
            printf("api_service::voltage_control - data = %s\n", body.c_str());

            std::string snapshot_uri;
            // parse data
            Document document;
            document.Parse(body.c_str());
            if (document.HasMember("boardId"))
            {
                int id = document["boardId"].GetInt();
                if(id < NUMBER_FPGA){
                    // read_temps(&fpga[id].fanLevel, &fpga[id].boardTemp, &fpga[id].chipTemp, fpga[id].path_uio, &fpga[id].check_connection);

                    // response
                    StringBuffer jsonBuffer;
                    Writer<StringBuffer> writer(jsonBuffer);
                    writer.StartObject();
                    writer.Key("boardId");
                    writer.Int(id);
                    writer.Key("value");
                    writer.Int(fpga[id].boardTemp);
                    writer.EndObject();
                    body_res = jsonBuffer.GetString();
                    return true;
                }
            }
            return false;
        });
        printf("api_service::voltage_control - response = %s\n", body_res.c_str());
        res.set_content(body_res, "application/json"); });
}

void get_all_info(void)
{
    svr.Get("/controller/getAllInfo", [](const httplib::Request &, httplib::Response &res)
            {
        // response
        std::string body_res = "{\"result\":0}";
        StringBuffer jsonBuffer;
        Writer<StringBuffer> writer(jsonBuffer);

        // get temperature
        for (int i = 0; i < NUMBER_FPGA; i++)
        {

            // read FPGA temps information
            printf("****************************:FPGA_%d********* ", i);
            for (size_t j = 0; j < fpga[i].temps_i2c_addr.size(); j++)
            {
                if (!read_temps(fpga[i].temps_i2c_addr[j], &fpga[i].fanLevel, &fpga[i].boardTemp, &fpga[i].chipTemp, fpga[i].path_uio, &fpga[i].check_connection))
                {
                    //printf("READ temp FAIL\n");
                    usleep(1000);
                    // continue;
                }
                else{
                    break;
                }
            }
            if (!read_voltages(fpga[i].voltage_i2c_addr, fpga[i].path_uio, &fpga[i].voltage_vccint, &fpga[i].voltage_hbm))
            {
                //printf("READ voltage FAIL\n");
                usleep(1000);
            }



        }
        // response
        writer.StartObject();
        writer.Key("FPGA");
        writer.StartArray();
        uint8_t fpga_num = 3;
        for (size_t i = 0; i < NUMBER_FPGA; i++)
        {
            writer.StartObject();
            writer.Key("boardId");
            writer.Int(i);
            writer.Key("status");
            writer.Int(fpga[i].power_on_fpga);
            writer.Key("chipTemp");
            writer.Int(fpga[i].chipTemp);
            writer.Key("boardTemp");
            writer.Int(fpga[i].boardTemp);
            writer.Key("voltage_vccint");
            writer.Int(fpga[i].voltage_vccint);
            writer.Key("voltage_hbm");
            writer.Int(fpga[i].voltage_hbm);
            writer.Key("fanLevel");
            writer.Int(fpga[i].fanLevel);
            writer.Key("tempMax");
            writer.Int(fpga[i].temp_max);
            writer.Key("chipType");
            writer.String(fpga[i].chip_type.c_str());
            writer.Key("temp_i2c_addr");
            writer.String("0x4D");
            writer.Key("voltage_i2c_addr");
            writer.String(UtilityFunc::hexToString(fpga[i].voltage_i2c_addr).c_str());
            writer.EndObject();
        }
        
        writer.EndArray();
        writer.Key("curentFanLevel");
        writer.Int(current_fan_level);
        writer.EndObject();
        
        body_res = jsonBuffer.GetString();
        printf("api_service::getAllInfo - response = %s\n", body_res.c_str());
        res.set_content(body_res, "application/json"); });
}

void reboot_board(void)
{
    svr.Get("/controller/reboot", [](const httplib::Request &, httplib::Response &res)
            {
        // response
        std::string body_res = "{\"result\":0}";
        StringBuffer jsonBuffer;
        Writer<StringBuffer> writer(jsonBuffer);

        std::string reboot_cmd = "sudo reboot";
        system(reboot_cmd.c_str());

        // response
        printf("api_service::reboot_board - response = %s\n", body_res.c_str());
        res.set_content(body_res, "application/json"); });
}

void reset_factory_fpga(void)
{
    svr.Get("/controller/resetFactoryFpga", [](const httplib::Request &, httplib::Response &res)
            {
        // response
        std::string body_res = "{\"result\":1}";
        res.set_content(body_res, "application/json");
        StringBuffer jsonBuffer;
        Writer<StringBuffer> writer(jsonBuffer);

        std::cout << "reset factory FPGA" << std::endl;
        for (size_t i = 0; i < NUMBER_FPGA; i++)
        {
            if (fpga[i].check_connection)
            {
                int voltage_vccint_default = 850;
                int voltage_hbm_default = 1265;
                set_voltages(fpga[i].voltage_i2c_addr, fpga[i].path_uio, voltage_vccint_default, voltage_hbm_default);
            }
        }

        std::cout << "reset factory all configuration" << std::endl;
        m_config.reset_factory();

        // response
        printf("api_service::reset_factory_fpga - response = %s\n", body_res.c_str());
        exit(1);
        // res.set_content(body_res, "application/json"); 
        });
}

void set_chips_type(void)
{
    svr.Post("/controller/setChipType", [&](const Request &req, Response &res, const ContentReader &content_reader)
             {
        std::string body;
        std::string body_res = "{\"result\":1}";
        res.set_content(body_res, "application/json");
        content_reader([&](const char *data, size_t data_length){
            body.append(data, data_length);
            printf("api_service::set_chips_type - data = %s\n", body.c_str());

            std::string snapshot_uri;
            bool result = true;
            // parse data
            Document document;
            document.Parse(body.c_str());
            if (document.HasMember("chipType") && document.HasMember("boardId"))
            {
                std::string chipType = document["chipType"].GetString();
                int boardId = document["boardId"].GetDouble();

                if(boardId == NUMBER_FPGA){
                    for (size_t i = 0; i < NUMBER_FPGA; i++)
                    {
                        fpga[i].chip_type = chipType;
                        CommonEngine::config_info m_config_info;
                        m_config.load(m_config_info);
                        m_config_info.chips_type[i] = chipType;
                        m_config.save(m_config_info);
                    }
                }
                else if(boardId >= 0 && boardId < NUMBER_FPGA){
                    fpga[boardId].chip_type = chipType;
                    CommonEngine::config_info m_config_info;
                    m_config.load(m_config_info);
                    m_config_info.chips_type[boardId] = chipType;
                    m_config.save(m_config_info);
                }
                if(result == true){
                    // response
                    body_res = "{\"result\":1}";
                }
                return true;
            }
            return false;
        });
        printf("api_service::set_chips_type - response = %s\n", body_res.c_str());
        res.set_content(body_res, "application/json"); 
    });
}

void set_temp_max(void)
{
    svr.Post("/controller/setTempMax", [&](const Request &req, Response &res, const ContentReader &content_reader)
             {
        std::string body;
        std::string body_res = "{\"result\":1}";
        res.set_content(body_res, "application/json");
        content_reader([&](const char *data, size_t data_length){
            body.append(data, data_length);
            printf("api_service::set_temp_max - data = %s\n", body.c_str());

            std::string snapshot_uri;
            bool result = true;
            // parse data
            Document document;
            document.Parse(body.c_str());
            if (document.HasMember("tempMax") && document.HasMember("boardId"))
            {
                int tempMax = document["tempMax"].GetDouble();
                int boardId = document["boardId"].GetDouble();

                if(boardId == NUMBER_FPGA){
                    for (size_t i = 0; i < NUMBER_FPGA; i++)
                    {
                        if(tempMax <= TEMP_MAX_SAFETY){
                            fpga[i].temp_max = tempMax;
                            CommonEngine::config_info m_config_info;
                            m_config.load(m_config_info);
                            m_config_info.temps_max[i] = tempMax;
                            m_config.save(m_config_info);
                        }
                        else{
                            result = false;
                        }
                    }
                }
                else if(boardId >= 0 && boardId < NUMBER_FPGA){
                    if (tempMax <= TEMP_MAX_SAFETY)
                    {
                        fpga[boardId].temp_max = tempMax;
                        CommonEngine::config_info m_config_info;
                        m_config.load(m_config_info);
                        m_config_info.temps_max[boardId] = tempMax;
                        m_config.save(m_config_info);
                    }
                    else
                    {
                        result = false;
                    }
                }
                if(result == true){
                    // response
                    body_res = "{\"result\":1}";
                }
                return true;
            }
            return false;
        });
        printf("api_service::set_temp_max - response = %s\n", body_res.c_str());
        res.set_content(body_res, "application/json"); 
    });
}

// void set_i2c_address(void)
// {
//     svr.Post("/controller/setI2CAddress", [&](const Request &req, Response &res, const ContentReader &content_reader)
//              {
//         std::string body;
//         std::string body_res = "{\"result\":1}";
//         res.set_content(body_res, "application/json");
//         content_reader([&](const char *data, size_t data_length){
//             body.append(data, data_length);
//             printf("api_service::set_i2c_address - data = %s\n", body.c_str());

//             std::string snapshot_uri;
//             bool result = true;
//             // parse data
//             Document document;
//             document.Parse(body.c_str());
//             if (document.HasMember("boardId"))
//             {
//                 int boardId = document["boardId"].GetDouble();
//                 if(document.HasMember("temp_i2c_addr")){
//                     std::string temp_i2c_addr = document["temp_i2c_addr"].GetString();
//                     int i2c_addr_value = UtilityFunc::stringToInt(temp_i2c_addr);
//                     std::cout << "api_service::set_i2c_address - cvt from " << temp_i2c_addr << " to " << i2c_addr_value << std::endl;
//                     // save
//                     CommonEngine::config_info m_config_info;
//                     m_config.load(m_config_info);
//                     m_config_info.temp_i2c_addr = temp_i2c_addr;
//                     m_config.save(m_config_info);
//                     if (boardId == NUMBER_FPGA)
//                     {
//                         for (size_t i = 0; i < NUMBER_FPGA; i++)
//                         {
                            
//                             fpga[i].temp_i2c_addr = i2c_addr_value;
//                         }
//                     }
//                     else if (boardId >= 0 && boardId < NUMBER_FPGA)
//                     {
//                         fpga[boardId].temp_i2c_addr = i2c_addr_value;
//                     }
//                 }
//                 if (document.HasMember("voltage_i2c_addr"))
//                 {
//                     std::string voltage_i2c_addr = document["voltage_i2c_addr"].GetString();
//                     int i2c_addr_value = UtilityFunc::stringToInt(voltage_i2c_addr);
//                     std::cout << "api_service::set_i2c_address - cvt from " << voltage_i2c_addr << " to " << i2c_addr_value << std::endl;
//                     // save
//                     CommonEngine::config_info m_config_info;
//                     m_config.load(m_config_info);
//                     m_config_info.voltage_i2c_addr = voltage_i2c_addr;
//                     m_config.save(m_config_info);
//                     if (boardId == NUMBER_FPGA)
//                     {
//                         for (size_t i = 0; i < NUMBER_FPGA; i++)
//                         {

//                             fpga[i].voltage_i2c_addr = i2c_addr_value;
//                         }
//                     }
//                     else if (boardId >= 0 && boardId < NUMBER_FPGA)
//                     {
//                         fpga[boardId].voltage_i2c_addr = i2c_addr_value;
//                     }
//                 }
//                 // response
//                 body_res = "{\"result\":1}";
//                 return true;
//             }
//             return false;
//         });
//         printf("api_service::set_i2c_address - response = %s\n", body_res.c_str());
//         // res.set_content(body_res, "application/json");
//     });
// }

void get_all_fan_info(void)
{
    svr.Get("/controller/getAllFanInfo", [](const httplib::Request &, httplib::Response &res)
            {
        // response
        std::string body_res = "{\"result\":0}";
        StringBuffer jsonBuffer;
        Writer<StringBuffer> writer(jsonBuffer);

        // response
        writer.StartObject();
        writer.Key("temp_device");
        writer.StartArray();
        uint8_t fpga_num = 3;
        for (size_t i = 0; i < TEMP_DEVICE_NUM; i++)
        {
            writer.StartObject();
            writer.Key("id");
            writer.Int(i);
            writer.Key("value");
            writer.Int(temp_device[i]);
            writer.EndObject();
        }
        
        writer.EndArray();

        writer.Key("fan_mode");
        writer.Int(fan_mode);
        writer.Key("fan_manual_level");
        writer.Int(fan_manual_level);
        writer.Key("current_fan_level");
        writer.Int(current_fan_level);

        writer.EndObject();
        
        body_res = jsonBuffer.GetString();
        printf("api_service::getAllInfo - response = %s\n", body_res.c_str());
        res.set_content(body_res, "application/json"); });
}

void set_fan_mode(void)
{
    svr.Post("/controller/setFansMode", [&](const Request &req, Response &res, const ContentReader &content_reader)
             {
        std::string body;
        std::string body_res = "{\"result\":0}";
        res.set_content(body_res, "application/json");
        content_reader([&](const char *data, size_t data_length){
            body.append(data, data_length);
            printf("api_service::set_fan_mode - data = %s\n", body.c_str());

            std::string snapshot_uri;
            // parse data
            Document document;
            document.Parse(body.c_str());
            if (document.HasMember("fan_mode") && document.HasMember("fan_level"))
            {
                int m_fan_mode = document["fan_mode"].GetInt();
                int m_fan_level = document["fan_level"].GetInt();

                // update global
                fan_mode = m_fan_mode;
                if(m_fan_mode == 1){ // manual
                    fan_manual_level = m_fan_level;
                    fans_speed_control(fan_manual_level);
                    current_fan_level = fan_manual_level;

                }

                // save config
                CommonEngine::config_info m_config_info;
                m_config.load(m_config_info);
                m_config_info.fan_mode = fan_mode;
                m_config_info.fan_manual_level = fan_manual_level;
                m_config.save(m_config_info);

                body_res = "{\"result\":1}";
                return true;
            }
            return false;
        });
        printf("api_service::set_fan_mode - response = %s\n", body_res.c_str());
        res.set_content(body_res, "application/json"); });
}

void set_temp_device(void)
{
    svr.Post("/controller/setTempDevice", [&](const Request &req, Response &res, const ContentReader &content_reader)
             {
        std::string body;
        std::string body_res = "{\"result\":0}";
        res.set_content(body_res, "application/json");
        content_reader([&](const char *data, size_t data_length){
            body.append(data, data_length);
            printf("api_service::set_temp_device - data = %s\n", body.c_str());

            std::string snapshot_uri;
            // parse data
            Document document;
            document.Parse(body.c_str());
            if (document.HasMember("temp_device"))
            {
                const Value &m_temp_device = document["temp_device"];
                if (m_temp_device.IsArray())
                {
                    if (m_temp_device.Size() <= TEMP_DEVICE_NUM)
                    {
                        for (size_t i = 0; i < m_temp_device.Size(); i++)
                        {
                            const Value &obj_value = m_temp_device[i];
                            if (obj_value.HasMember("id") && obj_value.HasMember("value"))
                            {
                                uint8_t id = obj_value["id"].GetInt();
                                int value = obj_value["value"].GetInt();

                                // update global
                                if(id <= TEMP_DEVICE_NUM){
                                    temp_device[id] = value;
                                }
                            }
                        }
                    }
                }

                // save config
                CommonEngine::config_info m_config_info;
                m_config.load(m_config_info);
                for (size_t i = 0; i < TEMP_DEVICE_NUM; i++)
                {
                    m_config_info.temp_device[i] = temp_device[i];
                }
                
                m_config.save(m_config_info);

                body_res = "{\"result\":1}";
                return true;
            }
            return false;
        });
        printf("api_service::set_temp_device - response = %s\n", body_res.c_str());
        res.set_content(body_res, "application/json"); });
}

// email

void get_email_config(void)
{
    svr.Get("/controller/getEmailConfig", [](const httplib::Request &, httplib::Response &res)
            {
        // response
        std::string body_res = "{\"result\":0}";
        StringBuffer jsonBuffer;
        Writer<StringBuffer> writer(jsonBuffer);

        // get temperature

        // response
        writer.StartObject();
        writer.Key("enable_noti");
        writer.String(std::to_string(enable_noti).c_str());
        writer.Key("desti_email_noti");
        writer.String(desti_email_noti.c_str());
        writer.Key("chip_temp_noti");
        writer.String(std::to_string(chip_temp_noti).c_str());
        writer.Key("frequency_noti");
        writer.String(std::to_string(frequency_noti).c_str());
        writer.Key("enable_hash_noti");
        writer.String(std::to_string(enable_hash_noti).c_str());
        writer.Key("enable_fan_noti");
        writer.String(std::to_string(enable_fan_noti).c_str());
        writer.Key("over_heat_temp_noti");
        writer.String(std::to_string(over_heat_temp_noti).c_str());
        writer.Key("enable_miner_status_noti");
        writer.String(std::to_string(enable_miner_status_noti).c_str());
        writer.EndObject();
        
        body_res = jsonBuffer.GetString();
        printf("api_service::get_email_config - response = %s\n", body_res.c_str());
        res.set_content(body_res, "application/json"); });
}

void set_email_config(void)
{
    svr.Post("/controller/setEmailConfig", [&](const Request &req, Response &res, const ContentReader &content_reader)
             {
        std::string body;
        std::string body_res = "{\"result\":0}";
        res.set_content(body_res, "application/json");
        content_reader([&](const char *data, size_t data_length){
            body.append(data, data_length);
            printf("api_service::set_email_config - data = %s\n", body.c_str());

            std::string snapshot_uri;
            // parse data
            Document document;
            ParseResult parse_result = document.Parse(body.c_str());
            if(parse_result){
                if(document.HasMember("enable_noti")){
                    std::string m_enable_noti = document["enable_noti"].GetString();
                    enable_noti = std::stoi(m_enable_noti);
                }
                if (document.HasMember("desti_email_noti"))
                {
                    std::string m_desti_email_noti = document["desti_email_noti"].GetString();
                    desti_email_noti = m_desti_email_noti;
                }
                if (document.HasMember("chip_temp_noti"))
                {
                    std::string m_chip_temp_noti = document["chip_temp_noti"].GetString();
                    chip_temp_noti = std::stoi(m_chip_temp_noti);
                }
                if (document.HasMember("frequency_noti"))
                {
                    std::string m_frequency_noti = document["frequency_noti"].GetString();
                    frequency_noti = std::stoi(m_frequency_noti);
                }
                if (document.HasMember("enable_hash_noti"))
                {
                    std::string m_enable_hash_noti = document["enable_hash_noti"].GetString();
                    enable_hash_noti = std::stoi(m_enable_hash_noti);
                }
                if (document.HasMember("enable_fan_noti"))
                {
                    std::string m_enable_fan_noti = document["enable_fan_noti"].GetString();
                    enable_fan_noti = std::stoi(m_enable_fan_noti);
                }
                if (document.HasMember("over_heat_temp_noti"))
                {
                    std::string m_over_heat_temp_noti = document["over_heat_temp_noti"].GetString();
                    over_heat_temp_noti = std::stoi(m_over_heat_temp_noti);
                }
                if (document.HasMember("enable_miner_status_noti"))
                {
                    std::string m_enable_miner_status_noti = document["enable_miner_status_noti"].GetString();
                    enable_miner_status_noti = std::stoi(m_enable_miner_status_noti);
                }

                // save config
                CommonEngine::config_info m_config_info;
                m_config.load(m_config_info);
                
                m_config_info.enable_noti = std::to_string(enable_noti);
                m_config_info.desti_email_noti = desti_email_noti;
                m_config_info.chip_temp_noti = std::to_string(chip_temp_noti);
                m_config_info.frequency_noti = std::to_string(frequency_noti);
                m_config_info.enable_hash_noti = std::to_string(enable_hash_noti);
                m_config_info.enable_fan_noti = std::to_string(enable_fan_noti);
                m_config_info.over_heat_temp_noti = std::to_string(over_heat_temp_noti);
                m_config_info.enable_miner_status_noti = std::to_string(enable_miner_status_noti);

                m_config.save(m_config_info);

                body_res = "{\"result\":1}";
                return true;
            }
            
            return false;
        });
        printf("api_service::set_email_config - response = %s\n", body_res.c_str());
        res.set_content(body_res, "application/json"); });
}

void api_service(void)
{
    voltage_control();
    get_voltage();
    get_temperature();
    get_temperature_by_id();
    get_all_info();
    reboot_board();
    set_chips_type();
    set_temp_max();
    // set_i2c_address();
    reset_factory_fpga();
    get_all_fan_info();
    set_fan_mode();
    set_temp_device();
    get_email_config();
    set_email_config();

    svr.listen("0.0.0.0", 8200);
}

int return_levl(int fan_lv)
{
    if (fan_lv == 1 || fan_lv == 2)
        return fan_lv;
    else if (fan_lv == 5)
        return 3;
    else if (fan_lv == 7)
        return 4;
    else if (fan_lv == 8)
        return 5;
    else
        return 6;
}

int handle_pl_ping(void)
{
    while (1)
    {
        const char *uiod_ping = "/dev/uio12";

        void *ip_ptr;
        int fd;

        printf("mmap ping\n");
        fd = open(uiod_ping, O_RDWR);

        if (fd < 1)
        {
            perror("Cannot mmap ping \n");
        }

        // mmap
        ip_ptr = mmap(NULL, MAP_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);

        if (ip_ptr == MAP_FAILED)
        {

            perror("mmap ping failed \n");
            exit(1);
        }

        // write number of seconds

        // writeReg(ip_ptr, 0, 10); // period = 10 seconds

        while (1)
        {
            //printf("send valid signal... \n");
            writeReg(ip_ptr, 4, 0); // send ping signal
            sleep(1);
        }

        // the board should shutoff here
        munmap(ip_ptr, MAP_SIZE);

        close(fd);
    }
}

std::string exeCommand(const std::string &cmd)
{
    std::shared_ptr<FILE> pipe(popen(cmd.c_str(), "r"), pclose);
    if (!pipe)
        return "ERROR";
    char buffer[128];
    std::string result = "";
    while (!feof(pipe.get()))
    {
        if (fgets(buffer, 128, pipe.get()) != NULL)
            result += buffer;
    }
    return result;
}

static int
GPIORead(int pin)
{
#define VALUE_MAX 30
    char path[VALUE_MAX];
    char value_str[3];
    int fd;

    snprintf(path, VALUE_MAX, "/sys/class/gpio/gpio%d/value", pin);
    fd = open(path, O_RDONLY);
    if (-1 == fd)
    {
        fprintf(stderr, "Failed to open gpio value for reading!\n");
        return (-1);
    }

    if (-1 == read(fd, value_str, 3))
    {
        fprintf(stderr, "Failed to read value!\n");
        return (-1);
    }

    close(fd);

    return (atoi(value_str));
}

void restore_password(void)
{
    std::string cmd = "sudo cp /opt/lighttpd.user /var/www/html/lighttpd.user";
    system(cmd.c_str());
}

void restore_network(void)
{
    std::string cmd = "sudo cp /etc/network/interfaces_backup /etc/network/interfaces";
    system(cmd.c_str());
    cmd = "sudo reboot";
    system(cmd.c_str());
}

int handle_FPGA_factory(void){
    // Export the desired pin by writing to /sys/class/gpio/export
    std::string gpio_index = "938";
    std::string gpio_name = "gpio" + gpio_index;

    std::cout << "gpio index = " << gpio_index << std::endl;

    int fd = open("/sys/class/gpio/unexport", O_WRONLY);
    if (fd == -1)
    {
        perror("Unable to open /sys/class/gpio/unexport");
        // exit(1);
    }

    if (write(fd, gpio_index.c_str(), 3) != 3)
    {
        perror("Error writing to /sys/class/gpio/unexport");
        // exit(1);
    }

    close(fd);

    fd = open("/sys/class/gpio/export", O_WRONLY);
    if (fd == -1)
    {
        perror("Unable to open /sys/class/gpio/export");
        exit(1);
    }

    if (write(fd, gpio_index.c_str(), 3) != 3)
    {
        perror("Error writing to /sys/class/gpio/export");
        exit(1);
    }

    close(fd);

    // Set the pin to be an output by writing "out" to /sys/class/gpio/gpio24/direction
    std::string gpio_direction = "/sys/class/gpio/" + gpio_name + "/direction";
    fd = open(gpio_direction.c_str(), O_WRONLY);
    if (fd == -1)
    {
        perror("Unable to open direction");
        exit(1);
    }

    if (write(fd, "in", 2) != 2)
    {
        perror("Error writing to direction");
        exit(1);
    }

    close(fd);

    int button_cnt = 0;
    while (1)
    {
        int button_value = GPIORead(std::stoi(gpio_index));
        if (button_value == 1)
        {
            button_cnt++;
        }
        else
        {
            button_cnt = 0;
        }
        if (button_cnt == 5)
        {
            std::cout << "reset factory FPGA" << std::endl;
            for (size_t i = 0; i < NUMBER_FPGA; i++)
            {
                if (fpga[i].check_connection)
                {
                    int voltage_vccint_default = 850;
                    int voltage_hbm_default = 1265;
                    set_voltages(fpga[i].voltage_i2c_addr, fpga[i].path_uio, voltage_vccint_default, voltage_hbm_default);
                }
            }
            std::cout << "reset factory passwork" << std::endl;
            restore_password();

            std::cout << "reset factory all configuration" << std::endl;
            m_config.reset_factory();

            std::cout << "reset factory network and reboot" << std::endl;
            restore_network();

        }
        sleep(1);
    }

    // Unexport the pin by writing to /sys/class/gpio/unexport

    fd = open("/sys/class/gpio/unexport", O_WRONLY);
    if (fd == -1)
    {
        perror("Unable to open /sys/class/gpio/unexport");
        exit(1);
    }

    if (write(fd, gpio_index.c_str(), 3) != 3)
    {
        perror("Error writing to /sys/class/gpio/unexport");
        exit(1);
    }

    close(fd);

    // And exit
    return 0;
}

int global_count_loop_to_send_email = 0;
int global_sent_email = 0;
int global_ready_to_send = 0;
int global_check_temp_condition_alarm = 0;
int global_check_time_condition_alarm = 0;
int global_send_email_reason = 0 ; // 0 is interval ; 1 is dueto hightemp;
//int alert_email_temp = 50;
int global_check_reset_ic_cnt[NUMBER_FPGA];
#define IND_FPGA0 0
#define IND_FPGA1 1
#define IND_FPGA2 2


int main(int argc, char const *argv[])
{
    //printf("\n===================GET EMAIL CONFIG=======================\n");
    //printf("email_enable: %d \n", enable_noti);
   // printf("alert_email_temp: %d \n", alert_email_temp);
    //printf("\n===================DONE=======================\n");
    // init and load configurations
    CommonEngine::config_info m_config_info;
    m_config.init(argv[1]);
    m_config.load(m_config_info);

    // init and load email config
    init_email_config(m_config_info);

    // init fgpa
    init_fpga(m_config_info);

   // std::cout << "debug = " << m_config_info.temps_max[0] << std::endl;
    // start service
    std::thread ApiServiceThread(&api_service);
    std::thread handlePlPingThread(&handle_pl_ping);
    std::thread handleFPGAFactoryThread(&handle_FPGA_factory);

    int next_fan_level = FAN_LEVEL_6;
    int onboard = 0b000;
    global_check_reset_ic_cnt[IND_FPGA0] = 0;
    global_check_reset_ic_cnt[IND_FPGA1] = 0;
    global_check_reset_ic_cnt[IND_FPGA2] = 0;

    for (int i = 0; i < NUMBER_FPGA; i++){
        for (size_t j = 0; j < fpga[i].temps_i2c_addr.size(); j++)
        {
            if (read_temps(fpga[i].temps_i2c_addr[i], &fpga[i].fanLevel, &fpga[i].boardTemp, &fpga[i].chipTemp, fpga[i].path_uio, &fpga[i].check_connection)){
                break;
            }
        }
        
    }
        // read_temps(fpga[i].temp_i2c_addr, &fpga[i].fanLevel, &fpga[i].boardTemp, &fpga[i].chipTemp, fpga[i].path_uio, &fpga[i].check_connection);
    printf("************STARTING INFOR : ********** \n");
    show_fpga_info();

    // setup voltage as configuration
    for (size_t i = 0; i < NUMBER_FPGA; i++)
    {
        if(fpga[i].check_connection){
            if ((m_config_info.voltages_vccint.find(i) != m_config_info.voltages_vccint.end()) && (m_config_info.voltages_hbm.find(i) != m_config_info.voltages_hbm.end())){
                set_voltages(fpga[i].voltage_i2c_addr, fpga[i].path_uio, m_config_info.voltages_vccint[i], m_config_info.voltages_hbm[i]);
            }
        }
    }
    

    // INIT
    if (fpga[0].check_connection && fpga[0].boardTemp < 60 && fpga[0].boardTemp >= MIN_TEMP_SAFETY)
    {
        onboard = onboard | 0b001;
        fpga[0].power_on_fpga = 1;
        //printf("hereeeeeeeeeeeeeeeeeeeee \n");
    }
    if (fpga[1].check_connection && fpga[1].boardTemp < 60 && fpga[1].boardTemp >= MIN_TEMP_SAFETY)
    {
        onboard = onboard | 0b010;
        fpga[1].power_on_fpga = 1;
    }
    if (fpga[2].check_connection && fpga[2].boardTemp < 60 && fpga[2].boardTemp >= MIN_TEMP_SAFETY)
    {
        onboard = onboard | 0b100;
        fpga[2].power_on_fpga = 1;
    }
    on_off_fpga(onboard);

    if ( fan_mode == 1)
    {
        printf("Running fan manual mode with : fan_manual_level = %d \n", fan_manual_level);
        fans_speed_control(fan_manual_level);
        current_fan_level = fan_manual_level;
    }

    while (1)
    {   

    	   //      fans__read_speed_control(0x04);
        // usleep(2000);
        // fans__read_speed_control(0x08);
        // usleep(2000);
        
        if (fan_mode == 1 && fan_manual_level != current_fan_level)
        {
            fans_speed_control(fan_manual_level);
            current_fan_level = fan_manual_level;
        }
        for (int i = 0; i < NUMBER_FPGA; i++)
        {

            // read FPGA temps information
            printf("****************************:FPGA_%d********* \n", i);
            bool read_temp_result = false;
            for (size_t j = 0; j < fpga[i].temps_i2c_addr.size(); j++)
            {
                if (!read_temps(fpga[i].temps_i2c_addr[j], &fpga[i].fanLevel, &fpga[i].boardTemp, &fpga[i].chipTemp, fpga[i].path_uio, &fpga[i].check_connection))
                {
                   // printf("READ FAIL\n");
                    usleep(1000);
                    // continue;
                }
                else{
                    read_temp_result = true;
                    break;
                }
            }
            
            if(read_temp_result){

                global_check_reset_ic_cnt[IND_FPGA0] ++;
                global_check_reset_ic_cnt[IND_FPGA1] ++;
                global_check_reset_ic_cnt[IND_FPGA2] ++;
                if ((fpga[i].boardTemp > fpga[i].temp_max) || (fpga[i].boardTemp < MIN_TEMP_SAFETY) || (fpga[i].chipTemp > fpga[i].temp_max))
                {   
                    printf("oohooo temp is not safe ...............\n");
                    fpga[i].high_temp_cnt++;
                }
                else
                {
                    fpga[i].high_temp_cnt = 0;
                }
                

                if (enable_noti)
                {
                    if ((fpga[i].boardTemp > chip_temp_noti) || (fpga[i].chipTemp > chip_temp_noti))
                    {
                        fpga[i].alert_email_temp_cnt++;


                    }
                    else
                    {
                        fpga[i].alert_email_temp_cnt = 0;
                    }
                }

                if ((fpga[i].old_chipTemp != fpga[i].chipTemp) || (fpga[i].old_boardTemp != fpga[i].boardTemp))
                {
                    global_check_reset_ic_cnt[i] = 0;
                } 


                if ( (global_check_reset_ic_cnt[i]  > INTERVAL_CHECK_RESET_IC_SENSOR) && fpga[i].check_connection )
                {
                        reset_i2c_funtion(0x4D,fpga[i].path_uio);
                        reset_i2c_funtion(0x4E,fpga[i].path_uio);
                        global_check_reset_ic_cnt[i]  = 0;
                        //printf("============reset IC FPGA : %d \n", i);

                }
                else if (global_check_reset_ic_cnt[i] > MAX_COUNT)
                        global_check_reset_ic_cnt[i]  = 0;
                //remember remove
                // printf("\nfpga[%d].old_chipTemp = %d  fpga[%d].old_boardTemp = %d \n", i, fpga[i].old_chipTemp, i,  fpga[i].old_boardTemp );
                // printf("fpga[%d].chipTemp = %d  fpga[%d].boardTemp = %d \n",i,  fpga[i].chipTemp, i, fpga[i].boardTemp );
                // printf("global_check_reset_ic_cnt[%d] = %d \n", i, global_check_reset_ic_cnt[i]);

                fpga[i].old_chipTemp = fpga[i].chipTemp;
                fpga[i].old_boardTemp = fpga[i].boardTemp;

            }
            
            if (!read_voltages(fpga[i].voltage_i2c_addr, fpga[i].path_uio, &fpga[i].voltage_vccint, &fpga[i].voltage_hbm))
            {
               // printf("READ FAIL\n");
                usleep(1000);
                // continue;
            }

            int current_Int;
            int current_HBM;
            usleep(1000);

            read_Current(fpga[i].voltage_i2c_addr, fpga[i].path_uio, &current_Int, &current_HBM  );
            usleep(1000);
            read_wattage(fpga[i].voltage_i2c_addr, fpga[i].path_uio,  &current_Int, &current_HBM  );
            // enable/shutdown FPGA

            // if (fpga[i].fanLevel == 6)
            // {
            //     // shutdown fpga
            //     on_off_fpga(0b000); // TODO
            // }
            // else
            // {                       // enable fpga
            //     on_off_fpga(0b001); // TODO
            // }
        }



        // on off board
        if (fpga[0].check_connection && (fpga[0].boardTemp > fpga[0].temp_max || fpga[0].chipTemp > fpga[0].temp_max || fpga[0].boardTemp < MIN_TEMP_SAFETY ) && fpga[0].high_temp_cnt > CHECK_HIGH_TEMP_CNT)
        {

            // gia dang la 0b101  & 0b110 -> 0b100
            // muon ve 0b000 :
            onboard = onboard & 0b110;
            //off FPGA0
            fpga[0].power_on_fpga = 0;
        }
        else if (fpga[0].check_connection && fpga[0].boardTemp < 60 && fpga[0].boardTemp >= MIN_TEMP_SAFETY)
        {
            onboard = onboard | 0b001;
            fpga[0].power_on_fpga = 1;
        }

        if (fpga[1].check_connection && (fpga[1].boardTemp > fpga[1].temp_max || fpga[1].chipTemp > fpga[1].temp_max || fpga[1].boardTemp < MIN_TEMP_SAFETY) && fpga[1].high_temp_cnt > CHECK_HIGH_TEMP_CNT)
        {

            // gia dang la 0b101  & 0b110 -> 0b100
            // muon ve 0b000 :
            onboard = onboard & 0b101;
            fpga[1].power_on_fpga = 0;
        }
        else if (fpga[1].check_connection && fpga[1].boardTemp < 60 && fpga[1].boardTemp >= MIN_TEMP_SAFETY)
        {
            onboard = onboard | 0b010;
            fpga[1].power_on_fpga = 1;
        }

        if (fpga[2].check_connection && (fpga[2].boardTemp > fpga[2].temp_max || fpga[2].chipTemp > fpga[2].temp_max || fpga[2].boardTemp < MIN_TEMP_SAFETY) && fpga[2].high_temp_cnt > CHECK_HIGH_TEMP_CNT)
        {

            // gia dang la 0b101  & 0b110 -> 0b100
            // muon ve 0b000 :
            onboard = onboard & 0b011;
            fpga[2].power_on_fpga = 0;
        }
        else if (fpga[2].check_connection && fpga[2].boardTemp < 60 &&   fpga[2].boardTemp >= MIN_TEMP_SAFETY)
        {
            onboard = onboard | 0b100;
            fpga[2].power_on_fpga = 1;
        }

        on_off_fpga(onboard);

        // fan controller
        if (!fpga[0].check_connection && !fpga[1].check_connection && !fpga[2].check_connection)
        {
            printf("THERE ARE NO FPGAs CONNECT WITH ZYNQ BOARD \n");
        }
        else
        {  
            if (fan_mode == 0)
            {
                next_fan_level = get_next_fan_level();
                if (current_fan_level != next_fan_level)
                {
                    current_fan_level = next_fan_level;
                    fans_speed_control(current_fan_level);
                }        
            }

        }

        
        if (enable_noti == 1)
        {   
             global_count_loop_to_send_email ++;

           // check condition to reset tem sensor and chip ID
            if (fpga[0].check_connection && (fpga[0].boardTemp > chip_temp_noti || fpga[0].chipTemp > chip_temp_noti) && fpga[0].alert_email_temp_cnt > CHECK_HIGH_TEMP_CNT)
            {
                 fpga[0].alert_email_temp_cnt = 0;
                 global_check_temp_condition_alarm = 1;
            }
            else if (fpga[1].check_connection && (fpga[1].boardTemp > chip_temp_noti || fpga[1].chipTemp > chip_temp_noti) && fpga[1].alert_email_temp_cnt > CHECK_HIGH_TEMP_CNT)
            {

                global_check_temp_condition_alarm = 1;
                 fpga[1].alert_email_temp_cnt = 0;
            }
          
            else if (fpga[2].check_connection && (fpga[2].boardTemp > chip_temp_noti || fpga[2].chipTemp > chip_temp_noti) && fpga[2].alert_email_temp_cnt > CHECK_HIGH_TEMP_CNT)
            {

                global_check_temp_condition_alarm = 1;
                 fpga[2].alert_email_temp_cnt = 0;
            }

            else
            {
                global_check_temp_condition_alarm = 0;
            }


            if ( global_count_loop_to_send_email >= (INTERVAL_SEND_NOTI*frequency_noti) )
            {   
                
                global_count_loop_to_send_email = 0;
                global_check_temp_condition_alarm = 0;
                global_send_email_reason = 0;
                system("sudo nodejs /opt/trm/app.js");

                //send due to overheat
            }
            else  
            {
                if (global_check_temp_condition_alarm == 1 && global_send_email_reason == 0)
                {
                    global_send_email_reason = 1;
                    global_check_temp_condition_alarm = 0;
                    global_count_loop_to_send_email = 0;
                    system("sudo nodejs /opt/trm/app.js");
                }
            }
            // else if (global_count_loop_to_send_email >= MAX_COUNT)
            // {
            //     global_count_loop_to_send_email = 0;
            // } 

            // printf("global_check_temp_condition_alarm : %d ; global_count_loop_to_send_email = %d; INTERVAL_SEND_NOTI = %d \n",global_check_temp_condition_alarm,  global_count_loop_to_send_email, INTERVAL_SEND_NOTI);
            // printf("fpga[0].alert_email_temp_cnt = %d; fpga[1].alert_email_temp_cnt = %d; fpga[2].alert_email_temp_cnt = %d; ", fpga[0].alert_email_temp_cnt, fpga[1].alert_email_temp_cnt, fpga[2].alert_email_temp_cnt  );
           
        }


        // printf("\n enable_noti : %d ;  desti_email_noti : %d ; chip_temp_noti : %d  frequency_noti: %d , enable_hash_noti: %d , enable_fan_noti: %d, over_heat_temp_noti: %d, enable_miner_status_noti: %d \n ", enable_noti,
        // desti_email_noti,
        // chip_temp_noti,
        // frequency_noti,
        // enable_hash_noti,
        // enable_fan_noti,
        // over_heat_temp_noti,
        // enable_miner_status_noti);


         printf("Current  fans level : %d ; speed   %d percent \n", current_fan_level, current_fan_level * 10);
        show_fpga_info();

        // check netowkring
        std::string net_cmd = "sudo ip addr show eth0 | grep \"secondary\"";
        std::string cmd_out = exeCommand(net_cmd);
        if(cmd_out.length() > 5){
            std::cout << "flush secondary ip = " << cmd_out << std::endl;
            net_cmd = "sudo ip addr flush dev eth0 secondary";
            system(net_cmd.c_str());
        }

        sleep(20);
    }

    return 0;
}
