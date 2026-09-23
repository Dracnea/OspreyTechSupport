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

using namespace rapidjson;
using namespace httplib;
httplib::Server svr;

std::mutex i2c_locker;
std::mutex uart_locker;

#define MAP_SIZE 4096

#define writeReg(ip_ptr, reg, value) *((unsigned *)(((u_int8_t *)ip_ptr) + reg)) = value
#define readReg(ip_ptr, reg) *((unsigned *)(((u_int8_t *)ip_ptr) + reg))

#define RESET_FPGA_UIO_PATH "/dev/uio7"
#define FAN_UIO_PATH "/dev/uio0"
#define CHECK_HIGH_TEMP_CNT 4

#define TEMP_MAX_SAFETY     95

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

#define ON_FPGA 1
#define OFF_FPGA 0
#define NUMBER_FPGA 3

#define GPIO_RELEASE 0
#define GPIO_PRESS 1

typedef struct __attribute__((packed))
{
    int temp_max;
    int boardTemp;
    int chipTemp;
    int fanLevel;
    int index;
    int check_connection;
    char path_uio[30];
    int high_temp_cnt;
    int voltage_vccint;
    int voltage_hbm;
} FPGA_Info;

FPGA_Info fpga[NUMBER_FPGA];
int current_fan_level = FAN_LEVEL_6;

void init_fpga()
{
    for (int i = 0; i < NUMBER_FPGA; i++)
    {
        fpga[i].temp_max = TEMP_MAX_SAFETY;
        fpga[i].boardTemp = 90;
        fpga[i].chipTemp = 90;
        fpga[i].fanLevel = FAN_LEVEL_1;
        fpga[i].check_connection = 0;
        fpga[i].high_temp_cnt = 0;
    }
    strcpy(fpga[0].path_uio, "/dev/uio4"); // I2C 1
    strcpy(fpga[1].path_uio, "/dev/uio5"); // I2C 1
    strcpy(fpga[2].path_uio, "/dev/uio6"); // I2C 1
}

static int on_off_fpga(int inputValue)
{
    printf("========================================== onboard = %02hhX\n", inputValue);
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
}

static int set_voltages(char *uiod_iic, int voltage_vccint, int voltage_hbm)
{
    void *ip_ptr;
    int fd;
    u8 slaveAddr;
    u8 Buffer[2];
    printf("set vol_vccint_input = %d, vol_hbm_input = %d\n", voltage_vccint, voltage_hbm);

    uint8_t vol_vccint_step_cvt;
    int vol_vccint_step = (voltage_vccint - 850) / 5;

    uint8_t vol_hbm_step_cvt;
    int vol_hbm_step = (voltage_hbm - 1265) / 5;

    if ((vol_vccint_step > 20) || (vol_vccint_step < -50))
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
    printf("set vol_vccint_step = 0x%x, vol_vccint_step_cvt = 0x%x\n", vol_vccint_step, vol_vccint_step_cvt);

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
    printf("set vol_hbm_step = 0x%x, vol_hbm_step_cvt = 0x%x\n", vol_hbm_step, vol_hbm_step_cvt);

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

    slaveAddr = 0x7C;
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

static int read_voltages(char *uiod_iic, int *voltage_vccint, int *voltage_hbm)
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

    slaveAddr = 0x7C;

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
            printf("volt page 0 = %d mV \n", rd_val);
            if((rd_val >= 600) && (rd_val <= 950)){
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
            printf("volt page 1 = %d mV \n", rd_val);
            if ((rd_val >= 1000) && (rd_val <= 1300))
            {
                *voltage_hbm = rd_val;
            }
        }
    }
    munmap(ip_ptr, MAP_SIZE);
    close(fd);
    i2c_locker.unlock();
}

