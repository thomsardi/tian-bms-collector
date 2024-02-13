#include "Talis5Memory.h"

Talis5Memory::Talis5Memory(/* args */)
{
}

void Talis5Memory::begin(String name)
{
    Preferences preferences;
    preferences.begin(name.c_str());
    _name = name;
    if (!preferences.isKey("init_flg")) // get init flag to detect the first time init
    {
        ESP_LOGI(_TAG, "Create default");
        createDefault();
        copy();
    }
    if (preferences.getBool("rst_flg")) // get reset flag to revert the user parameter into default
    {
        ESP_LOGI(_TAG, "Revert to default");
        copy();
        preferences.putBool("rst_flg", false);
    }
    _isActive = true;
    printDefault();
    printUser();
    writeShadow();
}

void Talis5Memory::createDefault()
{
    Preferences preferences;
    preferences.begin(_name.c_str());
    preferences.putString("d_mbus_ip", "192.168.4.1");
    preferences.putUShort("d_mbus_port", 502);
    std::array<uint8_t, 16> slaveList;
    for (size_t i = 0; i < slaveList.size(); i++)
    {
        slaveList[i] = i + 1;
    }
    preferences.putBytes("d_slave_list", slaveList.data(), slaveList.size());
    preferences.putBool("init_flg", true);
    preferences.putBool("rst_flg", false);
    preferences.end();
}

void Talis5Memory::copy()
{
    Preferences preferences;
    preferences.begin(_name.c_str());
    preferences.putString("u_mbus_ip", preferences.getString("d_mbus_ip"));
    preferences.putUShort("u_mbus_port", preferences.getUShort("d_mbus_port"));
    size_t len = preferences.getBytesLength("d_slave_list");
    std::vector<uint8_t> arr;
    arr.reserve(len);
    arr.resize(len);
    preferences.getBytes("d_slave_list", arr.data(), len);
    preferences.putBytes("u_slave_list", arr.data(), len);
    preferences.end();
}

void Talis5Memory::setModbusTargetIp(String ip)
{
    if (_isActive)
    {
        _shadowParameter.modbusTargetIp = ip;
    }
}

void Talis5Memory::setModbusPort(uint16_t port)
{
    if (_isActive)
    {
        _shadowParameter.modbusPort = port;
    }
}

size_t Talis5Memory::setSlave(const uint8_t* value, size_t len)
{
    if (_shadowParameter.slaveList.size() < len)
    {
        return 0;
    }

    if (_isActive)
    {
        size_t bytesWritten = 0;
        for (size_t i = 0; i < len; i++)
        {
            _shadowParameter.slaveList[i] = value[i];
            bytesWritten++;
        }
        for (size_t i = len; i < _shadowParameter.slaveList.size(); i++)
        {
            _shadowParameter.slaveList[i] = 0;
        }
        return bytesWritten;
    }
    return 0;
}

void Talis5Memory::save()
{
    if (_isActive)
    {
        Preferences preferences;
        preferences.begin(_name.c_str());
        preferences.putString("u_mbus_ip", _shadowParameter.modbusTargetIp);
        preferences.putUShort("u_mbus_port", _shadowParameter.modbusPort);

        std::vector<uint8_t> arr;
        arr.reserve(_shadowParameter.slaveList.size());
        for (size_t i = 0; i < _shadowParameter.slaveList.size(); i++)
        {
            uint8_t value = _shadowParameter.slaveList[i];
            if (value != 0)
            {
                arr.push_back(value);
            }
            else
            {
                break;
            }
        }
        preferences.putBytes("u_slave_list", arr.data(), arr.size());
        preferences.end();
    }
}

