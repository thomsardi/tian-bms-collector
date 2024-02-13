#ifndef TALIS5_MEMORY_H
#define TALIS5_MEMORY_H

#include <Arduino.h>
#include <Preferences.h>
#include <stdint.h>
#include <map>
#include <memory>
#include <vector>
#include "LittleFS.h"

struct Talis5ParameterData
{
    String modbusTargetIp;
    uint16_t modbusPort;
    std::array<uint8_t, 255> slaveList;

    Talis5ParameterData()
    {
        slaveList.fill(0);
    }
};

class Talis5Memory
{
private:
    /* data */
    const char* _TAG = "Talis5 Memory Save";
    Talis5ParameterData _shadowParameter;
    String _name;
    bool _isActive = false;
    void copy();
    void createDefault();
    void writeShadow();
public:
    Talis5Memory(/* args */);
    void printDefault();
    void printUser();

    void begin(String name);
    void save();
    void cancel();
    void reset();
    void restart();
    void clear();

    void setModbusTargetIp(String ip);
    void setModbusPort(uint16_t port);
    void setSlave(uint8_t start, uint8_t end);

    String getModbusTargetIp();
    uint16_t getModbusPort();
    size_t setSlave(const uint8_t* value, size_t len);
    size_t getSlaveSize();
    size_t getSlave(uint8_t *buffer, size_t len);

    ~Talis5Memory();
};




#endif