static int read_temps(int *fan_level, int *board_temp, int *chip_temp, char *uiod_iic, int *check_connection)
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

    slaveAddr = 0x4E;

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
                if (Buffer[0] > -20 && Buffer[0] < 150)
                {
                    read_oke = 1;
                    break;
                }
            }
        }
        if (read_oke == 1)
        {
            printf("debug 1\n");
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
                if (Buffer[0] > 0 && Buffer[0] < 150)
                {
                    read_oke = 1;
                    break;
                }
            }
        }

        if (read_oke == 1)
        {
            printf("debug 2\n");
            // *chip_temp = Buffer[0];
            *check_connection = 1;
            m_chip_temp = Buffer[0];
        }
        else
        {
            *check_connection = 0;
        }

        if ((m_board_temp > 0) && (m_chip_temp > 0))
        {
            *chip_temp = m_chip_temp;
            *board_temp = m_board_temp;
            if (*chip_temp >= 70)
            {
                *fan_level = FAN_LEVEL_10;
            }
            else if (*chip_temp >= 65)
            {
                *fan_level = FAN_LEVEL_9;
            }
            else if (*chip_temp >= 60)
            {
                *fan_level = FAN_LEVEL_8;
            }
            else if (*chip_temp >= 55)
            {
                *fan_level = FAN_LEVEL_7;
            }
            else if (*chip_temp >= 50)
            {
                *fan_level = FAN_LEVEL_6;
            }
            else if (*chip_temp >= 45)
            {
                *fan_level = FAN_LEVEL_5;
            }

            else if (*chip_temp >= 40)
            {
                *fan_level = FAN_LEVEL_4;
            }
            else if (*chip_temp >= 35)
            {
                *fan_level = FAN_LEVEL_3;
            }

            else if (*chip_temp >= 30)
            {
                *fan_level = FAN_LEVEL_2;
            }

            else if (*chip_temp < 30)
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
}
void show_fpga_info()
{
    for (int i = 0; i < NUMBER_FPGA; i++)
    {

        printf("Index: %d ; board_temp : %d; chip_temp : %d ; path: %s ; is_plug : %d \n", i, fpga[i].boardTemp, fpga[i].chipTemp, fpga[i].path_uio, fpga[i].check_connection);
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
        std::string body_res = "{\"result\":0}";
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
                            set_voltages(fpga[i].path_uio, voltage_vccint, voltage_hbm);
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
                    set_voltages(fpga[boardId].path_uio, voltage_vccint, voltage_hbm);
                }
                // response
                body_res = "{\"result\":1}";
                return true;
            }
            return false;
        });
        printf("api_service::voltage_control - response = %s\n", body_res.c_str());
        res.set_content(body_res, "application/json"); });
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
            if (!read_temps(&fpga[i].fanLevel, &fpga[i].boardTemp, &fpga[i].chipTemp, fpga[i].path_uio, &fpga[i].check_connection))
            {
                printf("READ FAIL\n");
                usleep(1000);
                continue;
            }
            else
            {
                read_voltages(fpga[i].path_uio, &fpga[i].voltage_vccint, &fpga[i].voltage_hbm);
                printf("READ OK\n");
                // if ((fpga[i].boardTemp > 90) || (fpga[i].chipTemp > 90))
                // {
                //     fpga[i].high_temp_cnt++;
                // }
                // else
                // {
                //     fpga[i].high_temp_cnt = 0;
                // }
            }
        }
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
            writer.Key("status");
            writer.Int(fpga[i].check_connection);
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

