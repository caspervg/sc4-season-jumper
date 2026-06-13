#include "SeasonalPropVisibilityFix.hpp"

#include <array>
#include <cstring>

#include <cIGZDate.h>
#include <cISC4Simulator.h>
#include <GZServPtrs.h>

#include "../utils/Logger.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>

// All addresses are absolute VAs for the SimCity 4.exe 1.1.641 build
// (image base 0x00400000). The hook is gated on that build in Install().
namespace
{
    // cSC4PropOccupant::SetSimulatorDateRange(interval, duration, month, day).
    // __thiscall, callee-cleaned (ret 0x10). Prologue (8 bytes, 3 instructions):
    //   83 EC 10           sub   esp, 0x10
    //   53                 push  ebx
    //   8B 5C 24 18        mov   ebx, [esp+0x18]
    constexpr uintptr_t kSetSimulatorDateRangeAddr = 0x005EEFF0;
    constexpr size_t kPatchSize = 8;     // whole instructions overwritten by the jmp
    constexpr uintptr_t kResumeAddr = kSetSimulatorDateRangeAddr + kPatchSize;

    constexpr std::array<uint8_t, kPatchSize> kExpectedPrologue = {
        0x83, 0xEC, 0x10, 0x53, 0x8B, 0x5C, 0x24, 0x18};

    // cSC4PropOccupant primary vtable slots (mirrored from SetSimulatorDateRange).
    constexpr int kSlotGetPropTimerState = 0x50; // uint8_t GetPropTimerState(mask)
    constexpr int kSlotSetPropTimerState = 0x54; // void SetPropTimerState(mask, state)

    // Bit 2 of the prop timer bitfield is the simulator-date visibility flag.
    constexpr uint8_t kSimulatorDateTimerMask = 0x2;

    // Layout of the DateInfo block the game allocates at cSC4PropOccupant+0x54.
    // Each cGZDate is a multiple-inheritance object: day-number at object+0x04.
    constexpr int kPropDateInfoPtr = 0x54;
    constexpr int kDateInfoInterval = 0x00; // uint32 interval, in days
    constexpr int kDateInfoStartDay = 0x0C; // int32 start  date day-number
    constexpr int kDateInfoEndDay = 0x1C;   // int32 end    date day-number

    using GetTimerStateFn = uint8_t(__thiscall*)(void* self, uint8_t mask);
    using SetTimerStateFn = void(__thiscall*)(void* self, uint8_t mask, bool state);

    template <int Slot, class Fn>
    Fn VtableEntry(void* obj)
    {
        void** vtable = *reinterpret_cast<void***>(obj);
        return reinterpret_cast<Fn>(vtable[Slot / sizeof(void*)]);
    }

    void* g_originalTrampoline = nullptr;
}

// Runs after the original SetSimulatorDateRange has built the DateInfo block.
// If the prop was configured out-of-season but "today" actually lies within the
// current active occurrence, make it visible and pull the end date back to the
// current occurrence so it still hides at the real season end.
extern "C" void __fastcall SeasonalPropVisibilityFix_PostCorrect(void* propOccupant)
{
    if (!propOccupant) {
        return;
    }

    auto* dateInfo = *reinterpret_cast<char**>(static_cast<char*>(propOccupant) + kPropDateInfoPtr);
    if (!dateInfo) {
        return; // prop has no recurring date range (interval was 0)
    }

    const auto interval = *reinterpret_cast<uint32_t*>(dateInfo + kDateInfoInterval);
    if (interval == 0) {
        return;
    }

    // Already in-phase (placed on/before the start date): the game got it right.
    if (VtableEntry<kSlotGetPropTimerState, GetTimerStateFn>(propOccupant)(
            propOccupant, kSimulatorDateTimerMask)) {
        return;
    }

    const cISC4SimulatorPtr simulator;
    if (!simulator) {
        return;
    }

    cIGZDate* const today = simulator->GetSimDate();
    if (!today) {
        return;
    }

    // The stored start/end dates describe the NEXT occurrence (the game bumped
    // the start past today). The currently-active occurrence is one interval
    // earlier. cGZDate stores its day-number at object+0x04; the cIGZDate
    // interface pointer points at object+0x08, so the value is at today-0x04.
    const int32_t todayDay = *reinterpret_cast<int32_t*>(reinterpret_cast<char*>(today) - 0x04);
    const auto storedEndDay = *reinterpret_cast<int32_t*>(dateInfo + kDateInfoEndDay);
    const int32_t activeStartDay =
        *reinterpret_cast<int32_t*>(dateInfo + kDateInfoStartDay) - static_cast<int32_t>(interval);
    const int32_t activeEndDay = storedEndDay - static_cast<int32_t>(interval);

    if (todayDay < activeStartDay || todayDay > activeEndDay) {
        return; // genuinely out of season; leave the game's configuration alone
    }

    // Pull the end date back to the active occurrence so the prop hides at the
    // real season end. The start date stays at the next occurrence, which is the
    // invariant TickSimulatorDate expects while a prop is visible.
    *reinterpret_cast<int32_t*>(dateInfo + kDateInfoEndDay) = activeEndDay;

    VtableEntry<kSlotSetPropTimerState, SetTimerStateFn>(propOccupant)(
        propOccupant, kSimulatorDateTimerMask, true);
}

