/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// FSO.SimAntics.Model.VMPlacementError
#pragma once
namespace FSO { namespace SimAntics { namespace Model {
enum class VMPlacementError : int {
    Success = -1,
    // NOTE: Each of the below corresponds to a string in cst 137: placement_errors
    // Don't change the names or order of these!
    LocationOutOfBounds = 0, LevelOutOfBounds = 1, MustBeAtTileCenter = 2, MustBeInCorner = 3, CantBeInCorner = 4,
    MustBeAgainstWall = 5, CantBeThroughWall = 6, MustBeOnUpstairsFloorHole = 7, MustBeOutside = 8, MustBeInside = 9,
    CantIntersectOtherObjects = 10, MustBeOnGround = 11, CantFindSlot = 12, CantSupportWeight = 13,
    CantSupportSize = 14, CantPlaceOnTop = 15, CantPlaceOnSlope = 16, CantPlaceInAir = 17, NotAllowedOnFloor = 18,
    NotAllowedOnTerrain = 19, CantAfford = 20, MustBeAgainstUnusedWall = 21, MustBeOnDiagonal = 22,
    HeightNotAllowed = 23, InsufficientFunds = 24, SpecialShowMoneyError = 25, HasWater = 26, NoWater = 27,
    CantPlaceOnWater = 28, MustPlaceOnWater = 29, MustPlaceOnPool = 30, MustBeOnFirstLevel = 31,
    // wall-specific
    Floor2NeedsSupport = 32, MustRemoveObjectsOnWall = 33,
    // wallpaper-specific
    MustBeAgainst2ndFloorWall = 34, CannotDeleteObject = 35, ObjectNotOwnedByYou = 36, CounterHeight = 37,
    CantBePickedup = 38, InUse = 39, CantBePickedupOutOfBounds = 40, CantEffectFirstLevelFromSecondLevel = 41,
    CantPlaceInAirOtherLevel = 42, NotAllowedOnFloorOtherLevel = 43, CannotPlaceComputerOnEndTable = 44,
    CannotDeletePoolWhilePeopleAreInIt = 45, MustRemoveLadderOrDivingBoardBeforeDeletingPool = 46,
    CannotWallpaperFence = 47, MustHaveMoreSpaceFromCeiling = 48, TooManyObjectsOnTheLot = 49,
    CantPlaceOnForSaleObject = 50
};
}}}
