#pragma once

#include <cstdint>

// Fixes a vanilla SimCity 4 bug where a seasonal prop placed mid-season stays
// invisible until the next time its start date comes around. The game's
// cSC4PropOccupant::SetSimulatorDateRange advances the season start date to the
// following year before testing whether "today" falls in range, so a prop
// created during its active season is configured as out-of-season and hidden.
//
// This affects painted props, plopped/grown lot props, and any 3D prop preview.
namespace SeasonalPropVisibilityFix
{
    // Installs the inline hook. Safe to call once, during app init.
    // - enabled:     value of the FixSeasonalPropVisibility INI option.
    // - gameVersion: minor build revision from VersionDetection (e.g. 641).
    // The patch is only applied for the supported build and when the expected
    // prologue bytes are present; otherwise it logs and no-ops.
    void Install(bool enabled, uint16_t gameVersion);
}
