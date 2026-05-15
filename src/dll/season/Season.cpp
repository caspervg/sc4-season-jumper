#include "Season.hpp"

namespace
{
    constexpr const char* kMonthNames[] = {
        "",
        "Jan",
        "Feb",
        "Mar",
        "Apr",
        "May",
        "Jun",
        "Jul",
        "Aug",
        "Sep",
        "Oct",
        "Nov",
        "Dec",
    };
}

namespace SeasonJumper
{
    const char* GetMonthName(const uint32_t month) noexcept
    {
        if (month >= 1 && month <= 12) {
            return kMonthNames[month];
        }

        return "Unknown";
    }

    const char* GetSeasonName(const Season season) noexcept
    {
        switch (season) {
            case Season::Spring: return "spring";
            case Season::Summer: return "summer";
            case Season::Fall: return "fall";
            case Season::Winter: return "winter";
            default: return "unknown";
        }
    }

    uint32_t GetStartMonth(const Season season) noexcept
    {
        switch (season) {
            case Season::Spring: return 3;
            case Season::Summer: return 6;
            case Season::Fall: return 9;
            case Season::Winter: return 12;
            default: return 9;
        }
    }

    Season GetNextHotkeySeason(const uint32_t currentMonth) noexcept
    {
        return currentMonth >= 9 ? Season::Summer : Season::Fall;
    }
}
