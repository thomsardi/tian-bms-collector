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

bool Talis5JsonHandler::parseSetNetwork(JsonVariant &json, WifiParameterData& wifiParameterData)
{
    if( !json.containsKey("mode") || !json.containsKey("server") || !json.containsKey("ip") ||
        !json.containsKey("gateway") || !json.containsKey("subnet") || !json.containsKey("ssid") ||
        !json.containsKey("password") )
    {
        return 0;
    }

    JsonVariant mode = json["mode"];
    JsonVariant server = json["server"];
    JsonVariant ip = json["ip"];
    JsonVariant gateway = json["gateway"];
    JsonVariant subnet = json["subnet"];
    JsonVariant ssid = json["ssid"];
    JsonVariant password = json["password"];

    wifiParameterData.mode = mode.as<uint8_t>();
    wifiParameterData.server = server.as<uint8_t>();
    wifiParameterData.ip = ip.as<String>();
    wifiParameterData.gateway = gateway.as<String>();
    wifiParameterData.subnet = subnet.as<String>();
    wifiParameterData.ssid = ssid.as<String>();
    wifiParameterData.password = password.as<String>();
    return 1;
}

bool Talis5JsonHandler::parseModbusScan(JsonVariant &json)
{
    if (!json.containsKey("scan"))
    {
        return 0;
    }

    JsonVariant scan = json["scan"];

    if (scan.as<int>() > 0)
    {
        return 1;
    }
    return 0;
}

bool Talis5JsonHandler::parseSetModbus(JsonVariant &json, Talis5ParameterData& talis5ParameterData)
{
    if (!json.containsKey("ip") || !json.containsKey("port"))
    {
        return 0;
    }

    JsonVariant ip = json["ip"];
    JsonVariant port = json["port"];
    talis5ParameterData.modbusTargetIp = ip.as<String>();
    talis5ParameterData.modbusPort = port.as<int>();
    return 1;
}

bool Talis5JsonHandler::parseRestart(JsonVariant& json)
{
    if (!json.containsKey("restart"))
    {
        return 0;
    }

    JsonVariant restart = json["restart"];

    if (restart.as<int>() > 0)
    {
        return 1;
    }
    return 0;
}

bool Talis5JsonHandler::parseFactoryReset(JsonVariant& json)
{
    if (!json.containsKey("freset"))
    {
        return 0;
    }

    JsonVariant freset = json["freset"];

    if (freset.as<int>() > 0)
    {
        return 1;
    }
    return 0;
}

Talis5JsonHandler::~Talis5JsonHandler()
{
}