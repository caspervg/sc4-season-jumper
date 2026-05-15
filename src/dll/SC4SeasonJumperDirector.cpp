#include "SC4SeasonJumperDirector.hpp"

#include <cGZPersistResourceKey.h>
#include <cIGZCheatCodeManager.h>
#include <cIGZCOM.h>
#include <cIGZDate.h>
#include <cIGZFrameWork.h>
#include <cIGZMessage2.h>
#include <cIGZMessage2Standard.h>
#include <cIGZMessageServer2.h>
#include <cIGZPersistResourceManager.h>
#include <cIGZWin.h>
#include <cIGZWinKeyAcceleratorRes.h>
#include <cISC4App.h>
#include <cISC4Simulator.h>
#include <cISC4View3DWin.h>
#include <cRZBaseString.h>
#include <GZServPtrs.h>

#include "season/SeasonJumpIds.hpp"
#include "utils/Logger.h"
#include "utils/Settings.h"
#include "utils/VersionDetection.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>
#include <wil/win32_helpers.h>

namespace
{
    struct CheatDefinition
    {
        uint32_t id;
        const char* command;
    };

    constexpr CheatDefinition kCheats[] = {
        {SeasonJumper::Ids::CheatJumpSeason, "jumpseason"},
        {SeasonJumper::Ids::CheatJumpSpring, "jumpspring"},
        {SeasonJumper::Ids::CheatJumpSummer, "jumpsummer"},
        {SeasonJumper::Ids::CheatJumpFall, "jumpfall"},
        {SeasonJumper::Ids::CheatJumpWinter, "jumpwinter"},
    };

    [[nodiscard]] bool IsSeasonJumperCheat(const uint32_t cheatId) noexcept
    {
        for (const auto& cheat : kCheats) {
            if (cheat.id == cheatId) {
                return true;
            }
        }

        return false;
    }
}

SC4SeasonJumperDirector::SC4SeasonJumperDirector() = default;

SC4SeasonJumperDirector::~SC4SeasonJumperDirector() = default;

uint32_t SC4SeasonJumperDirector::GetDirectorID() const
{
    return SeasonJumper::Ids::Director;
}

bool SC4SeasonJumperDirector::OnStart(cIGZCOM* pCOM)
{
    cRZMessage2COMDirector::OnStart(pCOM);

    if (auto* framework = RZGetFrameWork()) {
        framework->AddHook(this);
    }

    return true;
}

bool SC4SeasonJumperDirector::PreFrameWorkInit()
{
    return true;
}

bool SC4SeasonJumperDirector::PreAppInit()
{
    return true;
}

bool SC4SeasonJumperDirector::PostAppInit()
{
    InitializeLogger_();

    LOG_INFO("SC4SeasonJumper {} starting", SC4_TEMPLATE_DLL_VERSION_LABEL);
    LOG_INFO("Detected game version: {}", VersionDetection::GetInstance().GetGameVersion());

    RegisterCityNotifications_();
    RegisterCheats_();

    return true;
}

bool SC4SeasonJumperDirector::PreAppShutdown()
{
    fastForward_.Cancel(this);
    return true;
}

bool SC4SeasonJumperDirector::PostAppShutdown()
{
    PreCityShutdown_();
    UnregisterCheats_();
    UnregisterCityNotifications_();

    if (auto* framework = RZGetFrameWork()) {
        framework->RemoveHook(this);
    }

    Logger::Shutdown();
    return true;
}

bool SC4SeasonJumperDirector::PostSystemServiceShutdown()
{
    return true;
}

bool SC4SeasonJumperDirector::AbortiveQuit()
{
    fastForward_.Cancel(this);
    return true;
}

bool SC4SeasonJumperDirector::OnInstall()
{
    return true;
}

