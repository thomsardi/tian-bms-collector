#ifndef TIANBMS_H
#define TIANBMS_H

#include <stdint.h>
#include <array>
#include <Utilities.h>
#include <vector>
#include <memory>
#include <ArduinoJson.h>
#include <map>

namespace TianBMSUtils {
    enum RequestType : uint8_t 
    {
        REQUEST_DATA = 0x00,
        REQUEST_PCB_CODE = 0x01,
        REQUEST_SN1_CODE = 0x02,
        REQUEST_SN2_CODE = 0x03,
        REQUEST_SCAN = 0x04
    };

    enum Endianess : uint8_t 
    {
        ENDIAN_LITTLE = 0,
        ENDIAN_BIG = 1
    };
}

union WarningFlag
{
    struct Bits 
    {
        // byte 0
        uint16_t cell_ov_alarm : 1;
        uint16_t cell_uv_alarm : 1;
        uint16_t pack_ov_alarm : 1;
        uint16_t pack_uv_alarm : 1;
        uint16_t chg_oc_alarm : 1;
        uint16_t dchg_oc_alarm : 1;
        uint16_t bat_ot_alarm : 1;
        uint16_t bat_ut_alarm : 1;
        // byte 1
        uint16_t env_ot_alarm : 1;
        uint16_t env_ut_alarm : 1;
        uint16_t mos_ot_alarm : 1;
        uint16_t low_capacity : 1;
        uint16_t :4;
    } bits;
    uint16_t value;

    WarningFlag()
    {
        this->value = 0;
    }
};

union ProtectionFlag
{
    struct Bits 
    {
        // byte 0
        uint16_t cell_ov_prot : 1;
        uint16_t cell_uv_prot : 1;
        uint16_t pack_ov_prot : 1;
        uint16_t pack_uv_prot : 1;
        uint16_t short_prot : 1;
        uint16_t oc_prot : 1;
        uint16_t chg_ot_prot : 1;
        uint16_t chg_ut_prot : 1;
        // byte 1
        uint16_t dchg_ot_prot : 1;
        uint16_t dchg_ut_prot : 1;
        uint16_t :6;
    } bits;
    uint16_t value;

    ProtectionFlag()
    {
        this->value = 0;
    }
};

union FaultStatusFlag
{
    struct Bits
    {
        // byte 0
        uint16_t comm_sampling_fault : 1;
        uint16_t temp_sensor_break : 1;
        uint16_t :6;
        // byte 1
        uint16_t soc : 1;
        uint16_t sod : 1;
        uint16_t chg_mos : 1;
        uint16_t dchg_mos : 1;
        uint16_t chg_limit_function : 1;
        uint16_t :3;
    } bits;
    uint16_t value;

    FaultStatusFlag()
    {
        this->value = 0;
    }
};

struct TianBMSData
{
    uint32_t msgCount = 0;
    uint8_t errorCount = 0;
    uint8_t id = 0;
    uint16_t packVoltage = 0;
    int16_t packCurrent = 0;
    uint16_t remainingCapacity = 0;
    int16_t avgCellTemperature = 0;
    int16_t envTemperature = 0;
    WarningFlag warningFlag;
    ProtectionFlag protectionFlag;
    FaultStatusFlag faultStatusFlag;
    uint16_t soc = 0;
    uint16_t soh = 0;
    uint16_t fullChargedCap = 0;
    uint16_t cycleCount = 0;
    std::array<uint16_t, 16> cellVoltage;
    std::array<uint16_t, 4> cellTemperature;
    uint16_t balanceTemperature = 0;
    uint16_t maxCellVoltage = 0;
    uint16_t minCellVoltage = 0;
    uint16_t cellVoltageDiff = 0;
    uint16_t maxCellTemp = 0;
    uint16_t minCellTemp = 0;
    uint16_t fetTemp = 0;
    uint32_t remainChgTime = 0;
    uint32_t remainDsgTime = 0;
    std::array<char, 33> pcbBarcode;
    std::array<char, 33> snCode1;
    std::array<char, 33> snCode2;

    TianBMSData()
    {
        pcbBarcode.fill(0);
        snCode1.fill(0);
        snCode2.fill(0);
        cellVoltage.fill(0);
        cellTemperature.fill(0);
    }

};

struct TokenInfo
{
    uint8_t id = 0;
    uint8_t requestType = 0;
};

class TianBMS
{
private:
    /* data */
    std::map<int, TianBMSData> _bmsData;
    const char* _TAG = "TIANBMS";
    uint32_t _uniqueIdentifier = 12345;
    uint8_t _endianess;
    uint8_t _maxErrorCount = 3;
    bool updateData(uint8_t id, uint16_t* data, size_t dataSize);
    bool updateOnScan(uint8_t id, uint16_t* data, size_t dataSize);
    bool updatePcbBarcode(uint8_t id, uint16_t* data, size_t dataSize, bool swap = false);
    bool updateSnCode1(uint8_t id, uint16_t* data, size_t dataSize, bool swap = false);
    bool updateSnCode2(uint8_t id, uint16_t* data, size_t dataSize, bool swap = false);
public:
    TianBMS(TianBMSUtils::Endianess endianess = TianBMSUtils::Endianess::ENDIAN_LITTLE);
    ~TianBMS();
    bool update(uint8_t id, uint32_t token, uint16_t* data, size_t dataSize);
    bool updateOnError(uint8_t id, uint32_t token);
    void cleanUp();
    void setMaxErrorCount(uint8_t maxErrorCount);
    uint32_t getToken(uint8_t id, TianBMSUtils::RequestType requestType);
    TokenInfo parseToken(uint32_t token);
    std::map<int, TianBMSData>& getTianBMSData();
    uint16_t getPackVoltage(uint8_t id);
    uint16_t getPackCurrent(uint8_t id);
    uint16_t getRemainingCapacity(uint8_t id);
    int16_t getAvgCellTemperature(uint8_t id);
    int16_t getEnvTemperature(uint8_t id);
    WarningFlag getWarningFlag(uint8_t id);
    ProtectionFlag getProtectionFlag(uint8_t id);
    FaultStatusFlag getFaultStatusFlag(uint8_t id);
    uint16_t getSoc(uint8_t id);
    uint16_t getSoh(uint8_t id);
    uint16_t getFullChargedCap(uint8_t id);
    uint16_t getCycleCount(uint8_t id);
    void getCellVoltage(uint8_t id, std::array<uint16_t, 16> buffer);
    void getCellTemperature(uint8_t id, std::array<uint16_t, 4> buffer);
    uint16_t getBalanceTemperature(uint8_t id);
    uint16_t getMaxCellVoltage(uint8_t id);
    uint16_t getMinCellVoltage(uint8_t id);
    uint16_t getCellVoltageDiff(uint8_t id);
    uint16_t getMaxCellTemp(uint8_t id);
    uint16_t getMinCellTemp(uint8_t id);
    uint16_t getFetTemp(uint8_t id);
    uint32_t getRemainChgTime(uint8_t id);
    uint32_t getRemainDsgTime(uint8_t id);
    std::string getPcbBarcode(uint8_t id);
    std::string getSnCode1(uint8_t id);
    std::string getSnCode2(uint8_t id);
};

class TianBMSJsonManager
{
private:
    
public:
    TianBMSJsonManager();
    ~TianBMSJsonManager();
    String buildData(const TianBMSData &tianBMSData);
};





#endif