void Talis5Memory::writeShadow()
{
    if (_isActive)
    {
        Preferences preferences;
        preferences.begin(_name.c_str());
        _shadowParameter.modbusTargetIp = preferences.getString("u_mbus_ip");
        _shadowParameter.modbusPort = preferences.getUShort("u_mbus_port");
        size_t len = preferences.getBytesLength("u_slave_list");
        std::vector<uint8_t> arr;
        arr.reserve(len);
        arr.resize(len);
        preferences.getBytes("u_slave_list", arr.data(), len);
        for (size_t i = 0; i < len; i++)
        {
            _shadowParameter.slaveList[i] = arr.at(i);
        }

        for (size_t i = len; i < _shadowParameter.slaveList.size(); i++)
        {
            _shadowParameter.slaveList[i] = 0;
        }
        preferences.end();
    }
}

void Talis5Memory::cancel()
{
    writeShadow();
}

void Talis5Memory::reset()
{
    if (_isActive)
    {
        Preferences preferences;
        preferences.begin(_name.c_str());
        preferences.putBool("rst_flg", true);
        preferences.end();
        _isActive = false;
        restart();
    }
}

void Talis5Memory::restart()
{
    begin(_name);
}

void Talis5Memory::clear()
{
    if (_isActive)
    {
        Preferences preferences;
        preferences.begin(_name.c_str());
        preferences.clear();
        preferences.end();
        _isActive = false;
    }
}

String Talis5Memory::getModbusTargetIp()
{
    if (_isActive)
    {
        Preferences preferences;
        preferences.begin(_name.c_str());
        String value = preferences.getString("u_mbus_ip");
        preferences.end();
        return value;
    }
    return "";
}

uint16_t Talis5Memory::getModbusPort()
{
    if (_isActive)
    {
        Preferences preferences;
        preferences.begin(_name.c_str());
        int value = preferences.getUShort("u_mbus_port");
        preferences.end();
        return value;
    }
    return 0;
}

size_t Talis5Memory::getSlaveSize()
{
    if (_isActive)
    {
        Preferences preferences;
        preferences.begin(_name.c_str());
        size_t len = preferences.getBytesLength("u_slave_list");
        preferences.end();
        return len;
    }
    return 0;
}

size_t Talis5Memory::getSlave(uint8_t *buffer, size_t len)
{
    if (_isActive)
    {
        Preferences preferences;
        preferences.begin(_name.c_str());
        size_t elementSize = preferences.getBytesLength("u_slave_list");
        if (len < elementSize)
        {
            return 0;
        }
        size_t result = preferences.getBytes("u_slave_list", buffer, len);
        preferences.end();
        return result;
    }
    return 0;
}

void Talis5Memory::printDefault()
{
    Preferences preferences;
    preferences.begin(_name.c_str());
    ESP_LOGI(_TAG, "d_mbus_ip : %s\n", preferences.getString("d_mbus_ip").c_str());
    ESP_LOGI(_TAG, "d_mbus_port : %d\n", preferences.getUShort("d_mbus_port"));

    std::vector<uint8_t> arr;
    size_t len = preferences.getBytesLength("d_slave_list");
    arr.reserve(len);
    arr.resize(len);
    preferences.getBytes("d_slave_list", arr.data(), len);
    for (size_t i = 0; i < len; i++)
    {
        ESP_LOGI(_TAG, "d_slave_list : %d\n", arr.at(i));
    }
    preferences.end();
}

void Talis5Memory::printUser()
{
    Preferences preferences;
    preferences.begin(_name.c_str());
    ESP_LOGI(_TAG, "u_mbus_ip : %s\n", preferences.getString("u_mbus_ip").c_str());
    ESP_LOGI(_TAG, "u_mbus_port : %d\n", preferences.getUShort("u_mbus_port"));

    std::vector<uint8_t> arr;
    size_t len = preferences.getBytesLength("u_slave_list");
    arr.reserve(len);
    arr.resize(len);
    preferences.getBytes("u_slave_list", arr.data(), len);
    for (size_t i = 0; i < len; i++)
    {
        ESP_LOGI(_TAG, "u_slave_list : %d\n", arr.at(i));
    }
    preferences.end();
}

Talis5Memory::~Talis5Memory()
{
}