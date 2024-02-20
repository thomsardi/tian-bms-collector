#include "TianBMS.h"

TianBMS::TianBMS(TianBMSUtils::Endianess endianess)
{
    _endianess = endianess;
    esp_log_level_set(_TAG, ESP_LOG_INFO);
}

bool TianBMS::update(uint8_t id, uint32_t token, uint16_t* data, size_t dataSize)
{
    TokenInfo tokenInfo = parseToken(token);
    uint8_t requestType = tokenInfo.requestType;
    switch (requestType)
    {
    case TianBMSUtils::RequestType::REQUEST_DATA :
        /* code */
        _bmsData[id].id = id;
        if (updateData(id, data, dataSize))
        {
            _bmsData[id].msgCount++;
            return true;
        }
        break;
    case TianBMSUtils::RequestType::REQUEST_PCB_CODE :
        _bmsData[id].id = id;
        if (_endianess == TianBMSUtils::Endianess::ENDIAN_LITTLE)
        {
            if (updatePcbBarcode(id, data, dataSize, true))
            {
                _bmsData[id].msgCount++;
                return true;
            }
        }
        else
        {
            if (updatePcbBarcode(id, data, dataSize))
            {
                _bmsData[id].msgCount++;
                return true;
            }
        }
        break;
    case TianBMSUtils::RequestType::REQUEST_SN1_CODE :
        _bmsData[id].id = id;
        if (_endianess == TianBMSUtils::Endianess::ENDIAN_LITTLE)
        {
            if (updateSnCode1(id, data, dataSize, true))
            {
                _bmsData[id].msgCount++;
                return true;
            }
        }
        else
        {
            if (updateSnCode1(id, data, dataSize))
            {
                _bmsData[id].msgCount++;
                return true;
            }
        }
        
        break;
    case TianBMSUtils::RequestType::REQUEST_SN2_CODE :
        if (_endianess == TianBMSUtils::Endianess::ENDIAN_LITTLE)
        {
            if (updateSnCode2(id, data, dataSize, true))
            {
                _bmsData[id].msgCount++;
                return true;
            }
        }
        else
        {
            if (updateSnCode2(id, data, dataSize))
            {
                _bmsData[id].msgCount++;
                return true;
            }
        }
        break;
    case TianBMSUtils::RequestType::REQUEST_SCAN :
        /* code */
        _bmsData[id].id = id;
        if (updateOnScan(id, data, dataSize))
        {
            _bmsData[id].msgCount++;
            return true;
        }
        break;
    default:
        break;
    }
    
    return false;
}

TokenInfo TianBMS::parseToken(uint32_t token)
{
    TokenInfo tokenInfo;
    tokenInfo.id = (token - _uniqueIdentifier) >> 24;
    tokenInfo.requestType = (token - _uniqueIdentifier) & 0x00FFFFFF;
    return tokenInfo;
}

bool TianBMS::updateOnError(uint32_t token)
{
    TokenInfo tokenInfo = parseToken(token);
    ESP_LOGI(_TAG, "Id : %d error\n", tokenInfo.id);
    if (_bmsData.find(tokenInfo.id) != _bmsData.end())
    {
        _bmsData[tokenInfo.id].errorCount++;
        return true;
    }
    return false;
}

void TianBMS::cleanUp()
{
    std::vector<int> obsoleteDataIndex;
    obsoleteDataIndex.reserve(16);
    std::map<int, TianBMSData>::iterator it;
    for (it = _bmsData.begin(); it != _bmsData.end(); it++)
    {
        if ((*it).second.errorCount > _maxErrorCount)
        {
            obsoleteDataIndex.push_back((*it).first);
        }
    }

    for (size_t i = 0; i < obsoleteDataIndex.size(); i++)
    {
        _bmsData.erase(obsoleteDataIndex.at(i));
    }
}

void TianBMS::setMaxErrorCount(uint8_t maxErrorCount)
{
    _maxErrorCount = maxErrorCount;
}

