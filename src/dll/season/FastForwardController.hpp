#pragma once

#include <cstdint>

#include "Season.hpp"

class cIGZMessageTarget2;

namespace SeasonJumper
{
    class FastForwardController
    {
    public:
        [[nodiscard]] bool IsRunning() const noexcept;

        void Begin(cIGZMessageTarget2* agent, Season season);
        void Begin(cIGZMessageTarget2* agent, uint32_t targetMonth);
        void Cancel(cIGZMessageTarget2* agent);
        void CheckArrival(cIGZMessageTarget2* agent);
        void ConfirmPaused();

    private:
        bool agentRegistered_ = false;
        uint32_t targetDayNumber_ = 0;
        uint32_t targetMonth_ = 0;
        uint32_t targetYear_ = 0;
    };
}
