/**
******************************************************************************
* @file           : config.cpp
* @brief          : Source file of configuration
* @author         : Tuan_NT
******************************************************************************
******************************************************************************
*/
/* Include ------------------------------------------------------------------*/
#include "config.hpp"
#include "utility/utility.hpp"
#include "rapidjson/document.h"
#include "rapidjson/writer.h"
#include <iostream>
#include <map>

using namespace CommonEngine;
using namespace rapidjson;

bool Configuration::load(config_info &m_config_info)
{
    std::cout << "BoxConfig::loadConfig - start" << std::endl;

    bool result = false;
    // load file config
    this->locker.lock();
    if(UtilityFunc::checkFile(this->file_path)){
        FILE *file = NULL;
        size_t len;
        ssize_t read;
        char *line = NULL;
        file = fopen(this->file_path.c_str(), "r");
        if (file != NULL)
        {
            read = getline(&line, &len, file);
            if (read != -1)
            {
                std::string json_data(line);
                // free(string);
                std::cout << "Config::loadConfig - read config file = " << json_data << std::endl;

                try{
                    // parse json data        
                    Document document;
                    document.Parse(json_data.c_str());
                    if (document.HasMember("temps_i2c_addr"))
                    {
                        const Value &temps_i2c_addr = document["temps_i2c_addr"];
                        if(temps_i2c_addr.IsArray()){
                            for (size_t i = 0; i < temps_i2c_addr.Size(); i++)
                            {
                                std::string temp_i2c_addr = temps_i2c_addr[i].GetString();
                                m_config_info.temps_i2c_addr.push_back(temp_i2c_addr);
                            }
                            if(temps_i2c_addr.Size() == 0){
                                std::string temp_i2c_addr = "0x4D";
                                m_config_info.temps_i2c_addr.push_back(temp_i2c_addr);
                                temp_i2c_addr = "0x4E";
                                m_config_info.temps_i2c_addr.push_back(temp_i2c_addr);
                            }
                        }
                    }
                    if (document.HasMember("voltage_i2c_addr"))
                    {
                        std::string voltage_i2c_addr = document["voltage_i2c_addr"].GetString();
                        m_config_info.voltage_i2c_addr = voltage_i2c_addr;
                    }
                    // chips_type
                    if (document.HasMember("chips_type"))
                    {
                        const Value &chips_type = document["chips_type"];
                        if (chips_type.IsArray())
                        {
                            for (size_t i = 0; i < chips_type.Size(); i++)
                            {
                                const Value &obj_value = chips_type[i];
                                if (obj_value.HasMember("id") && obj_value.HasMember("value"))
                                {
                                    uint8_t id = obj_value["id"].GetInt();
                                    std::string value = obj_value["value"].GetString();
                                    m_config_info.chips_type[id] = value;
                                }
                            }
                        }
                    }
                    // temps_max
                    if (document.HasMember("temps_max"))
                    {
                        const Value &temps_max = document["temps_max"];
                        if(temps_max.IsArray()){
                            for (size_t i = 0; i < temps_max.Size(); i++)
                            {
                                const Value &obj_value = temps_max[i];
                                if(obj_value.HasMember("id") && obj_value.HasMember("value")){
                                    uint8_t id = obj_value["id"].GetInt();
                                    int value = obj_value["value"].GetInt();
                                    m_config_info.temps_max[id] = value;
                                }
                            }
                            
                        }
                    }
                    // voltages_vccint
                    if (document.HasMember("voltages_vccint"))
                    {
                        const Value &voltages_vccint = document["voltages_vccint"];
                        if (voltages_vccint.IsArray())
                        {
                            for (size_t i = 0; i < voltages_vccint.Size(); i++)
                            {
                                const Value &obj_value = voltages_vccint[i];
                                if (obj_value.HasMember("id") && obj_value.HasMember("value"))
                                {
                                    uint8_t id = obj_value["id"].GetInt();
                                    int value = obj_value["value"].GetInt();
                                    m_config_info.voltages_vccint[id] = value;
                                }
                            }
                        }
                    }
                    // voltages_hbm
                    if (document.HasMember("voltages_hbm"))
                    {
                        const Value &voltages_hbm = document["voltages_hbm"];
                        if (voltages_hbm.IsArray())
                        {
                            for (size_t i = 0; i < voltages_hbm.Size(); i++)
                            {
                                const Value &obj_value = voltages_hbm[i];
                                if (obj_value.HasMember("id") && obj_value.HasMember("value"))
                                {
                                    uint8_t id = obj_value["id"].GetInt();
                                    int value = obj_value["value"].GetInt();
                                    m_config_info.voltages_hbm[id] = value;
                                }
                            }
                        }
                    }

                    // temp_device
                    if (document.HasMember("temp_device"))
                    {
                        const Value &temp_device = document["temp_device"];
                        if (temp_device.IsArray())
                        {
                            for (size_t i = 0; i < temp_device.Size(); i++)
                            {
                                const Value &obj_value = temp_device[i];
                                if (obj_value.HasMember("id") && obj_value.HasMember("value"))
                                {
                                    uint8_t id = obj_value["id"].GetInt();
                                    int value = obj_value["value"].GetInt();
                                    m_config_info.temp_device[id] = value;
                                }
                            }
                        }
                    }

                    // fan_mode
                    if (document.HasMember("fan_mode")){
                        uint8_t fan_mode = document["fan_mode"].GetInt();
                        m_config_info.fan_mode = fan_mode;
                    }

                    // fan_manual_level
                    if (document.HasMember("fan_manual_level"))
                    {
                        uint8_t fan_manual_level = document["fan_manual_level"].GetInt();
                        m_config_info.fan_manual_level = fan_manual_level;
                    }

                    // email config
                    if (document.HasMember("enable_noti"))
                    {
                        std::string enable_noti = document["enable_noti"].GetString();
                        m_config_info.enable_noti = enable_noti;
                    }
                    if (document.HasMember("desti_email_noti"))
                    {
                        std::string desti_email_noti = document["desti_email_noti"].GetString();
                        m_config_info.desti_email_noti = desti_email_noti;
                    }
                    if (document.HasMember("chip_temp_noti"))
                    {
                        std::string chip_temp_noti = document["chip_temp_noti"].GetString();
                        m_config_info.chip_temp_noti = chip_temp_noti;
                    }
                    if (document.HasMember("frequency_noti"))
                    {
                        std::string frequency_noti = document["frequency_noti"].GetString();
                        m_config_info.frequency_noti = frequency_noti;
                    }
                    if (document.HasMember("enable_hash_noti"))
                    {
                        std::string enable_hash_noti = document["enable_hash_noti"].GetString();
                        m_config_info.enable_hash_noti = enable_hash_noti;
                    }
                    if (document.HasMember("enable_fan_noti"))
                    {
                        std::string enable_fan_noti = document["enable_fan_noti"].GetString();
                        m_config_info.enable_fan_noti = enable_fan_noti;
                    }
                    if (document.HasMember("over_heat_temp_noti"))
                    {
                        std::string over_heat_temp_noti = document["over_heat_temp_noti"].GetString();
                        m_config_info.over_heat_temp_noti = over_heat_temp_noti;
                    }
                    if (document.HasMember("enable_miner_status_noti"))
                    {
                        std::string enable_miner_status_noti = document["enable_miner_status_noti"].GetString();
                        m_config_info.enable_miner_status_noti = enable_miner_status_noti;
                    }

                    result = true;
                }
                catch(const std::exception& e){
                    std::cout << "Config::loadConfig - exception when parse json data = " << json_data << std::endl;
                    result = false;
                }
            }
            else{
                std::cout << "Config::loadConfig - config file not read" << std::endl;
            }
        }
        else{
            std::cout << "Config::loadConfig - config file not opened" << std::endl;
        }
        fclose(file);
    }
    else{
        std::cout << "Config::loadConfig - config file not found" << std::endl;
        UtilityFunc::creatFile(this->file_path);
    }
    this->locker.unlock();
    return result;
}

