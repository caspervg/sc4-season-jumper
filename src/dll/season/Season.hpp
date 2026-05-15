#pragma once

#include <cstdint>

namespace SeasonJumper
{
    enum class Season : uint8_t
    {
        Spring,
        Summer,
        Fall,
        Winter,
    };

    [[nodiscard]] const char* GetMonthName(uint32_t month) noexcept;
    [[nodiscard]] const char* GetSeasonName(Season season) noexcept;
    [[nodiscard]] uint32_t GetStartMonth(Season season) noexcept;
    [[nodiscard]] Season GetNextHotkeySeason(uint32_t currentMonth) noexcept;
}
