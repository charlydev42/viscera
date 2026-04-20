// Logger.cpp — see header.
#include "Logger.h"

namespace bb {

// ─ Log directory resolution ──────────────────────────────────────────

juce::File Logger::getLogDirectory()
{
    juce::File base;
   #if JUCE_MAC
    base = juce::File::getSpecialLocation(juce::File::userHomeDirectory)
               .getChildFile("Library").getChildFile("Logs").getChildFile("Parasite");
   #elif JUCE_WINDOWS
    base = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
               .getChildFile("Parasite").getChildFile("logs");
   #else
    base = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
               .getChildFile("Parasite").getChildFile("logs");
   #endif
    base.createDirectory();
    return base;
}

juce::File Logger::getCurrentLogFile()
{
    return getLogDirectory().getChildFile("parasite.log");
}

// ─ Singleton ─────────────────────────────────────────────────────────

Logger& Logger::get()
{
    static Logger instance;
    return instance;
}

Logger::Logger()
    : logFile(getCurrentLogFile())
{
    rotateIfNeeded();

    // Mark the session boundary so it's easy to tell where a run started
    // when a user hands us a log.
    juce::String header;
    header << "\n===== Parasite session start "
           << juce::Time::getCurrentTime().toISO8601(true)
           << " (build " << juce::String(__DATE__) << " " << juce::String(__TIME__)
           << ") =====\n";
    logFile.appendText(header);
}

// ─ Rotation ──────────────────────────────────────────────────────────

void Logger::rotateIfNeeded()
{
    if (!logFile.existsAsFile()) return;
    if (logFile.getSize() < kMaxLogBytes) return;

    // Delete the oldest rotated log; slide the rest down by one index.
    auto nameAt = [this](int i) {
        return logFile.getSiblingFile(logFile.getFileName() + "." + juce::String(i));
    };
    nameAt(kKeepRotated).deleteFile();
    for (int i = kKeepRotated - 1; i >= 1; --i)
        nameAt(i).moveFileTo(nameAt(i + 1));
    logFile.moveFileTo(nameAt(1));
}

// ─ Writing ───────────────────────────────────────────────────────────

const char* Logger::levelTag(LogLevel l) noexcept
{
    switch (l)
    {
        case LogLevel::Info:  return "INFO ";
        case LogLevel::Warn:  return "WARN ";
        case LogLevel::Error: return "ERROR";
    }
    return "???? ";
}

void Logger::log(LogLevel level, const juce::String& message)
{
    // Format off-lock to keep the critical section small.
    juce::String line;
    line << juce::Time::getCurrentTime().formatted("%Y-%m-%d %H:%M:%S")
         << " [" << levelTag(level) << "] "
         << message
         << "\n";

    const juce::SpinLock::ScopedLockType lock(writeLock);
    logFile.appendText(line);
}

// ─ Crash handler ─────────────────────────────────────────────────────

static void crashHandler(void* /*platformSpecific*/)
{
    // Called from a crashed thread — be minimal. No heap allocation beyond
    // what juce::String/File absolutely need, no custom formatting beyond
    // what juce::SystemStats already does for us.
    auto stack = juce::SystemStats::getStackBacktrace();
    auto when  = juce::Time::getCurrentTime().formatted("%Y%m%d-%H%M%S");
    auto file  = Logger::getLogDirectory()
                     .getChildFile("crash-" + when + ".log");

    juce::String body;
    body << "===== Parasite crash at " << juce::Time::getCurrentTime().toISO8601(true) << " =====\n"
         << "Build:       " << juce::String(__DATE__) << " " << juce::String(__TIME__) << "\n"
         << "OS:          " << juce::SystemStats::getOperatingSystemName() << "\n"
         << "CPU:         " << juce::SystemStats::getCpuVendor()
                            << " / " << juce::String(juce::SystemStats::getNumCpus()) << " cores\n"
         << "Memory (MB): " << juce::String(juce::SystemStats::getMemorySizeInMegabytes()) << "\n"
         << "\n-- Stack --\n"
         << stack
         << "\n";
    file.appendText(body);
}

void Logger::installCrashHandler()
{
    static std::atomic<bool> installed { false };
    if (installed.exchange(true, std::memory_order_acq_rel))
        return; // already installed
    juce::SystemStats::setApplicationCrashHandler(&crashHandler);
}

} // namespace bb