bool Configuration::save(config_info &m_config_info)
{
    this->locker.lock();
    StringBuffer str_buff;
    Writer<StringBuffer> writer(str_buff);

    writer.StartObject();
    writer.Key("temps_i2c_addr");
    writer.StartArray();
    for (size_t i = 0; i < m_config_info.temps_i2c_addr.size(); i++)
    {
        writer.String(m_config_info.temps_i2c_addr[i].c_str());
    }
    writer.EndArray();

    writer.Key("voltage_i2c_addr");
    writer.String(m_config_info.voltage_i2c_addr.c_str());

    writer.Key("chips_type");
    writer.StartArray();
    std::map<uint8_t, std::string>::iterator s_map;
    for (s_map = m_config_info.chips_type.begin(); s_map != m_config_info.chips_type.end(); s_map++)
    {
        writer.StartObject();
        writer.Key("id");
        writer.Int(s_map->first);
        writer.Key("value");
        writer.String(s_map->second.c_str());
        writer.EndObject();
    }
    writer.EndArray();

    writer.Key("temps_max");
    writer.StartArray();
    std::map<uint8_t, int>::iterator m_map;
    for (m_map = m_config_info.temps_max.begin(); m_map != m_config_info.temps_max.end(); m_map++)
    {
        writer.StartObject();
        writer.Key("id");
        writer.Int(m_map->first);
        writer.Key("value");
        writer.Int(m_map->second);
        writer.EndObject();
    }

    writer.EndArray();

    writer.Key("voltages_vccint");
    writer.StartArray();
    for (m_map = m_config_info.voltages_vccint.begin(); m_map != m_config_info.voltages_vccint.end(); m_map++)
    {
        writer.StartObject();
        writer.Key("id");
        writer.Int(m_map->first);
        writer.Key("value");
        writer.Int(m_map->second);
        writer.EndObject();
    }

    writer.EndArray();

    writer.Key("voltages_hbm");
    writer.StartArray();
    for (m_map = m_config_info.voltages_hbm.begin(); m_map != m_config_info.voltages_hbm.end(); m_map++)
    {
        writer.StartObject();
        writer.Key("id");
        writer.Int(m_map->first);
        writer.Key("value");
        writer.Int(m_map->second);
        writer.EndObject();
    }

    writer.EndArray();

    writer.Key("temp_device");
    writer.StartArray();
    for (m_map = m_config_info.temp_device.begin(); m_map != m_config_info.temp_device.end(); m_map++)
    {
        writer.StartObject();
        writer.Key("id");
        writer.Int(m_map->first);
        writer.Key("value");
        writer.Int(m_map->second);
        writer.EndObject();
    }
    writer.EndArray();

    writer.Key("fan_mode");
    writer.Int(m_config_info.fan_mode);
    writer.Key("fan_manual_level");
    writer.Int(m_config_info.fan_manual_level);

    // email
    writer.Key("enable_noti");
    writer.String(m_config_info.enable_noti.c_str());
    writer.Key("desti_email_noti");
    writer.String(m_config_info.desti_email_noti.c_str());
    writer.Key("chip_temp_noti");
    writer.String(m_config_info.chip_temp_noti.c_str());
    writer.Key("frequency_noti");
    writer.String(m_config_info.frequency_noti.c_str());
    writer.Key("enable_hash_noti");
    writer.String(m_config_info.enable_hash_noti.c_str());
    writer.Key("enable_fan_noti");
    writer.String(m_config_info.enable_fan_noti.c_str());
    writer.Key("over_heat_temp_noti");
    writer.String(m_config_info.over_heat_temp_noti.c_str());
    writer.Key("enable_miner_status_noti");
    writer.String(m_config_info.enable_miner_status_noti.c_str());

    writer.EndObject();

    std::string str_out = str_buff.GetString();

    bool result = false;
    std::string configFile = this->file_path;
    FILE *file = NULL;
    // usleep(100000);
    file = fopen(configFile.c_str(), "wb");
    assert(file != NULL);
    if (file != NULL)
    {
        fputs(str_out.c_str(), file);
        result = true;
    }
    if (file != NULL)
    {
        fclose(file);
    }
    this->locker.unlock();
    return true;
}

