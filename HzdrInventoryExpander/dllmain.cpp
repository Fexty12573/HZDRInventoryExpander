
#include "framework.h"
#include "extern/json.hpp"
#include "extern/safetyhook.hpp"
#include "PatternScan.h"
#include "Types.h"

#include <fstream>
#include <unordered_map>
#include <print>
#include <combaseapi.h>

template<>
struct std::hash<GUID> {
    std::size_t operator()(const GUID& g) const noexcept {
        using std::size_t;
        using std::hash;

        return hash<uint32_t>()(g.Data1) ^ hash<uint16_t>()(g.Data2) ^ hash<uint16_t>()(g.Data3) ^ hash<uint64_t>()(*(const uint64_t*)&g.Data4);
    }
};

template<>
struct std::formatter<GUID> : std::formatter<std::string> {
    template<typename FormatContext>
    auto format(const GUID& g, FormatContext& ctx) const {
        return std::formatter<std::string>::format(std::format("{:08X}-{:04X}-{:04X}-{:02X}{:02X}-{:02X}{:02X}{:02X}{:02X}{:02X}{:02X}",
            g.Data1, g.Data2, g.Data3, g.Data4[0], g.Data4[1], g.Data4[2], g.Data4[3], g.Data4[4], g.Data4[5], g.Data4[6], g.Data4[7]), ctx);
    }
};

std::ostream& operator<<(std::ostream& os, const GUID& g) {
    os << std::format("{:08X}-{:04X}-{:04X}-{:02X}{:02X}-{:02X}{:02X}{:02X}{:02X}{:02X}{:02X}",
        g.Data1, g.Data2, g.Data3, g.Data4[0], g.Data4[1], g.Data4[2], g.Data4[3], g.Data4[4], g.Data4[5], g.Data4[6], g.Data4[7]);
    return os;
}

