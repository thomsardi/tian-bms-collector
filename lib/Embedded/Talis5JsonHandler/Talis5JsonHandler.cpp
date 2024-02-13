#include "Talis5JsonHandler.h"

Talis5JsonHandler::Talis5JsonHandler(/* args */)
{
}

size_t Talis5JsonHandler::parseSetSlaveJson(JsonVariant& json, std::vector<uint8_t>& buff)
{
    size_t len = 0;
    if (!json.containsKey("slave-list") || !json.containsKey("slave-set"))
    {
        return 0;
    }

    JsonVariant flag = json["slave-set"];
    if (flag.as<int>() > 0)
    {
        JsonArray arr = json["slave-list"].as<JsonArray>();
        buff.reserve(arr.size());
        ESP_LOGI(_TAG, "arr size : %d\n", arr.size());
        for (size_t i = 0; i < arr.size(); i++)
        {
            JsonVariant value = arr[i];
            buff.push_back(value.as<uint8_t>());
        }
        for (size_t i = 0; i < buff.size(); i++)
        {
            ESP_LOGI(_TAG, "slave list : %d\n", buff.at(i));
        }
        len = arr.size();
    }
    return len;
}

String Talis5JsonHandler::buildJsonResponse(int code)
{
    StaticJsonDocument<16> doc;
    String response;
    doc["status"] = code;
    serializeJson(doc, response);
    return response;
}

Talis5JsonHandler::~Talis5JsonHandler()
{
}