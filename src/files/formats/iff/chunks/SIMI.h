/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// Translated from FreeSims sims.files/formats/iff/chunks/SIMI.cs
#pragma once
#include "files/formats/iff/AbstractIffChunk.h"

namespace FSO { namespace Files { namespace Formats { namespace IFF { namespace Chunks {

class SIMI : public IffChunk
{
public:
    IFF_CHUNK_TYPE("SIMI")

    uint32_t Version = 0;
    std::vector<int16_t> GlobalData;
    
    int16_t Unknown1 = 0;
    int32_t Unknown2 = 0;
    int32_t Unknown3 = 0;
    int32_t GUID1 = 0;
    int32_t GUID2 = 0;
    int32_t Unknown4 = 0;
    int32_t LotValue = 0;
    int32_t ObjectsValue = 0;
    int32_t ArchitectureValue = 0;

    class SIMIBudgetDay
    {
    public:
        int32_t Valid = 0;
        int32_t MiscIncome = 0;
        int32_t JobIncome = 0;

        int32_t ServiceExpense = 0;
        int32_t FoodExpense = 0;
        int32_t BillsExpense = 0;

        int32_t MiscExpense = 0;
        int32_t HouseholdExpense = 0;
        int32_t ArchitectureExpense = 0;

        SIMIBudgetDay()
        {

        }

        explicit SIMIBudgetDay(st::Reader& io)
        {
            Valid = io.ReadInt32();
            if (Valid == 0) return;
            MiscIncome = io.ReadInt32();
            JobIncome = io.ReadInt32();
            ServiceExpense = io.ReadInt32();
            FoodExpense = io.ReadInt32();
            BillsExpense = io.ReadInt32();
            MiscExpense = io.ReadInt32();
            HouseholdExpense = io.ReadInt32();
            ArchitectureExpense = io.ReadInt32();
        }

        void Write(st::Writer& io)
        {
            io.WriteInt32(Valid);
            if (Valid == 0) return;
            io.WriteInt32(MiscIncome);
            io.WriteInt32(JobIncome);
            io.WriteInt32(ServiceExpense);
            io.WriteInt32(FoodExpense);
            io.WriteInt32(BillsExpense);
            io.WriteInt32(MiscIncome); //(sic) C# writes MiscIncome in place of MiscExpense
            io.WriteInt32(HouseholdExpense);
            io.WriteInt32(ArchitectureExpense);
        }
    };

    std::vector<SIMIBudgetDay> BudgetDays;

    int32_t PurchaseValue() const
    {
        return LotValue + ObjectsValue + (ArchitectureValue * 7) / 10;
    }

    void Read(IffFile* iff, st::Reader& stream) override;

    bool Write(IffFile* iff, st::Writer& stream) override;
};

}}}}} // namespace FSO::Files::Formats::IFF::Chunks
