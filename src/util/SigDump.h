#pragma once
#include <atomic>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

// Developer tool: dumps function offsets + ready-to-paste signatures.
//
//  - arm("AttackEvent")  -> the next time that Latite event is dispatched, the call stack is captured, every
//                           Minecraft.Windows.exe frame is resolved to its function start (via the x64 unwind
//                           table), and a wildcarded + uniqueness-checked signature is generated for it.
//  - dumpAll()           -> every registered SigImpl: found / NOT FOUND, scan RVA, resolved RVA.
//  - dumpAt(addr)        -> signature for an arbitrary RVA / address (e.g. from a debugger).
//  - dumpHooks()         -> every Hook target (RVA + signature from the ORIGINAL, pre-patch bytes).
//
// Output goes to <Latite dir>/SigDump/*.txt and to the log. All heavy work runs on a worker thread.
namespace SigDump {
    // Checked on the hot path of Eventing::dispatch (relaxed load only)
    extern std::atomic<int> armedCount;

    // Called by Eventing::dispatch when armedCount > 0. `funcSig` is __FUNCSIG__ of dispatch<T>.
    void onEvent(std::string_view funcSig);

    // Hook bookkeeping (called from Hook ctor, before the target is patched)
    // tableSwap: `target` is the address of a vtable slot instead of a function
    void registerHook(uintptr_t target, void* detour, std::string_view name, bool tableSwap);

    // Control (return value is a human readable status line for the command to show)
    std::string arm(std::string_view eventFilter, int count);
    std::string disarm();
    std::string status();
    std::vector<std::string> knownEvents();

    // One-shot dumps, each returns the output file path (or an error string)
    std::string dumpAll();
    std::string dumpHooks();
    std::string dumpAt(uintptr_t addrOrRva);

    // Blocks until worker threads are done (call before the DLL unloads)
    void waitIdle();
}
