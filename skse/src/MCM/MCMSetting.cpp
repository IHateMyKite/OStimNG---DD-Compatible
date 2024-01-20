#include "MCMSetting.h"

namespace MCM {
    MCMSetting::MCMSetting() {}

    MCMSetting::MCMSetting(float defaultValue, std::string exportValue) : defaultValue{defaultValue}, exportValue {exportValue}{}

    void MCMSetting::setup(uint32_t formID) {
        globalVariable = RE::TESDataHandler::GetSingleton()->LookupForm<RE::TESGlobal>(formID, "OStim.esp");
    }

    void MCMSetting::restoreDefault() {
        if (globalVariable)
            globalVariable->value = defaultValue;
    }

    float MCMSetting::asFloat() {
        return globalVariable ? globalVariable->value : 0.0f;
    }

    int MCMSetting::asInt() {
        return globalVariable ? static_cast<int>(globalVariable->value) : 0;
    }

    bool MCMSetting::asBool() {
        return globalVariable ? globalVariable->value != 0 : false;
    }

    void MCMSetting::exportSetting(json& json) {
        json[exportValue] = globalVariable ? globalVariable->value : 0;
    }

    void MCMSetting::importSetting(json& json) {
        if (!globalVariable) return;
        if (json.contains(exportValue)) {
            globalVariable->value = json[exportValue];
        } else {
            globalVariable->value = defaultValue;
        }
    }
}