namespace {

struct InventoryUpgrade {
    int weapons = 0;
    int mods = 0;
    int outfits = 0;
    int resources = 0;
    int tools = 0;
};

safetyhook::InlineHook hook1{};
safetyhook::InlineHook hook2{};
std::unordered_map<GUID, InventoryUpgrade> upgrades{};
InventoryUpgrade base_upgrade{};

void apply_capacity_upgrade(InventoryCapacityModificationComponent* component) {
    const auto upgrade = upgrades.find(component->resource->guid);
    if (upgrade == upgrades.end()) {
        std::println("No upgrade found for {}", component->resource->guid);
        return hook1.call(component);
    }

    std::println("Applying upgrade to {}", component->resource->guid);

    component->resource->weapon_capacity_increase = upgrade->second.weapons;
    component->resource->mod_capacity_increase = upgrade->second.mods;
    component->resource->outfit_capacity_increase = upgrade->second.outfits;
    component->resource->resource_capacity_increase = upgrade->second.resources;

    return hook1.call(component);
}

void apply_base_capacity(InventoryCapacityComponent* component) {
    component->resource->weapon_capacity = base_upgrade.weapons;
    component->resource->mod_capacity = base_upgrade.mods;
    component->resource->outfit_capacity = base_upgrade.outfits;
    component->resource->resource_capacity = base_upgrade.resources;
    component->resource->tool_capacity = base_upgrade.tools;
    return hook2.call(component);
}

void load() {
#ifdef _DEBUG
    AllocConsole();
    freopen_s((FILE**)stdout, "CONOUT$", "w", stdout);
#endif

    auto pattern = Pattern::from_string("48 8B 59 30 33 FF C6 41 38 01 48 8B E9 48 8B 43 48 48 85 C0 75 05");
    auto result = PatternScanner::find_first(pattern);
    if (!result) {
        MessageBoxA(nullptr, "Failed to find pattern 1", "Inventory Expander Error", MB_ICONERROR);
        return;
    }

    std::println("Found Hook 1 address at {:X}", result - 20);
    hook1 = safetyhook::create_inline(result - 20, apply_capacity_upgrade);

    pattern = Pattern::from_string("48 8D 40 04 41 FF C0 41 8B 54 89 28 01 50 FC 41 83 F8 08");
    result = PatternScanner::find_first(pattern);
    if (!result) {
        MessageBoxA(nullptr, "Failed to find pattern 2", "Inventory Expander Error", MB_ICONERROR);
        return;
    }

    std::println("Found Hook 2 address at {:X}", result - 20);
    hook2 = safetyhook::create_inline(result - 20, apply_base_capacity);

    nlohmann::json json;
    try {
        std::ifstream("InventoryUpgrades.json") >> json;
    }
    catch (const std::exception& e) {
        MessageBoxA(nullptr, e.what(), "Inventory Expander Error", MB_ICONERROR);
        return;
    }

    (void)CoInitializeEx(nullptr, COINIT_MULTITHREADED);

    base_upgrade = {
        .weapons = json["Base"]["Weapons"],
        .mods = json["Base"]["Modifications"],
        .outfits = json["Base"]["Outfits"],
        .resources = json["Base"]["Resources"],
        .tools = json["Base"]["Tools"]
    };

    for (const auto& upgrade : json["Weapons"]) {
        GUID guid;
        const std::string guid_str = upgrade["GUID"];
        std::wstring guid_wstr(guid_str.begin(), guid_str.end());
        (void)IIDFromString(guid_wstr.c_str(), &guid);

        upgrades[guid] = { .weapons = upgrade["Increase"] };
    }

    for (const auto& upgrade : json["Outfits"]) {
        GUID guid;
        const std::string guid_str = upgrade["GUID"];
        std::wstring guid_wstr(guid_str.begin(), guid_str.end());
        (void)IIDFromString(guid_wstr.c_str(), &guid);

        upgrades[guid] = { .outfits = upgrade["Increase"] };
    }

    for (const auto& upgrade : json["Modifications"]) {
        GUID guid;
        const std::string guid_str = upgrade["GUID"];
        std::wstring guid_wstr(guid_str.begin(), guid_str.end());
        (void)IIDFromString(guid_wstr.c_str(), &guid);

        upgrades[guid] = { .mods = upgrade["Increase"] };
    }

    for (const auto& upgrade : json["Resources"]) {
        GUID guid;
        const std::string guid_str = upgrade["GUID"];
        std::wstring guid_wstr(guid_str.begin(), guid_str.end());
        (void)IIDFromString(guid_wstr.c_str(), &guid);

        upgrades[guid] = { .resources = upgrade["Increase"] };
    }

    CoUninitialize();
}

} // namespace

BOOL APIENTRY DllMain(HMODULE hModule, DWORD  ul_reason_for_call, LPVOID lpReserved) {
    if (ul_reason_for_call == DLL_PROCESS_ATTACH) {
        std::println("Inventory Expander loaded");
        load();
    }

    return TRUE;
}

