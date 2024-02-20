#include "TianBMS.h"

TianBMS::TianBMS(TianBMSUtils::Endianess endianess)
{
    _endianess = endianess;
    esp_log_level_set(_TAG, ESP_LOG_INFO);
}

/**
 * Update the bms data, pcb barcode, sn1 code, or sn2 code. The incoming data identified based on token
 * @param[in]   id  the id of the slave
 * @param[in]   token   token of the message, also act as identifier
 * @param[in]   data    pointer to data array of uint16_t
 * @param[in]   dataSize    the length of the data
 * @return      true if update, false if nothing is updated
*/
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

/**
 * Parse the token into id and request type
 * @param[in]   token   token of the incoming message
 * @return      TokenInfo data type
*/
TokenInfo TianBMS::parseToken(uint32_t token)
{
    TokenInfo tokenInfo;
    tokenInfo.id = (token - _uniqueIdentifier) >> 24;
    tokenInfo.requestType = (token - _uniqueIdentifier) & 0x00FFFFFF;
    return tokenInfo;
}

/**
 * Update error counter when error happened. This will increase the error count on data to signal if error count reached
 * certain threshold to call data cleanUp method
 * 
 * @param[in]   token   token of the incoming message
 * @return      true when updated, false if nothing is updated
*/
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

/**
 * Method to cleanup the data with too many errors. This detect the error count of each data, after reaching certain threshold
 * the data will be deleted and free up space for another data
*/
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

/**
 * Set the max threshold for error count
*/
void TianBMS::setMaxErrorCount(uint8_t maxErrorCount)
{
    _maxErrorCount = maxErrorCount;
}

/**
 * Update the bms data, it is expecting the data length to be >= 43 based on the required register to get all the data
 * 
 * @param[in]   id  the id of the slave
 * @param[in]   data    pointer to data array of uint16_t
 * @param[in]   dataSize    the length of data array
 * @return      true if updated, false if nothing is updated
*/
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

/**
 * Update when scan is happening, expecting single data register of packVoltage
 * 
 * @param[in]   id  id of the slave
 * @param[in]   data    pointer to data array of uint16_t
 * @param[in]   dataSize    length of the data array
 * @return      true if updated, false if nothing is updated
*/
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

/**
 * Update pcb barcode
 * 
 * @param[in]   id  id of the slave
 * @param[in]   data    pointer to data array of uint16_t
 * @param[in]   dataSize    length of the data array
 * @param[in]   swap    swap the MSB and LSB of the uint16_t
 * 
 * @return  true if success update, false if failed
*/
bool TianBMS::updatePcbBarcode(uint8_t id, uint16_t* data, size_t dataSize, bool swap)
{
    if (Utilities::uint16ArrayToCharArray(data, dataSize, _bmsData[id].pcbBarcode.data(), _bmsData[id].pcbBarcode.size(), swap) > 0)
    {
        return true;
    }
    return false;
}

/**
 * Update sn1 code
 * 
 * @param[in]   id  id of the slave
 * @param[in]   data    pointer to data array of uint16_t
 * @param[in]   dataSize    length of the data array
 * @param[in]   swap    swap the MSB and LSB of the uint16_t
 * 
 * @return  true if success update, false if failed
*/
bool TianBMS::updateSnCode1(uint8_t id, uint16_t* data, size_t dataSize, bool swap)
{
    if (Utilities::uint16ArrayToCharArray(data, dataSize, _bmsData[id].snCode1.data(), _bmsData[id].snCode1.size(), swap) > 0)
    {
        return true;
    }
    return false;
}

/**
 * Update sn2 code
 * 
 * @param[in]   id  id of the slave
 * @param[in]   data    pointer to data array of uint16_t
 * @param[in]   dataSize    length of the data array
 * @param[in]   swap    swap the MSB and LSB of the uint16_t
 * 
 * @return  true if success update, false if failed
*/
bool TianBMS::updateSnCode2(uint8_t id, uint16_t* data, size_t dataSize, bool swap)
{
    if (Utilities::uint16ArrayToCharArray(data, dataSize, _bmsData[id].snCode2.data(), _bmsData[id].snCode2.size(), swap) > 0)
    {
        return true;
    }
    return false;
}

/**
 * Get token identifier based on id and request type
 * 
 * @param[in]   id  id of the slave
 * @param[in]   requestType request type, refer to TianBMSUtils::RequestType
 * 
 * @return  token
*/
uint32_t TianBMS::getToken(uint8_t id, TianBMSUtils::RequestType requestType)
{
    uint32_t token = requestType + _uniqueIdentifier + (id << 24);
    return token;
}

/**
 * Clear bms data
*/
void TianBMS::clearData()
{
    _bmsData.clear();
}