// bool Configuration::load_email_config(email_config_info &m_config_info){
//     std::cout << "BoxConfig::load_email_config - start" << std::endl;

//     bool result = false;
//     // load file config
//     this->locker.lock();
//     if (UtilityFunc::checkFile(this->email_path))
//     {
//         FILE *file = NULL;
//         size_t len;
//         ssize_t read;
//         char *line = NULL;
//         file = fopen(this->email_path.c_str(), "r");
//         if (file != NULL)
//         {
//             read = getline(&line, &len, file);
//             if (read != -1)
//             {
//                 std::string json_data(line);
//                 // free(string);
//                 std::cout << "Config::load_email_config - read config file = " << json_data << std::endl;

//                 try
//                 {
//                     // parse json data
//                     Document document;
//                     ParseResult parse_result = document.Parse(json_data.c_str());
//                     if(parse_result){
//                         if (document.HasMember("enable"))
//                         {
//                             std::string enable = document["enable"].GetString();
//                             m_config_info.enable = enable;
//                         }
//                         if (document.HasMember("chip_temp"))
//                         {
//                             std::string chip_temp = document["chip_temp"].GetString();
//                             m_config_info.chip_temp = chip_temp;
//                         }
//                         result = true;
//                     }
//                 }
//                 catch (const std::exception &e)
//                 {
//                     std::cout << "Config::load_email_config - exception when parse json data = " << json_data << std::endl;
//                     result = false;
//                 }
//             }
//             else
//             {
//                 std::cout << "Config::load_email_config - config file not read" << std::endl;
//             }
//         }
//         else
//         {
//             std::cout << "Config::load_email_config - config file not opened" << std::endl;
//         }
//         fclose(file);
//     }
//     else
//     {
//         std::cout << "Config::load_email_config - config file not found = " << this->email_path << std::endl;
//         UtilityFunc::creatFile(this->email_path);
//     }
//     this->locker.unlock();
//     return result;
// }