bool TianBMS::updateData(uint8_t id, uint16_t* data, size_t dataSize)
{    
    if (dataSize >= 43)
    {
        _bmsData[id].packVoltage = *data++;
        _bmsData[id].packCurrent = *data++;
        _bmsData[id].remainingCapacity = *data++;
        _bmsData[id].avgCellTemperature = *data++;
        _bmsData[id].envTemperature = *data++;
        _bmsData[id].warningFlag.value = *data++;
        _bmsData[id].protectionFlag.value = *data++;
        _bmsData[id].faultStatusFlag.value = *data++;
        _bmsData[id].soc = *data++;
        _bmsData[id].soh = *data++;
        _bmsData[id].fullChargedCap = *data++;
        _bmsData[id].cycleCount = *data++;
        for (size_t i = 0; i < _bmsData[id].cellVoltage.size(); i++)
        {
            _bmsData[id].cellVoltage[i] = *data++;
        }
        for (size_t i = 0; i < _bmsData[id].cellTemperature.size(); i++)
        {
            _bmsData[id].cellTemperature[i] = *data++;
        }
        _bmsData[id].balanceTemperature = *data++;
        _bmsData[id].maxCellVoltage = *data++;
        _bmsData[id].minCellVoltage = *data++;
        _bmsData[id].cellVoltageDiff = *data++;
        _bmsData[id].maxCellTemp = *data++;
        _bmsData[id].minCellTemp = *data++;
        _bmsData[id].fetTemp = *data++;
        _bmsData[id].remainChgTime = ((*data++) << 16) + *data++;
        _bmsData[id].remainDsgTime = ((*data++) << 16) + *data++;
        return true;
    }
    return false;
}

bool TianBMS::updateOnScan(uint8_t id, uint16_t* data, size_t dataSize)
{    
    if (dataSize == 1)
    {
        _bmsData[id].packVoltage = *data++;
        ESP_LOGI(_TAG, "Update on scan");
        return true;
    }
    return false;
}

bool TianBMS::updatePcbBarcode(uint8_t id, uint16_t* data, size_t dataSize, bool swap)
{
    if (Utilities::uint16ArrayToCharArray(data, dataSize, _bmsData[id].pcbBarcode.data(), _bmsData[id].pcbBarcode.size(), swap) > 0)
    {
        return true;
    }
    return false;
}

bool TianBMS::updateSnCode1(uint8_t id, uint16_t* data, size_t dataSize, bool swap)
{
    if (Utilities::uint16ArrayToCharArray(data, dataSize, _bmsData[id].snCode1.data(), _bmsData[id].snCode1.size(), swap) > 0)
    {
        return true;
    }
    return false;
}

bool TianBMS::updateSnCode2(uint8_t id, uint16_t* data, size_t dataSize, bool swap)
{
    if (Utilities::uint16ArrayToCharArray(data, dataSize, _bmsData[id].snCode2.data(), _bmsData[id].snCode2.size(), swap) > 0)
    {
        return true;
    }
    return false;
}

uint32_t TianBMS::getToken(uint8_t id, TianBMSUtils::RequestType requestType)
{
    uint32_t token = requestType + _uniqueIdentifier + (id << 24);
    return token;
}

void TianBMS::clearData()
{
    _bmsData.clear();
}

std::map<int, TianBMSData>& TianBMS::getTianBMSData()
{
    return _bmsData;
}

void TianBMS::getCloneTianBMSData(std::map<int, TianBMSData>& buff)
{
    buff = _bmsData;
}