/**
 * get bms data object
 * 
 * @return bms data object
*/
std::map<int, TianBMSData>& TianBMS::getTianBMSData()
{
    return _bmsData;
}

/**
 * get clone of bms data object
 * 
 * @param[in]   buff    std::map<int, TianBMSDATA> object
*/
void TianBMS::getCloneTianBMSData(std::map<int, TianBMSData>& buff)
{
    buff = _bmsData;
}

/**
 * get pack voltage from bms data
 * 
 * @param[in]   id  id of the slave
 * @return      pack voltage (divider 100), 5820 = 58.20V
*/
uint16_t TianBMS::getPackVoltage(uint8_t id)
{
    if (_bmsData.find(id) != _bmsData.end())
    {
        return _bmsData[id].packVoltage;
    }
    return 0;
}

/**
 * get pack current from bms data
 * 
 * @param[in]   id  id of the slave
 * 
 * @return      pack current (divider 100), 2560 = 25.6A
*/
uint16_t TianBMS::getPackCurrent(uint8_t id)
{
    if (_bmsData.find(id) != _bmsData.end())
    {
        return _bmsData[id].packCurrent;
    }
    return 0;
}

/**
 * get remaining capacity from bms data
 * 
 * @param[in]   id  id of the slave
 * 
 * @return      remaining capacity (divider 100), 6800 = 68Ah
*/
uint16_t TianBMS::getRemainingCapacity(uint8_t id)
{
    if (_bmsData.find(id) != _bmsData.end())
    {
        return _bmsData[id].remainingCapacity;
    }
    return 0;
}

/**
 * get average cell temperature from bms data
 * 
 * @param[in]   id  id of the slave
 * 
 * @return      average cell temperature (divider 10), 327 = 32.7 celcius
*/
int16_t TianBMS::getAvgCellTemperature(uint8_t id)
{
    if (_bmsData.find(id) != _bmsData.end())
    {
        return _bmsData[id].avgCellTemperature;
    }
    return 0;
}

/**
 * get environment temperature from bms data
 * 
 * @param[in]   id  id of the slave
 * 
 * @return      environment temperature (divider 10), 327 = 32.7 celcius
*/
int16_t TianBMS::getEnvTemperature(uint8_t id)
{
    if (_bmsData.find(id) != _bmsData.end())
    {
        return _bmsData[id].envTemperature;
    }
    return 0;
}

/**
 * get warning flag from bms data
 * 
 * @param[in]   id  id of the slave
 * 
 * @return      warning flag, refer to WarningFlag data structure for more information
*/
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

/**
 * get protection flag from bms data
 * 
 * @param[in]   id  id of the slave
 * 
 * @return      protection flag, refer to ProtectionFlag data structure for more information
*/
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

/**
 * get fault status flag from bms data
 * 
 * @param[in]   id  id of the slave
 * 
 * @return      fault status flag, refer to FaultStatusFlag data structure for more information
*/
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

/**
 * get soc from bms data
 * 
 * @param[in]   id  id of the slave
 * 
 * @return      soc (divider 10), 5860 = 56.60%
*/
uint16_t TianBMS::getSoc(uint8_t id)
{
    if (_bmsData.find(id) != _bmsData.end())
    {
        return _bmsData[id].soc;
    }
    return 0;
}

/**
 * get soh from bms data
 * 
 * @param[in]   id  id of the slave
 * 
 * @return      soh (divider 10), 5860 = 56.60%
*/
uint16_t TianBMS::getSoh(uint8_t id)
{
    if (_bmsData.find(id) != _bmsData.end())
    {
        return _bmsData[id].soh;
    }
    return 0;
}

/**
 * get full charged capacity from bms data
 * 
 * @param[in]   id  id of the slave
 * 
 * @return      full charged capacity (divider 100), 10000 = 100.00Ah
*/
uint16_t TianBMS::getFullChargedCap(uint8_t id)
{
    if (_bmsData.find(id) != _bmsData.end())
    {
        return _bmsData[id].fullChargedCap;
    }
    return 0;
}

/**
 * get cycle count from bms data
 * 
 * @param[in]   id  id of the slave
 * 
 * @return      cycle count
*/
uint16_t TianBMS::getCycleCount(uint8_t id)
{
    if (_bmsData.find(id) != _bmsData.end())
    {
        return _bmsData[id].cycleCount;
    }
    return 0;
}

/**
 * get cell voltages (1 - 16) from bms data
 * 
 * @param[in]   id  id of the slave
 * @param[in]   buffer  array of uint16_t with size of 16. 3256 = 3256mV
*/
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

