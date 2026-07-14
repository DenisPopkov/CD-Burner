#include "CrashReporter.h"
#include <array>
#include <atomic>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <mutex>

#if JUCE_WINDOWS
 #ifndef WIN32_LEAN_AND_MEAN
  #define WIN32_LEAN_AND_MEAN
 #endif
 #include <windows.h>
 #include <dbghelp.h>
 #pragma comment(lib, "dbghelp.lib")
#endif

namespace cassette
{

namespace
{

constexpr int kBreadcrumbCapacity = 64;
constexpr int kBreadcrumbMaxChars = 240;

struct BreadcrumbRing
{
    std::mutex lock;
    std::array<char, kBreadcrumbCapacity * kBreadcrumbMaxChars> storage {};
    int next = 0;
    int count = 0;

    void push(const juce::String& message)
    {
        const std::lock_guard<std::mutex> sl(lock);
        auto* slot = storage.data() + (next * kBreadcrumbMaxChars);
        std::memset(slot, 0, kBreadcrumbMaxChars);
        const auto utf8 = message.toRawUTF8();
        std::strncpy(slot, utf8, kBreadcrumbMaxChars - 1);
        next = (next + 1) % kBreadcrumbCapacity;
        count = juce::jmin(kBreadcrumbCapacity, count + 1);
    }