namespace
{
    // Calls the original via the trampoline, then post-corrects. The original is
    // __thiscall / callee-cleaned (ret 0x10), so we re-push its four arguments
    // before calling the trampoline and clean them ourselves on the way out.
    __declspec(naked) void DetourSetSimulatorDateRange()
    {
        __asm {
            push ecx                    // save 'this'
            push dword ptr [esp+0x14]   // re-push day
            push dword ptr [esp+0x14]   // re-push month
            push dword ptr [esp+0x14]   // re-push duration
            push dword ptr [esp+0x14]   // re-push interval
            mov  ecx, [esp+0x10]        // 'this'
            mov  eax, g_originalTrampoline
            call eax                    // original cleans the 4 re-pushed args
            mov  ecx, [esp]             // 'this'
            call SeasonalPropVisibilityFix_PostCorrect
            pop  ecx
            ret  0x10                   // clean the caller's 4 args
        }
    }

    bool PrologueMatches()
    {
        return std::memcmp(reinterpret_cast<void*>(kSetSimulatorDateRangeAddr),
                           kExpectedPrologue.data(), kPatchSize) == 0;
    }

    bool BuildTrampoline()
    {
        // 8 saved prologue bytes + a 5-byte jmp back to the resume point.
        auto* trampoline = static_cast<uint8_t*>(
            VirtualAlloc(nullptr, kPatchSize + 5, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE));
        if (!trampoline) {
            return false;
        }

        std::memcpy(trampoline, reinterpret_cast<void*>(kSetSimulatorDateRangeAddr), kPatchSize);
        trampoline[kPatchSize] = 0xE9; // jmp rel32
        const auto rel = static_cast<int32_t>(
            kResumeAddr - (reinterpret_cast<uintptr_t>(trampoline) + kPatchSize + 5));
        std::memcpy(trampoline + kPatchSize + 1, &rel, sizeof(rel));

        FlushInstructionCache(GetCurrentProcess(), trampoline, kPatchSize + 5);
        g_originalTrampoline = trampoline;
        return true;
    }

    bool WriteJumpToDetour()
    {
        auto* const target = reinterpret_cast<uint8_t*>(kSetSimulatorDateRangeAddr);

        DWORD oldProtect = 0;
        if (!VirtualProtect(target, kPatchSize, PAGE_EXECUTE_READWRITE, &oldProtect)) {
            return false;
        }

        std::array<uint8_t, kPatchSize> patch{};
        patch.fill(0x90); // pad the remaining bytes with nop
        patch[0] = 0xE9;  // jmp rel32
        const auto rel = static_cast<int32_t>(reinterpret_cast<uintptr_t>(&DetourSetSimulatorDateRange) -
                                              (kSetSimulatorDateRangeAddr + 5));
        std::memcpy(patch.data() + 1, &rel, sizeof(rel));
        std::memcpy(target, patch.data(), kPatchSize);

        VirtualProtect(target, kPatchSize, oldProtect, &oldProtect);
        FlushInstructionCache(GetCurrentProcess(), target, kPatchSize);
        return true;
    }
}

namespace SeasonalPropVisibilityFix
{
    void Install(const bool enabled, const uint16_t gameVersion)
    {
        if (!enabled) {
            LOG_INFO("Seasonal prop visibility fix disabled via INI");
            return;
        }

        if (gameVersion != 641) {
            LOG_WARN("Seasonal prop visibility fix supports game version 641; detected {}. Skipping.",
                     gameVersion);
            return;
        }

        if (!PrologueMatches()) {
            LOG_WARN("Seasonal prop visibility fix: unexpected bytes at {:#010x}; skipping to avoid a crash",
                     kSetSimulatorDateRangeAddr);
            return;
        }

        if (!BuildTrampoline() || !WriteJumpToDetour()) {
            LOG_ERROR("Seasonal prop visibility fix: failed to install the hook");
            return;
        }

        LOG_INFO("Seasonal prop visibility fix installed");
    }
}