#pragma region winmm forwarding
#pragma comment(linker, "/export:CloseDriver=\"C:\\Windows\\System32\\winmm.CloseDriver\"")
#pragma comment(linker, "/export:DefDriverProc=\"C:\\Windows\\System32\\winmm.DefDriverProc\"")
#pragma comment(linker, "/export:DriverCallback=\"C:\\Windows\\System32\\winmm.DriverCallback\"")
#pragma comment(linker, "/export:DrvGetModuleHandle=\"C:\\Windows\\System32\\winmm.DrvGetModuleHandle\"")
#pragma comment(linker, "/export:GetDriverModuleHandle=\"C:\\Windows\\System32\\winmm.GetDriverModuleHandle\"")
#pragma comment(linker, "/export:OpenDriver=\"C:\\Windows\\System32\\winmm.OpenDriver\"")
#pragma comment(linker, "/export:PlaySound=\"C:\\Windows\\System32\\winmm.PlaySound\"")
#pragma comment(linker, "/export:PlaySoundA=\"C:\\Windows\\System32\\winmm.PlaySoundA\"")
#pragma comment(linker, "/export:PlaySoundW=\"C:\\Windows\\System32\\winmm.PlaySoundW\"")
#pragma comment(linker, "/export:SendDriverMessage=\"C:\\Windows\\System32\\winmm.SendDriverMessage\"")
#pragma comment(linker, "/export:WOWAppExit=\"C:\\Windows\\System32\\winmm.WOWAppExit\"")
#pragma comment(linker, "/export:auxGetDevCapsA=\"C:\\Windows\\System32\\winmm.auxGetDevCapsA\"")
#pragma comment(linker, "/export:auxGetDevCapsW=\"C:\\Windows\\System32\\winmm.auxGetDevCapsW\"")
#pragma comment(linker, "/export:auxGetNumDevs=\"C:\\Windows\\System32\\winmm.auxGetNumDevs\"")
#pragma comment(linker, "/export:auxGetVolume=\"C:\\Windows\\System32\\winmm.auxGetVolume\"")
#pragma comment(linker, "/export:auxOutMessage=\"C:\\Windows\\System32\\winmm.auxOutMessage\"")
#pragma comment(linker, "/export:auxSetVolume=\"C:\\Windows\\System32\\winmm.auxSetVolume\"")
#pragma comment(linker, "/export:joyConfigChanged=\"C:\\Windows\\System32\\winmm.joyConfigChanged\"")
#pragma comment(linker, "/export:joyGetDevCapsA=\"C:\\Windows\\System32\\winmm.joyGetDevCapsA\"")
#pragma comment(linker, "/export:joyGetDevCapsW=\"C:\\Windows\\System32\\winmm.joyGetDevCapsW\"")
#pragma comment(linker, "/export:joyGetNumDevs=\"C:\\Windows\\System32\\winmm.joyGetNumDevs\"")
#pragma comment(linker, "/export:joyGetPos=\"C:\\Windows\\System32\\winmm.joyGetPos\"")
#pragma comment(linker, "/export:joyGetPosEx=\"C:\\Windows\\System32\\winmm.joyGetPosEx\"")
#pragma comment(linker, "/export:joyGetThreshold=\"C:\\Windows\\System32\\winmm.joyGetThreshold\"")
#pragma comment(linker, "/export:joyReleaseCapture=\"C:\\Windows\\System32\\winmm.joyReleaseCapture\"")
#pragma comment(linker, "/export:joySetCapture=\"C:\\Windows\\System32\\winmm.joySetCapture\"")
#pragma comment(linker, "/export:joySetThreshold=\"C:\\Windows\\System32\\winmm.joySetThreshold\"")
#pragma comment(linker, "/export:mciDriverNotify=\"C:\\Windows\\System32\\winmm.mciDriverNotify\"")
#pragma comment(linker, "/export:mciDriverYield=\"C:\\Windows\\System32\\winmm.mciDriverYield\"")
#pragma comment(linker, "/export:mciExecute=\"C:\\Windows\\System32\\winmm.mciExecute\"")
#pragma comment(linker, "/export:mciFreeCommandResource=\"C:\\Windows\\System32\\winmm.mciFreeCommandResource\"")
#pragma comment(linker, "/export:mciGetCreatorTask=\"C:\\Windows\\System32\\winmm.mciGetCreatorTask\"")
#pragma comment(linker, "/export:mciGetDeviceIDA=\"C:\\Windows\\System32\\winmm.mciGetDeviceIDA\"")
#pragma comment(linker, "/export:mciGetDeviceIDFromElementIDA=\"C:\\Windows\\System32\\winmm.mciGetDeviceIDFromElementIDA\"")
#pragma comment(linker, "/export:mciGetDeviceIDFromElementIDW=\"C:\\Windows\\System32\\winmm.mciGetDeviceIDFromElementIDW\"")
#pragma comment(linker, "/export:mciGetDeviceIDW=\"C:\\Windows\\System32\\winmm.mciGetDeviceIDW\"")
#pragma comment(linker, "/export:mciGetDriverData=\"C:\\Windows\\System32\\winmm.mciGetDriverData\"")
#pragma comment(linker, "/export:mciGetErrorStringA=\"C:\\Windows\\System32\\winmm.mciGetErrorStringA\"")
#pragma comment(linker, "/export:mciGetErrorStringW=\"C:\\Windows\\System32\\winmm.mciGetErrorStringW\"")
#pragma comment(linker, "/export:mciGetYieldProc=\"C:\\Windows\\System32\\winmm.mciGetYieldProc\"")
#pragma comment(linker, "/export:mciLoadCommandResource=\"C:\\Windows\\System32\\winmm.mciLoadCommandResource\"")
#pragma comment(linker, "/export:mciSendCommandA=\"C:\\Windows\\System32\\winmm.mciSendCommandA\"")
#pragma comment(linker, "/export:mciSendCommandW=\"C:\\Windows\\System32\\winmm.mciSendCommandW\"")
#pragma comment(linker, "/export:mciSendStringA=\"C:\\Windows\\System32\\winmm.mciSendStringA\"")
#pragma comment(linker, "/export:mciSendStringW=\"C:\\Windows\\System32\\winmm.mciSendStringW\"")
#pragma comment(linker, "/export:mciSetDriverData=\"C:\\Windows\\System32\\winmm.mciSetDriverData\"")
#pragma comment(linker, "/export:mciSetYieldProc=\"C:\\Windows\\System32\\winmm.mciSetYieldProc\"")
#pragma comment(linker, "/export:midiConnect=\"C:\\Windows\\System32\\winmm.midiConnect\"")
#pragma comment(linker, "/export:midiDisconnect=\"C:\\Windows\\System32\\winmm.midiDisconnect\"")
#pragma comment(linker, "/export:midiInAddBuffer=\"C:\\Windows\\System32\\winmm.midiInAddBuffer\"")
#pragma comment(linker, "/export:midiInClose=\"C:\\Windows\\System32\\winmm.midiInClose\"")
#pragma comment(linker, "/export:midiInGetDevCapsA=\"C:\\Windows\\System32\\winmm.midiInGetDevCapsA\"")
#pragma comment(linker, "/export:midiInGetDevCapsW=\"C:\\Windows\\System32\\winmm.midiInGetDevCapsW\"")
#pragma comment(linker, "/export:midiInGetErrorTextA=\"C:\\Windows\\System32\\winmm.midiInGetErrorTextA\"")
#pragma comment(linker, "/export:midiInGetErrorTextW=\"C:\\Windows\\System32\\winmm.midiInGetErrorTextW\"")
#pragma comment(linker, "/export:midiInGetID=\"C:\\Windows\\System32\\winmm.midiInGetID\"")
#pragma comment(linker, "/export:midiInGetNumDevs=\"C:\\Windows\\System32\\winmm.midiInGetNumDevs\"")
#pragma comment(linker, "/export:midiInMessage=\"C:\\Windows\\System32\\winmm.midiInMessage\"")
#pragma comment(linker, "/export:midiInOpen=\"C:\\Windows\\System32\\winmm.midiInOpen\"")
#pragma comment(linker, "/export:midiInPrepareHeader=\"C:\\Windows\\System32\\winmm.midiInPrepareHeader\"")
#pragma comment(linker, "/export:midiInReset=\"C:\\Windows\\System32\\winmm.midiInReset\"")
#pragma comment(linker, "/export:midiInStart=\"C:\\Windows\\System32\\winmm.midiInStart\"")
#pragma comment(linker, "/export:midiInStop=\"C:\\Windows\\System32\\winmm.midiInStop\"")
#pragma comment(linker, "/export:midiInUnprepareHeader=\"C:\\Windows\\System32\\winmm.midiInUnprepareHeader\"")
#pragma comment(linker, "/export:midiOutCacheDrumPatches=\"C:\\Windows\\System32\\winmm.midiOutCacheDrumPatches\"")
#pragma comment(linker, "/export:midiOutCachePatches=\"C:\\Windows\\System32\\winmm.midiOutCachePatches\"")
#pragma comment(linker, "/export:midiOutClose=\"C:\\Windows\\System32\\winmm.midiOutClose\"")
#pragma comment(linker, "/export:midiOutGetDevCapsA=\"C:\\Windows\\System32\\winmm.midiOutGetDevCapsA\"")
#pragma comment(linker, "/export:midiOutGetDevCapsW=\"C:\\Windows\\System32\\winmm.midiOutGetDevCapsW\"")
#pragma comment(linker, "/export:midiOutGetErrorTextA=\"C:\\Windows\\System32\\winmm.midiOutGetErrorTextA\"")
#pragma comment(linker, "/export:midiOutGetErrorTextW=\"C:\\Windows\\System32\\winmm.midiOutGetErrorTextW\"")
#pragma comment(linker, "/export:midiOutGetID=\"C:\\Windows\\System32\\winmm.midiOutGetID\"")
#pragma comment(linker, "/export:midiOutGetNumDevs=\"C:\\Windows\\System32\\winmm.midiOutGetNumDevs\"")
#pragma comment(linker, "/export:midiOutGetVolume=\"C:\\Windows\\System32\\winmm.midiOutGetVolume\"")
#pragma comment(linker, "/export:midiOutLongMsg=\"C:\\Windows\\System32\\winmm.midiOutLongMsg\"")
#pragma comment(linker, "/export:midiOutMessage=\"C:\\Windows\\System32\\winmm.midiOutMessage\"")
#pragma comment(linker, "/export:midiOutOpen=\"C:\\Windows\\System32\\winmm.midiOutOpen\"")
#pragma comment(linker, "/export:midiOutPrepareHeader=\"C:\\Windows\\System32\\winmm.midiOutPrepareHeader\"")
#pragma comment(linker, "/export:midiOutReset=\"C:\\Windows\\System32\\winmm.midiOutReset\"")
#pragma comment(linker, "/export:midiOutSetVolume=\"C:\\Windows\\System32\\winmm.midiOutSetVolume\"")
#pragma comment(linker, "/export:midiOutShortMsg=\"C:\\Windows\\System32\\winmm.midiOutShortMsg\"")
#pragma comment(linker, "/export:midiOutUnprepareHeader=\"C:\\Windows\\System32\\winmm.midiOutUnprepareHeader\"")
#pragma comment(linker, "/export:midiStreamClose=\"C:\\Windows\\System32\\winmm.midiStreamClose\"")
#pragma comment(linker, "/export:midiStreamOpen=\"C:\\Windows\\System32\\winmm.midiStreamOpen\"")
#pragma comment(linker, "/export:midiStreamOut=\"C:\\Windows\\System32\\winmm.midiStreamOut\"")
#pragma comment(linker, "/export:midiStreamPause=\"C:\\Windows\\System32\\winmm.midiStreamPause\"")
#pragma comment(linker, "/export:midiStreamPosition=\"C:\\Windows\\System32\\winmm.midiStreamPosition\"")
#pragma comment(linker, "/export:midiStreamProperty=\"C:\\Windows\\System32\\winmm.midiStreamProperty\"")
#pragma comment(linker, "/export:midiStreamRestart=\"C:\\Windows\\System32\\winmm.midiStreamRestart\"")
#pragma comment(linker, "/export:midiStreamStop=\"C:\\Windows\\System32\\winmm.midiStreamStop\"")
#pragma comment(linker, "/export:mixerClose=\"C:\\Windows\\System32\\winmm.mixerClose\"")
#pragma comment(linker, "/export:mixerGetControlDetailsA=\"C:\\Windows\\System32\\winmm.mixerGetControlDetailsA\"")
#pragma comment(linker, "/export:mixerGetControlDetailsW=\"C:\\Windows\\System32\\winmm.mixerGetControlDetailsW\"")
#pragma comment(linker, "/export:mixerGetDevCapsA=\"C:\\Windows\\System32\\winmm.mixerGetDevCapsA\"")
#pragma comment(linker, "/export:mixerGetDevCapsW=\"C:\\Windows\\System32\\winmm.mixerGetDevCapsW\"")
#pragma comment(linker, "/export:mixerGetID=\"C:\\Windows\\System32\\winmm.mixerGetID\"")
#pragma comment(linker, "/export:mixerGetLineControlsA=\"C:\\Windows\\System32\\winmm.mixerGetLineControlsA\"")
#pragma comment(linker, "/export:mixerGetLineControlsW=\"C:\\Windows\\System32\\winmm.mixerGetLineControlsW\"")
#pragma comment(linker, "/export:mixerGetLineInfoA=\"C:\\Windows\\System32\\winmm.mixerGetLineInfoA\"")
#pragma comment(linker, "/export:mixerGetLineInfoW=\"C:\\Windows\\System32\\winmm.mixerGetLineInfoW\"")
#pragma comment(linker, "/export:mixerGetNumDevs=\"C:\\Windows\\System32\\winmm.mixerGetNumDevs\"")
#pragma comment(linker, "/export:mixerMessage=\"C:\\Windows\\System32\\winmm.mixerMessage\"")
#pragma comment(linker, "/export:mixerOpen=\"C:\\Windows\\System32\\winmm.mixerOpen\"")
#pragma comment(linker, "/export:mixerSetControlDetails=\"C:\\Windows\\System32\\winmm.mixerSetControlDetails\"")
#pragma comment(linker, "/export:mmDrvInstall=\"C:\\Windows\\System32\\winmm.mmDrvInstall\"")
#pragma comment(linker, "/export:mmGetCurrentTask=\"C:\\Windows\\System32\\winmm.mmGetCurrentTask\"")
#pragma comment(linker, "/export:mmTaskBlock=\"C:\\Windows\\System32\\winmm.mmTaskBlock\"")
#pragma comment(linker, "/export:mmTaskCreate=\"C:\\Windows\\System32\\winmm.mmTaskCreate\"")
#pragma comment(linker, "/export:mmTaskSignal=\"C:\\Windows\\System32\\winmm.mmTaskSignal\"")
#pragma comment(linker, "/export:mmTaskYield=\"C:\\Windows\\System32\\winmm.mmTaskYield\"")
#pragma comment(linker, "/export:mmioAdvance=\"C:\\Windows\\System32\\winmm.mmioAdvance\"")
#pragma comment(linker, "/export:mmioAscend=\"C:\\Windows\\System32\\winmm.mmioAscend\"")
#pragma comment(linker, "/export:mmioClose=\"C:\\Windows\\System32\\winmm.mmioClose\"")
#pragma comment(linker, "/export:mmioCreateChunk=\"C:\\Windows\\System32\\winmm.mmioCreateChunk\"")
#pragma comment(linker, "/export:mmioDescend=\"C:\\Windows\\System32\\winmm.mmioDescend\"")
#pragma comment(linker, "/export:mmioFlush=\"C:\\Windows\\System32\\winmm.mmioFlush\"")
#pragma comment(linker, "/export:mmioGetInfo=\"C:\\Windows\\System32\\winmm.mmioGetInfo\"")
#pragma comment(linker, "/export:mmioInstallIOProcA=\"C:\\Windows\\System32\\winmm.mmioInstallIOProcA\"")
#pragma comment(linker, "/export:mmioInstallIOProcW=\"C:\\Windows\\System32\\winmm.mmioInstallIOProcW\"")
#pragma comment(linker, "/export:mmioOpenA=\"C:\\Windows\\System32\\winmm.mmioOpenA\"")
#pragma comment(linker, "/export:mmioOpenW=\"C:\\Windows\\System32\\winmm.mmioOpenW\"")
#pragma comment(linker, "/export:mmioRead=\"C:\\Windows\\System32\\winmm.mmioRead\"")
#pragma comment(linker, "/export:mmioRenameA=\"C:\\Windows\\System32\\winmm.mmioRenameA\"")
#pragma comment(linker, "/export:mmioRenameW=\"C:\\Windows\\System32\\winmm.mmioRenameW\"")
#pragma comment(linker, "/export:mmioSeek=\"C:\\Windows\\System32\\winmm.mmioSeek\"")
#pragma comment(linker, "/export:mmioSendMessage=\"C:\\Windows\\System32\\winmm.mmioSendMessage\"")
#pragma comment(linker, "/export:mmioSetBuffer=\"C:\\Windows\\System32\\winmm.mmioSetBuffer\"")
#pragma comment(linker, "/export:mmioSetInfo=\"C:\\Windows\\System32\\winmm.mmioSetInfo\"")
#pragma comment(linker, "/export:mmioStringToFOURCCA=\"C:\\Windows\\System32\\winmm.mmioStringToFOURCCA\"")
#pragma comment(linker, "/export:mmioStringToFOURCCW=\"C:\\Windows\\System32\\winmm.mmioStringToFOURCCW\"")
#pragma comment(linker, "/export:mmioWrite=\"C:\\Windows\\System32\\winmm.mmioWrite\"")
#pragma comment(linker, "/export:mmsystemGetVersion=\"C:\\Windows\\System32\\winmm.mmsystemGetVersion\"")
#pragma comment(linker, "/export:sndPlaySoundA=\"C:\\Windows\\System32\\winmm.sndPlaySoundA\"")
#pragma comment(linker, "/export:sndPlaySoundW=\"C:\\Windows\\System32\\winmm.sndPlaySoundW\"")
#pragma comment(linker, "/export:timeBeginPeriod=\"C:\\Windows\\System32\\winmm.timeBeginPeriod\"")
#pragma comment(linker, "/export:timeEndPeriod=\"C:\\Windows\\System32\\winmm.timeEndPeriod\"")
#pragma comment(linker, "/export:timeGetDevCaps=\"C:\\Windows\\System32\\winmm.timeGetDevCaps\"")
#pragma comment(linker, "/export:timeGetSystemTime=\"C:\\Windows\\System32\\winmm.timeGetSystemTime\"")
#pragma comment(linker, "/export:timeGetTime=\"C:\\Windows\\System32\\winmm.timeGetTime\"")
#pragma comment(linker, "/export:timeKillEvent=\"C:\\Windows\\System32\\winmm.timeKillEvent\"")
#pragma comment(linker, "/export:timeSetEvent=\"C:\\Windows\\System32\\winmm.timeSetEvent\"")
#pragma comment(linker, "/export:waveInAddBuffer=\"C:\\Windows\\System32\\winmm.waveInAddBuffer\"")
#pragma comment(linker, "/export:waveInClose=\"C:\\Windows\\System32\\winmm.waveInClose\"")
#pragma comment(linker, "/export:waveInGetDevCapsA=\"C:\\Windows\\System32\\winmm.waveInGetDevCapsA\"")
#pragma comment(linker, "/export:waveInGetDevCapsW=\"C:\\Windows\\System32\\winmm.waveInGetDevCapsW\"")
#pragma comment(linker, "/export:waveInGetErrorTextA=\"C:\\Windows\\System32\\winmm.waveInGetErrorTextA\"")
#pragma comment(linker, "/export:waveInGetErrorTextW=\"C:\\Windows\\System32\\winmm.waveInGetErrorTextW\"")
#pragma comment(linker, "/export:waveInGetID=\"C:\\Windows\\System32\\winmm.waveInGetID\"")
#pragma comment(linker, "/export:waveInGetNumDevs=\"C:\\Windows\\System32\\winmm.waveInGetNumDevs\"")
#pragma comment(linker, "/export:waveInGetPosition=\"C:\\Windows\\System32\\winmm.waveInGetPosition\"")
#pragma comment(linker, "/export:waveInMessage=\"C:\\Windows\\System32\\winmm.waveInMessage\"")
#pragma comment(linker, "/export:waveInOpen=\"C:\\Windows\\System32\\winmm.waveInOpen\"")
#pragma comment(linker, "/export:waveInPrepareHeader=\"C:\\Windows\\System32\\winmm.waveInPrepareHeader\"")
#pragma comment(linker, "/export:waveInReset=\"C:\\Windows\\System32\\winmm.waveInReset\"")
#pragma comment(linker, "/export:waveInStart=\"C:\\Windows\\System32\\winmm.waveInStart\"")
#pragma comment(linker, "/export:waveInStop=\"C:\\Windows\\System32\\winmm.waveInStop\"")
#pragma comment(linker, "/export:waveInUnprepareHeader=\"C:\\Windows\\System32\\winmm.waveInUnprepareHeader\"")
#pragma comment(linker, "/export:waveOutBreakLoop=\"C:\\Windows\\System32\\winmm.waveOutBreakLoop\"")
#pragma comment(linker, "/export:waveOutClose=\"C:\\Windows\\System32\\winmm.waveOutClose\"")
#pragma comment(linker, "/export:waveOutGetDevCapsA=\"C:\\Windows\\System32\\winmm.waveOutGetDevCapsA\"")
#pragma comment(linker, "/export:waveOutGetDevCapsW=\"C:\\Windows\\System32\\winmm.waveOutGetDevCapsW\"")
#pragma comment(linker, "/export:waveOutGetErrorTextA=\"C:\\Windows\\System32\\winmm.waveOutGetErrorTextA\"")
#pragma comment(linker, "/export:waveOutGetErrorTextW=\"C:\\Windows\\System32\\winmm.waveOutGetErrorTextW\"")
#pragma comment(linker, "/export:waveOutGetID=\"C:\\Windows\\System32\\winmm.waveOutGetID\"")
#pragma comment(linker, "/export:waveOutGetNumDevs=\"C:\\Windows\\System32\\winmm.waveOutGetNumDevs\"")
#pragma comment(linker, "/export:waveOutGetPitch=\"C:\\Windows\\System32\\winmm.waveOutGetPitch\"")
#pragma comment(linker, "/export:waveOutGetPlaybackRate=\"C:\\Windows\\System32\\winmm.waveOutGetPlaybackRate\"")
#pragma comment(linker, "/export:waveOutGetPosition=\"C:\\Windows\\System32\\winmm.waveOutGetPosition\"")
#pragma comment(linker, "/export:waveOutGetVolume=\"C:\\Windows\\System32\\winmm.waveOutGetVolume\"")
#pragma comment(linker, "/export:waveOutMessage=\"C:\\Windows\\System32\\winmm.waveOutMessage\"")
#pragma comment(linker, "/export:waveOutOpen=\"C:\\Windows\\System32\\winmm.waveOutOpen\"")
#pragma comment(linker, "/export:waveOutPause=\"C:\\Windows\\System32\\winmm.waveOutPause\"")
#pragma comment(linker, "/export:waveOutPrepareHeader=\"C:\\Windows\\System32\\winmm.waveOutPrepareHeader\"")
#pragma comment(linker, "/export:waveOutReset=\"C:\\Windows\\System32\\winmm.waveOutReset\"")
#pragma comment(linker, "/export:waveOutRestart=\"C:\\Windows\\System32\\winmm.waveOutRestart\"")
#pragma comment(linker, "/export:waveOutSetPitch=\"C:\\Windows\\System32\\winmm.waveOutSetPitch\"")
#pragma comment(linker, "/export:waveOutSetPlaybackRate=\"C:\\Windows\\System32\\winmm.waveOutSetPlaybackRate\"")
#pragma comment(linker, "/export:waveOutSetVolume=\"C:\\Windows\\System32\\winmm.waveOutSetVolume\"")
#pragma comment(linker, "/export:waveOutUnprepareHeader=\"C:\\Windows\\System32\\winmm.waveOutUnprepareHeader\"")
#pragma comment(linker, "/export:waveOutWrite=\"C:\\Windows\\System32\\winmm.waveOutWrite\"")
#pragma endregion
