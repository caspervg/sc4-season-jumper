#pragma once

#include <cIGZCheatCodeManager.h>
#include <cIGZMessageServer2.h>
#include <cRZMessage2COMDirector.h>
#include <cRZAutoRefCount.h>
#include <filesystem>

#include "season/FastForwardController.hpp"

class cIGZMessage2;
class cIGZCOM;
class cISC4View3DWin;

class SC4SeasonJumperDirector final : public cRZMessage2COMDirector
{
public:
    SC4SeasonJumperDirector();
    ~SC4SeasonJumperDirector() override;

    [[nodiscard]] uint32_t GetDirectorID() const override;
    bool OnStart(cIGZCOM* pCOM) override;
    bool PreFrameWorkInit() override;
    bool PreAppInit() override;
    bool PostAppInit() override;
    bool PreAppShutdown() override;
    bool PostAppShutdown() override;
    bool PostSystemServiceShutdown() override;
    bool AbortiveQuit() override;
    bool OnInstall() override;
    bool DoMessage(cIGZMessage2* pMsg) override;

private:
    static std::filesystem::path GetUserPluginsPath_();
    void InitializeLogger_();
    void RegisterCityNotifications_();
    void UnregisterCityNotifications_();
    void RegisterCheats_();
    void UnregisterCheats_();
    void PostCityInit_();
    void PreCityShutdown_();
    void RegisterHotkey_();
    void UnregisterHotkey_();
    void OnHotkey_();
    void OnCheat_(uint32_t cheatId);

private:
    bool cityLoaded_ = false;
    bool hotkeyRegistered_ = false;
    cISC4View3DWin* view3D_ = nullptr;
    cRZAutoRefCount<cIGZMessageServer2> messageServer_;
    cRZAutoRefCount<cIGZCheatCodeManager> cheatManager_;
    SeasonJumper::FastForwardController fastForward_;
};
