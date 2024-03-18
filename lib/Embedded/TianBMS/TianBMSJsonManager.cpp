#include "TianBMS.h"

TianBMSJsonManager::TianBMSJsonManager()
{
}

/**
 * Build data from TianBMSData into json
 * 
 * @param[in]   tianBMSData TianBMSData object
 * 
 * @return      json formatted string
*/
String TianBMSJsonManager::buildData(const TianBMSData &tianBMSData)
{
    DynamicJsonDocument doc(3072);
    String output;
    doc["msg_count"] = tianBMSData.msgCount;
    doc["id"] = tianBMSData.id;
    doc["pcb_barcode"] = String(tianBMSData.pcbBarcode.data());
    doc["sn_code_1"] = String(tianBMSData.snCode1.data());
    doc["sn_code_2"] = String(tianBMSData.snCode2.data());

    JsonObject pack_voltage = doc.createNestedObject("pack_voltage");
    pack_voltage["unit"] = "V";
    pack_voltage["divider"] = 100;
    pack_voltage["value"] = tianBMSData.packVoltage;

    JsonObject pack_current = doc.createNestedObject("pack_current");
    pack_current["unit"] = "A";
    pack_current["divider"] = 100;
    pack_current["value"] = tianBMSData.packCurrent;

    JsonObject remaining_capacity = doc.createNestedObject("remaining_capacity");
    remaining_capacity["unit"] = "Ah";
    remaining_capacity["divider"] = 1;
    remaining_capacity["value"] = tianBMSData.remainingCapacity;

    JsonObject average_cell_temperature = doc.createNestedObject("average_cell_temperature");
    average_cell_temperature["unit"] = "Celcius";
    average_cell_temperature["divider"] = 10;
    average_cell_temperature["value"] = tianBMSData.avgCellTemperature;

    JsonObject env_temperature = doc.createNestedObject("env_temperature");
    env_temperature["unit"] = "Celcius";
    env_temperature["divider"] = 10;
    env_temperature["value"] = tianBMSData.envTemperature;

    JsonObject warning_flag = doc.createNestedObject("warning_flag");
    warning_flag["unit"] = "None";
    warning_flag["value"] = tianBMSData.warningFlag.value;
    warning_flag["cell_ov_alm"] = tianBMSData.warningFlag.bits.cell_ov_alarm;
    warning_flag["cell_uv_alm"] = tianBMSData.warningFlag.bits.cell_uv_alarm;
    warning_flag["pack_ov_alm"] = tianBMSData.warningFlag.bits.pack_ov_alarm;
    warning_flag["pack_uv_alm"] = tianBMSData.warningFlag.bits.pack_uv_alarm;
    warning_flag["chg_oc_alm"] = tianBMSData.warningFlag.bits.chg_oc_alarm;
    warning_flag["dchg_oc_alm"] = tianBMSData.warningFlag.bits.dchg_oc_alarm;
    warning_flag["bat_ot_alm"] = tianBMSData.warningFlag.bits.bat_ot_alarm;
    warning_flag["bat_ut_alm"] = tianBMSData.warningFlag.bits.bat_ut_alarm;
    warning_flag["env_ot_alm"] = tianBMSData.warningFlag.bits.env_ot_alarm;
    warning_flag["env_ut_alm"] = tianBMSData.warningFlag.bits.env_ut_alarm;
    warning_flag["mos_ot_alm"] = tianBMSData.warningFlag.bits.mos_ot_alarm;
    warning_flag["low_capacity"] = tianBMSData.warningFlag.bits.low_capacity;

    JsonObject protection_flag = doc.createNestedObject("protection_flag");
    protection_flag["unit"] = "None";
    protection_flag["value"] = tianBMSData.protectionFlag.value;
    protection_flag["cell_ov_prot"] = tianBMSData.protectionFlag.bits.cell_ov_prot;
    protection_flag["cell_uv_prot"] = tianBMSData.protectionFlag.bits.cell_uv_prot;
    protection_flag["pack_ov_prot"] = tianBMSData.protectionFlag.bits.pack_ov_prot;
    protection_flag["pack_uv_prot"] = tianBMSData.protectionFlag.bits.pack_uv_prot;
    protection_flag["short_prot"] = tianBMSData.protectionFlag.bits.short_prot;
    protection_flag["oc_prot"] = tianBMSData.protectionFlag.bits.oc_prot;
    protection_flag["chg_ot_prot"] = tianBMSData.protectionFlag.bits.chg_ot_prot;
    protection_flag["chg_ut_prot"] = tianBMSData.protectionFlag.bits.chg_ut_prot;
    protection_flag["dchg_ot_prot"] = tianBMSData.protectionFlag.bits.dchg_ot_prot;
    protection_flag["dchg_ut_prot"] = tianBMSData.protectionFlag.bits.dchg_ut_prot;

    JsonObject fault_status_flag = doc.createNestedObject("fault_status_flag");
    fault_status_flag["unit"] = "None";
    fault_status_flag["value"] = tianBMSData.faultStatusFlag.value;
    fault_status_flag["comm_sampling_fault"] = tianBMSData.faultStatusFlag.bits.comm_sampling_fault;
    fault_status_flag["temp_sensor_break"] = tianBMSData.faultStatusFlag.bits.temp_sensor_break;
    fault_status_flag["soc"] = tianBMSData.faultStatusFlag.bits.soc;
    fault_status_flag["sod"] = tianBMSData.faultStatusFlag.bits.sod;
    fault_status_flag["chg_mos"] = tianBMSData.faultStatusFlag.bits.chg_mos;
    fault_status_flag["dchg_mos"] = tianBMSData.faultStatusFlag.bits.dchg_mos;
    fault_status_flag["chg_limit_function"] = tianBMSData.faultStatusFlag.bits.chg_limit_function;

    JsonObject soc = doc.createNestedObject("soc");
    soc["unit"] = "%";
    soc["divider"] = 100;
    soc["value"] = tianBMSData.soc;

    JsonObject soh = doc.createNestedObject("soh");
    soh["unit"] = "%";
    soh["divider"] = 100;
    soh["value"] = tianBMSData.soh;

    JsonObject full_charged_cap = doc.createNestedObject("full_charged_cap");
    full_charged_cap["unit"] = "Ah";
    full_charged_cap["divider"] = 1;
    full_charged_cap["value"] = tianBMSData.fullChargedCap;

    JsonObject cycle_count = doc.createNestedObject("cycle_count");
    cycle_count["unit"] = "None";
    cycle_count["divider"] = 1;
    cycle_count["value"] = tianBMSData.cycleCount;

    JsonObject cell_voltage = doc.createNestedObject("cell_voltage");
    cell_voltage["unit"] = "mV";
    cell_voltage["divider"] = 1;

    JsonArray cell_voltage_value = cell_voltage.createNestedArray("value");
    for (size_t i = 0; i < tianBMSData.cellVoltage.size(); i++)
    {
        cell_voltage_value.add(tianBMSData.cellVoltage[i]);
    }
    
    JsonObject max_cell_voltage = doc.createNestedObject("max_cell_voltage");
    max_cell_voltage["unit"] = "mV";
    max_cell_voltage["divider"] = 1;
    max_cell_voltage["value"] = tianBMSData.maxCellVoltage;

    JsonObject min_cell_voltage = doc.createNestedObject("min_cell_voltage");
    min_cell_voltage["unit"] = "mV";
    min_cell_voltage["divider"] = 1;
    min_cell_voltage["value"] = tianBMSData.minCellVoltage;

    JsonObject cell_voltage_diff = doc.createNestedObject("cell_voltage_diff");
    cell_voltage_diff["unit"] = "mV";
    cell_voltage_diff["divider"] = 1;
    cell_voltage_diff["value"] = tianBMSData.cellVoltageDiff;

    JsonObject max_cell_temperature = doc.createNestedObject("max_cell_temperature");
    max_cell_temperature["unit"] = "Celcius";
    max_cell_temperature["divider"] = 10;
    max_cell_temperature["value"] = tianBMSData.maxCellTemp;

    JsonObject min_cell_temperature = doc.createNestedObject("min_cell_temperature");
    min_cell_temperature["unit"] = "Celcius";
    min_cell_temperature["divider"] = 10;
    min_cell_temperature["value"] = tianBMSData.minCellTemp;

    JsonObject fet_temperature = doc.createNestedObject("fet_temperature");
    fet_temperature["unit"] = "Celcius";
    fet_temperature["divider"] = 10;
    fet_temperature["value"] = tianBMSData.fetTemp;

    JsonObject cell_temperature = doc.createNestedObject("cell_temperature");
    cell_temperature["unit"] = "Celcius";
    cell_temperature["divider"] = 10;

    JsonArray cell_temperature_value = cell_temperature.createNestedArray("value");
    for (size_t i = 0; i < tianBMSData.cellTemperature.size(); i++)
    {
        cell_temperature_value.add(tianBMSData.cellTemperature[i]);
    }

    JsonObject balance_temperature = doc.createNestedObject("ambient_temperature");
    balance_temperature["unit"] = "Celcius";
    balance_temperature["divider"] = 10;
    balance_temperature["value"] = tianBMSData.ambientTemperature;

    JsonObject remaining_charge_time = doc.createNestedObject("remaining_charge_time");
    remaining_charge_time["unit"] = "Minutes";
    remaining_charge_time["divider"] = 1;
    remaining_charge_time["value"] = tianBMSData.remainChgTime;

    JsonObject remaining_discharge_time = doc.createNestedObject("remaining_discharge_time");
    remaining_discharge_time["unit"] = "Minutes";
    remaining_discharge_time["divider"] = 1;
    remaining_discharge_time["value"] = tianBMSData.remainDsgTime;

    serializeJson(doc, output);
    return output;
    
}

/**
 * Build empty data json
 * 
 * @return      json formatted string
*/
String TianBMSJsonManager::buildEmptyData()
{
    String output;
    StaticJsonDocument<16> doc;
    JsonArray data = doc.createNestedArray("data");
    serializeJson(doc, output);
    return output;
}

TianBMSJsonManager::~TianBMSJsonManager()
{
}