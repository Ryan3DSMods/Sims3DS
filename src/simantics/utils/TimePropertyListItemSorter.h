/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// FSO.SimAntics.Utils.TimePropertyListItemSorter
#pragma once
#include "vitaboy/Animation.h"
namespace FSO { namespace SimAntics { namespace Utils {
class TimePropertyListItemSorter {
public:
    int Compare(const FSO::Vitaboy::TimePropertyListItem& x, const FSO::Vitaboy::TimePropertyListItem& y) const {
        return (x.ID < y.ID) ? -1 : ((x.ID > y.ID) ? 1 : 0);
    }
    bool operator()(const FSO::Vitaboy::TimePropertyListItem& x, const FSO::Vitaboy::TimePropertyListItem& y) const { return x.ID < y.ID; }
};
}}}
