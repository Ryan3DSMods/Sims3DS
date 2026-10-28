/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// FSO.SimAntics.Engine.TSOTransaction.VMTSOStandaloneDatabase
// STUB(3ds): in-memory only (C# persisted to a file with BinaryWriter).
#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
namespace FSO { namespace SimAntics { namespace Engine { namespace TSOTransaction {
class VMTSOStandaloneDatabase {
public:
    static const int32_t CURRENT_VERSION = 2;
    int32_t Version = CURRENT_VERSION;
    std::unordered_map<std::string, uint32_t> IpNameToPersist;
    std::unordered_set<uint32_t> TakenAvatarPersist; //COPY of above.Values, range 65536-int.maxvalue-1
    std::unordered_set<uint32_t> Administrators;
    std::unordered_map<uint32_t, std::unordered_map<uint32_t, std::vector<uint8_t>>> PluginStorage;

    void Save() {} // STUB(3ds)
    uint32_t FindOrAddAvatar(const std::string& idString) {
        auto it = IpNameToPersist.find(idString);
        if (it != IpNameToPersist.end()) return it->second;
        uint32_t newID = 65537;
        while (TakenAvatarPersist.count(newID)) newID++;
        IpNameToPersist[idString] = newID;
        TakenAvatarPersist.insert(newID);
        Save();
        return newID;
    }
    void SavePluginPersist(uint32_t obj, uint32_t plugin, const std::vector<uint8_t>& data) {
        PluginStorage[obj][plugin] = data;
        Save();
    }
    const std::vector<uint8_t>* LoadPluginPersist(uint32_t obj, uint32_t plugin) {
        auto o = PluginStorage.find(obj);
        if (o == PluginStorage.end()) return nullptr;
        auto p = o->second.find(plugin);
        return (p == o->second.end()) ? nullptr : &p->second;
    }
};
}}}}