bool SC4SeasonJumperDirector::DoMessage(cIGZMessage2* const pMsg)
{
    if (!pMsg) {
        return true;
    }

    switch (pMsg->GetType()) {
        case SeasonJumper::Ids::MessagePostCityInit:
            PostCityInit_();
            break;

        case SeasonJumper::Ids::MessagePreCityShutdown:
            PreCityShutdown_();
            break;

        case SeasonJumper::Ids::KeyShortcutMessage:
            OnHotkey_();
            break;

        case SeasonJumper::Ids::MessageDeferredPause:
            fastForward_.ConfirmPaused();
            break;

        case SeasonJumper::Ids::MessageCheatIssued: {
            const auto* const standardMessage = static_cast<cIGZMessage2Standard*>(pMsg);
            OnCheat_(static_cast<uint32_t>(standardMessage->GetData1()));
            break;
        }

        default:
            fastForward_.CheckArrival(this);
            break;
    }

    return true;
}

std::filesystem::path SC4SeasonJumperDirector::GetUserPluginsPath_()
{
    try {
        const auto modulePath = wil::GetModuleFileNameW(wil::GetModuleInstanceHandle());
        return std::filesystem::path(modulePath.get()).parent_path();
    }
    catch (const wil::ResultException&) {
        return {};
    }
}

void SC4SeasonJumperDirector::InitializeLogger_()
{
    const auto pluginsPath = GetUserPluginsPath_();
    const auto logPath = pluginsPath.parent_path();
    const auto settingsPath = pluginsPath / "SC4SeasonJumper.ini";

    Logger::Initialize("SC4SeasonJumper", logPath.string(), false);

    Settings settings;
    settings.Load(settingsPath);

    Logger::Shutdown();
    Logger::Initialize("SC4SeasonJumper", logPath.string(), settings.GetLogToFile());
    Logger::SetLevel(settings.GetLogLevel());

    LOG_INFO("Using settings file: {}", settingsPath.string());
}

void SC4SeasonJumperDirector::RegisterCityNotifications_()
{
    cIGZMessageServer2Ptr messageServer;
    if (!messageServer) {
        LOG_WARN("SeasonJumper: message server is not available");
        return;
    }

    messageServer->AddNotification(this, SeasonJumper::Ids::MessagePostCityInit);
    messageServer->AddNotification(this, SeasonJumper::Ids::MessagePreCityShutdown);
    messageServer_ = messageServer;
}

void SC4SeasonJumperDirector::UnregisterCityNotifications_()
{
    if (!messageServer_) {
        return;
    }

    UnregisterHotkey_();
    messageServer_->RemoveNotification(this, SeasonJumper::Ids::MessagePostCityInit);
    messageServer_->RemoveNotification(this, SeasonJumper::Ids::MessagePreCityShutdown);
    messageServer_.Reset();
}

void SC4SeasonJumperDirector::RegisterCheats_()
{
    const cISC4AppPtr app;
    if (!app) {
        LOG_WARN("SeasonJumper: SC4 app service is not available");
        return;
    }

    cIGZCheatCodeManager* const cheats = app->GetCheatCodeManager();
    if (!cheats) {
        LOG_WARN("SeasonJumper: cheat code manager is not available");
        return;
    }

    for (const auto& cheat : kCheats) {
        cheats->RegisterCheatCode(cheat.id, cRZBaseString(cheat.command));
    }

    cheats->AddNotification2(this, 0);
    cheatManager_ = cheats;

    LOG_INFO("SeasonJumper: cheat codes registered");
}

void SC4SeasonJumperDirector::UnregisterCheats_()
{
    if (!cheatManager_) {
        return;
    }

    for (const auto& cheat : kCheats) {
        cheatManager_->UnregisterCheatCode(cheat.id);
    }
    cheatManager_->RemoveNotification2(this, 0);
    cheatManager_.Reset();
}

void SC4SeasonJumperDirector::PostCityInit_()
{
    cityLoaded_ = true;
    RegisterHotkey_();
}

void SC4SeasonJumperDirector::PreCityShutdown_()
{
    fastForward_.Cancel(this);
    cityLoaded_ = false;
    UnregisterHotkey_();

    if (view3D_) {
        view3D_->Release();
        view3D_ = nullptr;
    }
}

