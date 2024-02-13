#ifndef TALIS5_JSON_HANDLER_H
#define TALIS5_JSON_HANDLER_H

#include <stdint.h>
#include <ArduinoJson.h>
#include <AsyncJson.h>
#include <vector>

class Talis5JsonHandler
{
private:
    /* data */
    const char* _TAG = "Talis5 Json Handler";
public:
    Talis5JsonHandler(/* args */);
    String buildJsonResponse(int code);
    size_t parseSetSlaveJson(JsonVariant& json, std::vector<uint8_t>& buff);
    ~Talis5JsonHandler();
};




#endif