// Logger.h — Persistent rotating logger + application crash handler.
//
// Why: DBG() only works in Debug builds, so production crashes / preset
// failures / cloud sync errors leave no trace. This writes to a real file
// so users can hand a log to support when something goes wrong.
//
// Log location:
//   macOS:   ~/Library/Logs/Parasite/parasite.log (+.1 .. .4 rotated)
//   Windows: %APPDATA%/Parasite/logs/parasite.log
//   Linux:   ~/.local/share/Parasite/logs/parasite.log
//
// Crash dumps: parallel file crash-YYYYMMDD-HHMMSS.log beside the main log.
#pragma once
#include <juce_core/juce_core.h>

namespace bb {

enum class LogLevel
{
    Info,
    Warn,
    Error
};

class Logger
{
public:
    // Lazy singleton — constructs on first call, thread-safe (Meyers).
    static Logger& get();

    // Install juce::SystemStats crash handler so a native crash writes a
    // separate timestamped dump beside the main log. Idempotent — call
    // once from the processor constructor.
    static void installCrashHandler();

    // Non-blocking: takes a short SpinLock for the fwrite. Don't call from
    // the audio thread (formatting allocates). Safe from any other thread.
    void log(LogLevel level, const juce::String& message);

    void info (const juce::String& m) { log(LogLevel::Info,  m); }
    void warn (const juce::String& m) { log(LogLevel::Warn,  m); }
    void error(const juce::String& m) { log(LogLevel::Error, m); }

    // Resolved log directory (created on demand). Useful to attach / open
    // from GUI ("Reveal log folder…").
    static juce::File getLogDirectory();
    static juce::File getCurrentLogFile();

private:
    Logger();
    ~Logger() = default;
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    // Rotate at startup if the current log exceeds kMaxLogBytes.
    static constexpr int64_t kMaxLogBytes = 1 * 1024 * 1024; // 1 MiB
    static constexpr int     kKeepRotated = 4;                // .1 .. .4

    void rotateIfNeeded();
    static const char* levelTag(LogLevel l) noexcept;

    juce::File logFile;
    juce::SpinLock writeLock; // fwrite is fast; SpinLock is fine here
};

// Convenience one-liners so call sites stay short.
#define BB_LOG_INFO(msg)  ::bb::Logger::get().info (msg)
#define BB_LOG_WARN(msg)  ::bb::Logger::get().warn (msg)
#define BB_LOG_ERROR(msg) ::bb::Logger::get().error(msg)

} // namespace bb