/**
 * get cell temperature (1 - 4) from bms data
 * 
 * @param[in]   id  id of the slave
 * @param[in]   buffer  array of uint16_t with size of 4 (divider 10). 327 = 32.7 celcius
*/
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

/**
 * get balance temperature from bms data
 * 
 * @param[in]   id  id of the slave
 * 
 * @return      balance temperature (divider 10), 327 = 32.7 celcius
*/
uint16_t TianBMS::getBalanceTemperature(uint8_t id)
{
    if (_bmsData.find(id) != _bmsData.end())
    {
        return _bmsData[id].balanceTemperature;
    }
    return 0;
}

/**
 * get max cell voltage from bms data
 * 
 * @param[in]   id  id of the slave
 * 
 * @return      max cell voltage. 3257 = 3257mV
*/
uint16_t TianBMS::getMaxCellVoltage(uint8_t id)
{
    if (_bmsData.find(id) != _bmsData.end())
    {
        return _bmsData[id].maxCellVoltage;
    }
    return 0;
}

/**
 * get min cell voltage from bms data
 * 
 * @param[in]   id  id of the slave
 * 
 * @return      min cell voltage. 2566 = 2566
*/
uint16_t TianBMS::getMinCellVoltage(uint8_t id)
{
    if (_bmsData.find(id) != _bmsData.end())
    {
        return _bmsData[id].minCellVoltage;
    }
    return 0;
}

/**
 * get cell difference voltage from bms data
 * 
 * @param[in]   id  id of the slave
 * 
 * @return      cell difference voltage. 15 = 15mV
*/
uint16_t TianBMS::getCellVoltageDiff(uint8_t id)
{
    if (_bmsData.find(id) != _bmsData.end())
    {
        return _bmsData[id].cellVoltageDiff;
    }
    return 0;
}

/**
 * get max cell temperature from bms data
 * 
 * @param[in]   id  id of the slave
 * 
 * @return      max cell temperature (divider 10), 327 = 32.7 celcius
*/
uint16_t TianBMS::getMaxCellTemp(uint8_t id)
{
    if (_bmsData.find(id) != _bmsData.end())
    {
        return _bmsData[id].maxCellTemp;
    }
    return 0;
}

/**
 * get min cell temperature from bms data
 * 
 * @param[in]   id  id of the slave
 * 
 * @return      min cell temperature (divider 10), 327 = 32.7 celcius
*/
uint16_t TianBMS::getMinCellTemp(uint8_t id)
{
    if (_bmsData.find(id) != _bmsData.end())
    {
        return _bmsData[id].minCellTemp;
    }
    return 0;
}

/**
 * get fet temperature from bms data
 * 
 * @param[in]   id  id of the slave
 * 
 * @return      fet temperature (divider 10), 327 = 32.7 celcius
*/
uint16_t TianBMS::getFetTemp(uint8_t id)
{
    if (_bmsData.find(id) != _bmsData.end())
    {
        return _bmsData[id].fetTemp;
    }
    return 0;
}

/**
 * get remaining charge time from bms data
 * 
 * @param[in]   id  id of the slave
 * 
 * @return      remaining charge time in seconds. 1800 = 1800 seconds
*/
uint32_t TianBMS::getRemainChgTime(uint8_t id)
{
    if (_bmsData.find(id) != _bmsData.end())
    {
        return _bmsData[id].remainChgTime;
    }
    return 0;
}

/**
 * get remaining charge time from bms data
 * 
 * @param[in]   id  id of the slave
 * 
 * @return      remaining discharge time in seconds. 3600 = 3600 seconds
*/
uint32_t TianBMS::getRemainDsgTime(uint8_t id)
{
    if (_bmsData.find(id) != _bmsData.end())
    {
        return _bmsData[id].remainDsgTime;
    }
    return 0;
}

/**
 * get pcb barcode from bms data
 * 
 * @param[in]   id  id of the slave
 * 
 * @return      pcb barcode as std::string
*/
std::string TianBMS::getPcbBarcode(uint8_t id)
{
    if (_bmsData.find(id) != _bmsData.end())
    {
        std::string result(_bmsData[id].pcbBarcode.data());
        return result;
    }
    return 0;
}

/**
 * get sn1 code from bms data
 * 
 * @param[in]   id  id of the slave
 * 
 * @return      sn1 code as std::string
*/
std::string TianBMS::getSnCode1(uint8_t id)
{
    if (_bmsData.find(id) != _bmsData.end())
    {
        std::string result(_bmsData[id].snCode1.data());
        return result;
    }
    return 0;
}

/**
 * get sn2 code from bms data
 * 
 * @param[in]   id  id of the slave
 * 
 * @return      sn2 code as std::string
*/
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