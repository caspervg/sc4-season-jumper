#include "FastForwardController.hpp"

#include <cIGZDate.h>
#include <cIGZMessageServer2.h>
#include <cISC4Simulator.h>
#include <cRZBaseString.h>
#include <cRZMessage2Standard.h>
#include <GZServPtrs.h>

#include "CityContext.hpp"
#include "SeasonJumpIds.hpp"
#include "utils/Logger.h"

namespace
{
    constexpr int32_t kFastestSimSpeed = 2;

    [[nodiscard]] cISC4Simulator::eAgentFlags GetAgentFlags() noexcept
    {
        return static_cast<cISC4Simulator::eAgentFlags>(
            cISC4Simulator::AgentFlagEnabledForUnestablishedCities |
            cISC4Simulator::AgentFlagEnabledForEstablishedCities);
    }

    [[nodiscard]] bool RequestPause(cISC4Simulator* const simulator)
    {
        if (!simulator) {
            return false;
        }

        simulator->SetSimSpeed(0);
        simulator->Pause();
        if (simulator->IsAnyPaused()) {
            return true;
        }

        simulator->Pause();
        return simulator->IsAnyPaused();
    }

    void PostDeferredPauseCheck(cIGZMessageTarget2* const target)
    {
        if (!target) {
            return;
        }

        const cIGZMessageServer2Ptr messageServer;
        if (!messageServer) {
            return;
        }

        auto* const message = new cRZMessage2Standard();
        message->AddRef();
        message->SetType(SeasonJumper::Ids::MessageDeferredPause);
        messageServer->GeneralMessagePostToTarget(
            static_cast<cIGZMessage2*>(static_cast<cIGZMessage2Standard*>(message)),
            target);
        message->Release();
    }
}

namespace SeasonJumper
{
    bool FastForwardController::IsRunning() const noexcept
    {
        return agentRegistered_;
    }

    void FastForwardController::Begin(cIGZMessageTarget2* const agent, const Season season)
    {
        Begin(agent, GetStartMonth(season));
    }

    void FastForwardController::Begin(cIGZMessageTarget2* const agent, const uint32_t targetMonth)
    {
        if (!agent || targetMonth < 1 || targetMonth > 12) {
            return;
        }

        if (agentRegistered_) {
            Cancel(agent);
        }

        const cISC4SimulatorPtr simulator;
        if (!simulator) {
            LOG_WARN("SeasonJumper: simulator service is not available");
            return;
        }

        cIGZDate* const currentDate = simulator->GetSimDate();
        if (!currentDate) {
            LOG_WARN("SeasonJumper: simulator date is not available");
            return;
        }

        const uint32_t currentYear = currentDate->Year();
        const uint32_t currentDayNumber = currentDate->DayNumber();

        uint32_t targetYear = currentYear;
        uint32_t targetDayNumber = currentDate->Jday(targetMonth, 1, targetYear);
        if (targetDayNumber <= currentDayNumber) {
            ++targetYear;
            targetDayNumber = currentDate->Jday(targetMonth, 1, targetYear);
        }

        cRZBaseString agentName("SC4SeasonJumper");
        if (!simulator->AddAgent(agent, cISC4Simulator::AgentTypeSimNewDay, agentName, GetAgentFlags())) {
            LOG_WARN("SeasonJumper: failed to register the daily simulator agent");
            return;
        }

        agentRegistered_ = true;
        targetDayNumber_ = targetDayNumber;
        targetMonth_ = targetMonth;
        targetYear_ = targetYear;

        if (simulator->IsAnyPaused()) {
            simulator->Resume();
        }
        simulator->SetSimSpeed(kFastestSimSpeed);

        LOG_INFO(
            "SeasonJumper: fast-forwarding {} to {} 1, {} ({} days ahead)",
            GetCurrentCityName(),
            GetMonthName(targetMonth_),
            targetYear_,
            targetDayNumber_ - currentDayNumber);
    }

    void FastForwardController::Cancel(cIGZMessageTarget2* const agent)
    {
        if (!agentRegistered_) {
            return;
        }

        const cISC4SimulatorPtr simulator;
        bool paused = false;
        if (simulator && agent) {
            simulator->RemoveAgent(agent, cISC4Simulator::AgentTypeSimNewDay);
            paused = RequestPause(simulator);
        }

        agentRegistered_ = false;
        targetDayNumber_ = 0;
        targetMonth_ = 0;
        targetYear_ = 0;

        if (paused) {
            LOG_INFO("SeasonJumper: fast-forward cancelled in {}", GetCurrentCityName());
        }
        else {
            LOG_WARN("SeasonJumper: fast-forward cancelled in {}, but SC4 did not report a paused state", GetCurrentCityName());
        }
        PostDeferredPauseCheck(agent);
    }

    void FastForwardController::CheckArrival(cIGZMessageTarget2* const agent)
    {
        if (!agentRegistered_) {
            return;
        }

        const cISC4SimulatorPtr simulator;
        if (!simulator) {
            return;
        }

        cIGZDate* const currentDate = simulator->GetSimDate();
        if (!currentDate || currentDate->DayNumber() < targetDayNumber_) {
            return;
        }

        simulator->RemoveAgent(agent, cISC4Simulator::AgentTypeSimNewDay);
        const bool paused = RequestPause(simulator);

        if (paused) {
            LOG_INFO(
                "SeasonJumper: {} arrived at {} 1, {}, paused",
                GetCurrentCityName(),
                GetMonthName(currentDate->Month()),
                currentDate->Year());
        }
        else {
            LOG_WARN(
                "SeasonJumper: {} arrived at {} 1, {}, but SC4 did not report a paused state",
                GetCurrentCityName(),
                GetMonthName(currentDate->Month()),
                currentDate->Year());
        }
        PostDeferredPauseCheck(agent);

        agentRegistered_ = false;
        targetDayNumber_ = 0;
        targetMonth_ = 0;
        targetYear_ = 0;
    }

    void FastForwardController::ConfirmPaused()
    {
        const cISC4SimulatorPtr simulator;
        if (!simulator || simulator->IsAnyPaused()) {
            return;
        }

        if (RequestPause(simulator)) {
            LOG_INFO("SeasonJumper: deferred pause applied in {}", GetCurrentCityName());
        }
        else {
            LOG_WARN("SeasonJumper: deferred pause failed in {}", GetCurrentCityName());
        }
    }
}
