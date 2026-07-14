#pragma once

#include <juce_core/juce_core.h>

namespace cassette
{

/** Install OS / terminate handlers. Safe to call once at startup. */
void installCrashReporting(const juce::String& appName);

/** Append a breadcrumb that appears in the next crash log (ring buffer). */
void crashBreadcrumb(const juce::String& message);

/**
 * Write a crash / fatal report to the Desktop:
 *   CD-Burner-crash-YYYYMMDD-HHMMSS.txt
 * Safe enough to call from SEH / terminate handlers (keeps it simple).
 */
void writeCrashLogToDesktop(const juce::String& appName,
                            const juce::String& reason,
                            const juce::String& details = {});

} // namespace cassette