void set_temp_max(void)
{
    svr.Post("/controller/setTempMax", [&](const Request &req, Response &res, const ContentReader &content_reader)
             {
        std::string body;
        std::string body_res = "{\"result\":0}";
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
    set_temp_max();
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
            printf("send valid signal... \n");
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
                    set_voltages(fpga[i].path_uio, voltage_vccint_default, voltage_hbm_default);
                }
            }
            std::cout << "reset factory passwork" << std::endl;
            restore_password();

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

int main(int argc, char const *argv[])
{
    std::thread ApiServiceThread(&api_service);
    std::thread handlePlPingThread(&handle_pl_ping);
    std::thread handleFPGAFactoryThread(&handle_FPGA_factory);

    int next_fan_level = FAN_LEVEL_6;
    int onboard = 0b000;

    memset(fpga, 0, sizeof(fpga));
    init_fpga();

    for (int i = 0; i < NUMBER_FPGA; i++)
        read_temps(&fpga[i].fanLevel, &fpga[i].boardTemp, &fpga[i].chipTemp, fpga[i].path_uio, &fpga[i].check_connection);
    printf("************STARTING INFOR : ********** \n");
    show_fpga_info();

    // INIT
    if (fpga[0].check_connection && fpga[0].boardTemp < 60)
    {
        onboard = onboard | 0b001;
    }
    if (fpga[1].check_connection && fpga[1].boardTemp < 60)
    {
        onboard = onboard | 0b010;
    }
    if (fpga[2].check_connection && fpga[2].boardTemp < 60)
    {
        onboard = onboard | 0b100;
    }
    on_off_fpga(onboard);

    while (1)
    {
        for (int i = 0; i < NUMBER_FPGA; i++)
        {

            // read FPGA temps information
            printf("****************************:FPGA_%d********* ", i);
            if (!read_temps(&fpga[i].fanLevel, &fpga[i].boardTemp, &fpga[i].chipTemp, fpga[i].path_uio, &fpga[i].check_connection))
            {
                printf("READ FAIL\n");
                usleep(1000);
                continue;
            }
            else
            {
                read_voltages(fpga[i].path_uio, &fpga[i].voltage_vccint, &fpga[i].voltage_hbm);
                printf("READ OK\n");
                if ((fpga[i].boardTemp > fpga[i].temp_max) || (fpga[i].chipTemp > fpga[i].temp_max))
                {
                    fpga[i].high_temp_cnt++;
                }
                else
                {
                    fpga[i].high_temp_cnt = 0;
                }
            }

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
        if (fpga[0].check_connection && (fpga[0].boardTemp > fpga[0].temp_max || fpga[0].chipTemp > fpga[0].temp_max) && fpga[0].high_temp_cnt > CHECK_HIGH_TEMP_CNT)
        {

            // gia dang la 0b101  & 0b110 -> 0b100
            // muon ve 0b000 :
            onboard = onboard & 0b110;
        }
        else if (fpga[0].check_connection && fpga[0].boardTemp < 60)
        {
            onboard = onboard | 0b001;
        }

        if (fpga[1].check_connection && (fpga[1].boardTemp > fpga[1].temp_max || fpga[1].chipTemp > fpga[1].temp_max) && fpga[1].high_temp_cnt > CHECK_HIGH_TEMP_CNT)
        {

            // gia dang la 0b101  & 0b110 -> 0b100
            // muon ve 0b000 :
            onboard = onboard & 0b101;
        }
        else if (fpga[1].check_connection && fpga[1].boardTemp < 60)
        {
            onboard = onboard | 0b010;
        }

        if (fpga[2].check_connection && (fpga[2].boardTemp > fpga[2].temp_max || fpga[2].chipTemp > fpga[2].temp_max) && fpga[2].high_temp_cnt > CHECK_HIGH_TEMP_CNT)
        {

            // gia dang la 0b101  & 0b110 -> 0b100
            // muon ve 0b000 :
            onboard = onboard & 0b011;
        }
        else if (fpga[2].check_connection && fpga[2].boardTemp < 60)
        {
            onboard = onboard | 0b100;
        }

        on_off_fpga(onboard);

        // fan controller
        if (!fpga[0].check_connection && !fpga[1].check_connection && !fpga[2].check_connection)
        {
            printf("THERE ARE NO FPGAs CONNECT WITH ZYNQ BOARD \n");
        }
        else
        {
            next_fan_level = get_next_fan_level();
            if (current_fan_level != next_fan_level)
            {
                current_fan_level = next_fan_level;
                fans_speed_control(current_fan_level);
            }
        }

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