    juce::String dump()
    {
        const std::lock_guard<std::mutex> sl(lock);
        juce::String out;
        const int n = count;
        const int start = (next - n + kBreadcrumbCapacity) % kBreadcrumbCapacity;
        for (int i = 0; i < n; ++i)
        {
            const int idx = (start + i) % kBreadcrumbCapacity;
            const char* slot = storage.data() + (idx * kBreadcrumbMaxChars);
            if (slot[0] == 0)
                continue;
            out << "  - " << juce::String::fromUTF8(slot) << "\n";
        }
        return out;
    }
};

BreadcrumbRing& breadcrumbs()
{
    static BreadcrumbRing ring;
    return ring;
}

std::atomic<bool> handlersInstalled { false };
juce::String gAppName { "CD Burner" };

juce::File crashLogPath(const juce::String& appName)
{
    const auto stamp = juce::Time::getCurrentTime().formatted("%Y%m%d-%H%M%S");
    const auto safeName = appName.replaceCharacters(" /\\:*?\"<>|", "---------");
    return juce::File::getSpecialLocation(juce::File::userDesktopDirectory)
        .getChildFile(safeName + "-crash-" + stamp + ".txt");
}

juce::String exceptionNameWindows(unsigned long code)
{
#if JUCE_WINDOWS
    switch (code)
    {
        case EXCEPTION_ACCESS_VIOLATION: return "EXCEPTION_ACCESS_VIOLATION";
        case EXCEPTION_ARRAY_BOUNDS_EXCEEDED: return "EXCEPTION_ARRAY_BOUNDS_EXCEEDED";
        case EXCEPTION_BREAKPOINT: return "EXCEPTION_BREAKPOINT";
        case EXCEPTION_DATATYPE_MISALIGNMENT: return "EXCEPTION_DATATYPE_MISALIGNMENT";
        case EXCEPTION_FLT_DIVIDE_BY_ZERO: return "EXCEPTION_FLT_DIVIDE_BY_ZERO";
        case EXCEPTION_FLT_OVERFLOW: return "EXCEPTION_FLT_OVERFLOW";
        case EXCEPTION_ILLEGAL_INSTRUCTION: return "EXCEPTION_ILLEGAL_INSTRUCTION";
        case EXCEPTION_IN_PAGE_ERROR: return "EXCEPTION_IN_PAGE_ERROR";
        case EXCEPTION_INT_DIVIDE_BY_ZERO: return "EXCEPTION_INT_DIVIDE_BY_ZERO";
        case EXCEPTION_INVALID_HANDLE: return "EXCEPTION_INVALID_HANDLE";
        case EXCEPTION_NONCONTINUABLE_EXCEPTION: return "EXCEPTION_NONCONTINUABLE_EXCEPTION";
        case EXCEPTION_PRIV_INSTRUCTION: return "EXCEPTION_PRIV_INSTRUCTION";
        case EXCEPTION_STACK_OVERFLOW: return "EXCEPTION_STACK_OVERFLOW";
        default: return "EXCEPTION_0x" + juce::String::toHexString(static_cast<int>(code));
    }
#else
    juce::ignoreUnused(code);
    return {};
#endif
}

#if JUCE_WINDOWS
juce::String captureWindowsStack(CONTEXT* context)
{
    juce::String out;
    HANDLE process = GetCurrentProcess();
    HANDLE thread = GetCurrentThread();
    SymInitialize(process, nullptr, TRUE);

    STACKFRAME64 frame {};
    DWORD machine = IMAGE_FILE_MACHINE_AMD64;
#if defined(_M_IX86)
    machine = IMAGE_FILE_MACHINE_I386;
    frame.AddrPC.Offset = context->Eip;
    frame.AddrFrame.Offset = context->Ebp;
    frame.AddrStack.Offset = context->Esp;
#elif defined(_M_ARM64)
    machine = IMAGE_FILE_MACHINE_ARM64;
    frame.AddrPC.Offset = context->Pc;
    frame.AddrFrame.Offset = context->Fp;
    frame.AddrStack.Offset = context->Sp;
#else
    frame.AddrPC.Offset = context->Rip;
    frame.AddrFrame.Offset = context->Rbp;
    frame.AddrStack.Offset = context->Rsp;
#endif
    frame.AddrPC.Mode = AddrModeFlat;
    frame.AddrFrame.Mode = AddrModeFlat;
    frame.AddrStack.Mode = AddrModeFlat;

    for (int i = 0; i < 32; ++i)
    {
        if (!StackWalk64(machine,
                         process,
                         thread,
                         &frame,
                         context,
                         nullptr,
                         SymFunctionTableAccess64,
                         SymGetModuleBase64,
                         nullptr))
            break;

        if (frame.AddrPC.Offset == 0)
            break;

        char symbolBuffer[sizeof(SYMBOL_INFO) + 256];
        std::memset(symbolBuffer, 0, sizeof(symbolBuffer));
        auto* symbol = reinterpret_cast<SYMBOL_INFO*>(symbolBuffer);
        symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
        symbol->MaxNameLen = 255;

        DWORD64 displacement = 0;
        juce::String name = "0x" + juce::String::toHexString(static_cast<int64_t>(frame.AddrPC.Offset));
        if (SymFromAddr(process, frame.AddrPC.Offset, &displacement, symbol))
            name = juce::String(symbol->Name) + " +0x" + juce::String::toHexString(static_cast<int>(displacement));

        out << "  #" << i << " " << name << "\n";
    }

    SymCleanup(process);
    return out;
}

LONG WINAPI unhandledExceptionFilter(EXCEPTION_POINTERS* info)
{
    juce::String details;
    if (info != nullptr && info->ExceptionRecord != nullptr)
    {
        const auto code = info->ExceptionRecord->ExceptionCode;
        details << "ExceptionCode: " << exceptionNameWindows(code)
                << " (0x" << juce::String::toHexString(static_cast<int>(code)) << ")\n";
        details << "ExceptionAddress: 0x"
                << juce::String::toHexString(reinterpret_cast<juce::pointer_sized_int>(
                       info->ExceptionRecord->ExceptionAddress))
                << "\n";

        if (code == EXCEPTION_ACCESS_VIOLATION && info->ExceptionRecord->NumberParameters >= 2)
        {
            const auto op = info->ExceptionRecord->ExceptionInformation[0];
            details << "AccessType: " << (op == 0 ? "read" : op == 1 ? "write" : "execute") << "\n";
            details << "AccessAddress: 0x"
                    << juce::String::toHexString(static_cast<int64_t>(info->ExceptionRecord->ExceptionInformation[1]))
                    << "\n";
        }

        if (info->ContextRecord != nullptr)
            details << "\nStack:\n" << captureWindowsStack(info->ContextRecord);
    }

    writeCrashLogToDesktop(gAppName, "Unhandled SEH exception", details);
    return EXCEPTION_EXECUTE_HANDLER;
}

void winInvalidParameter(const wchar_t*, const wchar_t*, const wchar_t*, unsigned int, uintptr_t)
{
    writeCrashLogToDesktop(gAppName, "Invalid parameter", {});
}

void winPureCall()
{
    writeCrashLogToDesktop(gAppName, "Pure virtual call", {});
}
#endif

[[noreturn]] void terminateHandler()
{
    juce::String details;
    try
    {
        auto eptr = std::current_exception();
        if (eptr)
        {
            try
            {
                std::rethrow_exception(eptr);
            }
            catch (const std::exception& e)
            {
                details << "std::exception: " << e.what() << "\n";
            }
            catch (...)
            {
                details << "Unknown C++ exception\n";
            }
        }
    }
    catch (...)
    {
    }

    writeCrashLogToDesktop(gAppName, "std::terminate", details);
    std::abort();
}

} // namespace

void crashBreadcrumb(const juce::String& message)
{
    const auto stamped = juce::Time::getCurrentTime().formatted("%H:%M:%S") + "  " + message;
    breadcrumbs().push(stamped);
}

void writeCrashLogToDesktop(const juce::String& appName,
                            const juce::String& reason,
                            const juce::String& details)
{
    try
    {
        const auto path = crashLogPath(appName);
        juce::String body;
        body << appName << " crash report\n";
        body << "Time: " << juce::Time::getCurrentTime().toString(true, true, true, true) << "\n";
        body << "Reason: " << reason << "\n";
#if JUCE_WINDOWS
        body << "Platform: Windows\n";
#elif JUCE_MAC
        body << "Platform: macOS\n";
#elif JUCE_LINUX
        body << "Platform: Linux\n";
#else
        body << "Platform: unknown\n";
#endif
        body << "OS: " << juce::SystemStats::getOperatingSystemName() << "\n";
        body << "CPU: " << juce::SystemStats::getCpuModel() << "\n";
        body << "\nRecent activity:\n" << breadcrumbs().dump();
        if (details.isNotEmpty())
            body << "\nDetails:\n" << details << "\n";

        path.replaceWithText(body);
    }
    catch (...)
    {
        // Last-resort: never throw from crash path.
    }
}

void installCrashReporting(const juce::String& appName)
{
    if (handlersInstalled.exchange(true))
        return;

    gAppName = appName.isNotEmpty() ? appName : juce::String("CD Burner");
    crashBreadcrumb("Crash reporting installed");

    std::set_terminate(terminateHandler);

#if JUCE_WINDOWS
    SetUnhandledExceptionFilter(unhandledExceptionFilter);
    _set_invalid_parameter_handler(winInvalidParameter);
    _set_purecall_handler(winPureCall);
#endif
}

} // namespace cassette