uint16_t TianBMS::getPackVoltage(uint8_t id)
{
    if (_bmsData.find(id) != _bmsData.end())
    {
        return _bmsData[id].packVoltage;
    }
    return 0;
}
uint16_t TianBMS::getPackCurrent(uint8_t id)
{
    if (_bmsData.find(id) != _bmsData.end())
    {
        return _bmsData[id].packCurrent;
    }
    return 0;
}
uint16_t TianBMS::getRemainingCapacity(uint8_t id)
{
    if (_bmsData.find(id) != _bmsData.end())
    {
        return _bmsData[id].remainingCapacity;
    }
    return 0;
}
int16_t TianBMS::getAvgCellTemperature(uint8_t id)
{
    if (_bmsData.find(id) != _bmsData.end())
    {
        return _bmsData[id].avgCellTemperature;
    }
    return 0;
}
int16_t TianBMS::getEnvTemperature(uint8_t id)
{
    if (_bmsData.find(id) != _bmsData.end())
    {
        return _bmsData[id].envTemperature;
    }
    return 0;
}
WarningFlag TianBMS::getWarningFlag(uint8_t id)
{
    WarningFlag flag;
    flag.value = 0;
    if (_bmsData.find(id) != _bmsData.end())
    {
        return _bmsData[id].warningFlag;
    }
    return flag;
}
ProtectionFlag TianBMS::getProtectionFlag(uint8_t id)
{
    ProtectionFlag flag;
    flag.value = 0;
    if (_bmsData.find(id) != _bmsData.end())
    {
        return _bmsData[id].protectionFlag;
    }
    return flag;
}
FaultStatusFlag TianBMS::getFaultStatusFlag(uint8_t id)
{
    FaultStatusFlag flag;
    flag.value = 0;
    if (_bmsData.find(id) != _bmsData.end())
    {
        return _bmsData[id].faultStatusFlag;
    }
    return flag;
}
uint16_t TianBMS::getSoc(uint8_t id)
{
    if (_bmsData.find(id) != _bmsData.end())
    {
        return _bmsData[id].soc;
    }
    return 0;
}
uint16_t TianBMS::getSoh(uint8_t id)
{
    if (_bmsData.find(id) != _bmsData.end())
    {
        return _bmsData[id].soh;
    }
    return 0;
}
uint16_t TianBMS::getFullChargedCap(uint8_t id)
{
    if (_bmsData.find(id) != _bmsData.end())
    {
        return _bmsData[id].fullChargedCap;
    }
    return 0;
}
uint16_t TianBMS::getCycleCount(uint8_t id)
{
    if (_bmsData.find(id) != _bmsData.end())
    {
        return _bmsData[id].cycleCount;
    }
    return 0;
}
void TianBMS::getCellVoltage(uint8_t id, std::array<uint16_t, 16> buffer)
{
    if (_bmsData.find(id) != _bmsData.end())
    {
        for (size_t i = 0; i < buffer.size(); i++)
        {
            buffer[i] = _bmsData[id].cellVoltage[i];
        }
    }
    
}
void TianBMS::getCellTemperature(uint8_t id, std::array<uint16_t, 4> buffer)
{
    if (_bmsData.find(id) != _bmsData.end())
    {
        for (size_t i = 0; i < buffer.size(); i++)
        {
            buffer[i] = _bmsData[id].cellTemperature[i];
        }
    }
}
uint16_t TianBMS::getBalanceTemperature(uint8_t id)
{
    if (_bmsData.find(id) != _bmsData.end())
    {
        return _bmsData[id].balanceTemperature;
    }
    return 0;
}
uint16_t TianBMS::getMaxCellVoltage(uint8_t id)
{
    if (_bmsData.find(id) != _bmsData.end())
    {
        return _bmsData[id].maxCellVoltage;
    }
    return 0;
}
uint16_t TianBMS::getMinCellVoltage(uint8_t id)
{
    if (_bmsData.find(id) != _bmsData.end())
    {
        return _bmsData[id].minCellVoltage;
    }
    return 0;
}
uint16_t TianBMS::getCellVoltageDiff(uint8_t id)
{
    if (_bmsData.find(id) != _bmsData.end())
    {
        return _bmsData[id].cellVoltageDiff;
    }
    return 0;
}
uint16_t TianBMS::getMaxCellTemp(uint8_t id)
{
    if (_bmsData.find(id) != _bmsData.end())
    {
        return _bmsData[id].maxCellTemp;
    }
    return 0;
}
uint16_t TianBMS::getMinCellTemp(uint8_t id)
{
    if (_bmsData.find(id) != _bmsData.end())
    {
        return _bmsData[id].minCellTemp;
    }
    return 0;
}
uint16_t TianBMS::getFetTemp(uint8_t id)
{
    if (_bmsData.find(id) != _bmsData.end())
    {
        return _bmsData[id].fetTemp;
    }
    return 0;
}
uint32_t TianBMS::getRemainChgTime(uint8_t id)
{
    if (_bmsData.find(id) != _bmsData.end())
    {
        return _bmsData[id].remainChgTime;
    }
    return 0;
}
uint32_t TianBMS::getRemainDsgTime(uint8_t id)
{
    if (_bmsData.find(id) != _bmsData.end())
    {
        return _bmsData[id].remainDsgTime;
    }
    return 0;
}
std::string TianBMS::getPcbBarcode(uint8_t id)
{
    if (_bmsData.find(id) != _bmsData.end())
    {
        std::string result(_bmsData[id].pcbBarcode.data());
        return result;
    }
    return 0;
}
std::string TianBMS::getSnCode1(uint8_t id)
{
    if (_bmsData.find(id) != _bmsData.end())
    {
        std::string result(_bmsData[id].snCode1.data());
        return result;
    }
    return 0;
}
std::string TianBMS::getSnCode2(uint8_t id)
{
    if (_bmsData.find(id) != _bmsData.end())
    {
        std::string result(_bmsData[id].snCode2.data());
        return result;
    }
    return 0;
}

TianBMS::~TianBMS()
{
}