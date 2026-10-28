/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// FSO.Common.Model.DynamicTuning (model/DynamicTuning.cs). Binary (de)serialization is dropped.
#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include <unordered_map>
namespace FSO { namespace Common { namespace Model {

class DynTuningEntry {
public:
    std::string tuning_type;
    int32_t tuning_table = 0;
    int32_t tuning_index = 0;
    float value = 0;
};

class DynamicTuning {
public:
    //string type/iff, int table, int index.
    std::unordered_map<std::string, std::unordered_map<int32_t, std::unordered_map<int32_t, float>>> Tuning;
    static constexpr int32_t CURRENT_VERSION = 0;
    int32_t Version = CURRENT_VERSION;

    DynamicTuning() {}
    explicit DynamicTuning(const std::vector<DynTuningEntry>& entries) {
        for (auto& entry : entries) AddTuning(entry);
    }
    DynamicTuning(const DynamicTuning& old) : Tuning(old.Tuning), Version(old.Version) {}

    void AddTuning(const DynTuningEntry& entry) {
        Tuning[entry.tuning_type][entry.tuning_table][entry.tuning_index] = entry.value;
    }

    std::unordered_map<int32_t, float>* GetTable(const std::string& type, int32_t table) {
        auto tables = Tuning.find(type);
        if (tables != Tuning.end()) {
            auto data = tables->second.find(table);
            if (data != tables->second.end()) return &data->second;
        }
        return nullptr;
    }

    // C# float? -> returns false when missing
    bool GetTuning(const std::string& type, int32_t table, int32_t index, float& result) {
        auto data = GetTable(type, table);
        if (data != nullptr) {
            auto it = data->find(index);
            if (it != data->end()) { result = it->second; return true; }
        }
        return false;
    }
};
}}}
namespace FSO { namespace SimAntics { namespace Model { using FSO::Common::Model::DynamicTuning; using FSO::Common::Model::DynTuningEntry; }}}