void SC4SeasonJumperDirector::RegisterHotkey_()
{
    if (hotkeyRegistered_) {
        return;
    }

    const cISC4AppPtr app;
    if (!app) {
        return;
    }

    cIGZWin* const mainWindow = app->GetMainWindow();
    if (!mainWindow) {
        return;
    }

    cIGZWin* const sc4AppWindow = mainWindow->GetChildWindowFromID(SeasonJumper::Ids::WinSC4App);
    if (!sc4AppWindow) {
        return;
    }

    if (!sc4AppWindow->GetChildAs(
            SeasonJumper::Ids::WinSC4View3D,
            kGZIID_cISC4View3DWin,
            reinterpret_cast<void**>(&view3D_))) {
        LOG_WARN("SeasonJumper: failed to acquire the View3D window");
        return;
    }

    cIGZPersistResourceManagerPtr resourceManager;
    if (!resourceManager) {
        LOG_WARN("SeasonJumper: resource manager is not available");
        return;
    }

    cRZAutoRefCount<cIGZWinKeyAcceleratorRes> acceleratorResource;
    const cGZPersistResourceKey key(
        SeasonJumper::Ids::KeyConfigType,
        SeasonJumper::Ids::KeyConfigGroup,
        SeasonJumper::Ids::KeyConfigInstance);

    if (!resourceManager->GetPrivateResource(
            key,
            kGZIID_cIGZWinKeyAcceleratorRes,
            acceleratorResource.AsPPVoid(),
            0,
            nullptr)) {
        LOG_WARN(
            "SeasonJumper: key config DAT not found (TGI {:08X}/{:08X}/{:08X}); hotkey disabled",
            SeasonJumper::Ids::KeyConfigType,
            SeasonJumper::Ids::KeyConfigGroup,
            SeasonJumper::Ids::KeyConfigInstance);
        return;
    }

    acceleratorResource->RegisterResources(view3D_->GetKeyAccelerator());

    if (messageServer_) {
        messageServer_->AddNotification(this, SeasonJumper::Ids::KeyShortcutMessage);
        hotkeyRegistered_ = true;
    }

    LOG_INFO("SeasonJumper: hotkey registered");
}

void SC4SeasonJumperDirector::UnregisterHotkey_()
{
    if (!hotkeyRegistered_) {
        return;
    }

    if (messageServer_) {
        messageServer_->RemoveNotification(this, SeasonJumper::Ids::KeyShortcutMessage);
    }
    hotkeyRegistered_ = false;
}

void SC4SeasonJumperDirector::OnHotkey_()
{
    if (!cityLoaded_) {
        return;
    }

    if (fastForward_.IsRunning()) {
        fastForward_.Cancel(this);
        return;
    }

    const cISC4SimulatorPtr simulator;
    if (!simulator) {
        return;
    }

    cIGZDate* const currentDate = simulator->GetSimDate();
    if (!currentDate) {
        return;
    }

    fastForward_.Begin(this, SeasonJumper::GetNextHotkeySeason(currentDate->Month()));
}

void SC4SeasonJumperDirector::OnCheat_(const uint32_t cheatId)
{
    if (!IsSeasonJumperCheat(cheatId)) {
        return;
    }

    if (!cityLoaded_) {
        LOG_INFO("SeasonJumper: cheat ignored because no city is loaded");
        return;
    }

    switch (cheatId) {
        case SeasonJumper::Ids::CheatJumpSeason:
            OnHotkey_();
            break;

        case SeasonJumper::Ids::CheatJumpSpring:
            fastForward_.Begin(this, SeasonJumper::Season::Spring);
            break;

        case SeasonJumper::Ids::CheatJumpSummer:
            fastForward_.Begin(this, SeasonJumper::Season::Summer);
            break;

        case SeasonJumper::Ids::CheatJumpFall:
            fastForward_.Begin(this, SeasonJumper::Season::Fall);
            break;

        case SeasonJumper::Ids::CheatJumpWinter:
            fastForward_.Begin(this, SeasonJumper::Season::Winter);
            break;

        default:
            break;
    }
}
