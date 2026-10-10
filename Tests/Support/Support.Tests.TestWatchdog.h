#pragma once

#include <Windows.h>

#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <mutex>
#include <thread>

namespace DxUi::TestSupport
{
// The exit code of a run the watchdog ended: the one of timeout(1), distinct from a failed check (1) and a usage error (2).
inline constexpr UINT kTestTimeoutExitCode = 124u;

// What each test may run before the watchdog ends the run. The slowest test of the non-foreground suites takes 2.5 s in x64
// Debug and 7.5 s in x64 ASan Debug on the machine that measured it (Testing_Validation.md has the measurements): 300 s is
// forty times the slowest of them and about five times a test that waits out every allowance of a cold UI Automation client,
// and it still ends a hung CI job in five minutes instead of at the job's 40-minute limit.
inline constexpr unsigned kDefaultTestTimeoutSeconds = 300u;

// Ends the process with its stuck test's name instead of letting a hung test hold a CI job until the job's time limit: on the
// pull-request run of PR 30 the x64 ASan Debug job printed nothing for the 35 minutes between starting the Menu suite and its
// cancellation, and nothing named the test it was in.
//
// A unit of work (one test, or a fixture suite that has no named tests) is armed under a name before it starts and disarmed
// when it ends. One watchdog thread, started by the first arming, waits for the armed deadline on a condition variable with a
// timeout: it never polls, and with nothing armed it sleeps until something is. When a unit outlives its deadline the watchdog
// writes "TIMEOUT: <name> after <N> s" to the stderr handle and terminates the process with kTestTimeoutExitCode. The test
// thread is stuck, possibly holding locks other threads need, so the process is terminated instead of unwound, and the line
// bypasses the C++ streams because the stuck thread may hold their locks. What the process still buffered for stdout is lost;
// the markers and failure lines go to stderr, which is unbuffered. Units do not nest.
class TestWatchdog final
{
public:
    TestWatchdog(const TestWatchdog&)            = delete;
    TestWatchdog& operator=(const TestWatchdog&) = delete;
    TestWatchdog(TestWatchdog&&)                 = delete;
    TestWatchdog& operator=(TestWatchdog&&)      = delete;

    // Never destroyed: it has to keep watching while the process exits, so a hang inside exit() ends as well.
    [[nodiscard]] static TestWatchdog& Instance()
    {
        static TestWatchdog* const instance = new TestWatchdog();
        return *instance;
    }

    // Sets the deadline of every unit armed from now on; zero turns the watchdog off.
    void SetTimeout(std::chrono::seconds timeout)
    {
        const std::lock_guard lock(_mutex);
        _timeout = timeout;
    }

    // Starts the deadline of the unit `name`, a string that outlives the unit (a test or suite name).
    void Arm(const char* name)
    {
        {
            const std::lock_guard lock(_mutex);
            if (_timeout.count() <= 0)
                return;
            if (_name != nullptr)
            {
                std::fprintf(stderr, "FAILED: test deadlines do not nest (%s inside %s)\n", name, _name);
                std::exit(1);
            }
            _name     = name;
            _limit    = _timeout;
            _deadline = std::chrono::steady_clock::now() + _limit;
            ++_generation;
            if (! _watching)
            {
                std::thread([this] { Watch(); }).detach();
                _watching = true;
            }
        }
        _changed.notify_one();
    }

    // The armed unit has made progress: its deadline starts over. A fixture that repeats one cycle many times reports each
    // cycle, so its deadline bounds a cycle and not the whole fixture. The waiting thread is not woken: it looks at the
    // deadline again when the one it waits for has passed.
    void Renew()
    {
        const std::lock_guard lock(_mutex);
        if (_name != nullptr)
            _deadline = std::chrono::steady_clock::now() + _limit;
    }

    // The unit ended in time.
    void Disarm()
    {
        {
            const std::lock_guard lock(_mutex);
            if (_name == nullptr)
                return;
            _name = nullptr;
            ++_generation;
        }
        _changed.notify_one();
    }

private:
    TestWatchdog() = default;

    void Watch()
    {
        std::unique_lock lock(_mutex);
        for (;;)
        {
            if (_name == nullptr)
            {
                _changed.wait(lock, [this] { return _name != nullptr; });
                continue;
            }
            const std::uint64_t generation = _generation;
            if (_changed.wait_until(lock, _deadline, [&] { return _generation != generation; }))
                continue; // Armed again or disarmed: look at what is armed now.
            if (std::chrono::steady_clock::now() < _deadline)
                continue; // Renewed while it waited: wait for the new deadline.
            Expire(_name, _limit);
        }
    }

    [[noreturn]] static void Expire(const char* name, std::chrono::seconds limit) noexcept
    {
        char line[512]{};
        const int length = std::snprintf(line, sizeof(line), "TIMEOUT: %s after %lld s\r\n", name, static_cast<long long>(limit.count()));
        if (length > 0)
        {
            DWORD written = 0u;
            static_cast<void>(WriteFile(
                GetStdHandle(STD_ERROR_HANDLE), line, static_cast<DWORD>((std::min)(static_cast<size_t>(length), sizeof(line) - 1u)), &written, nullptr));
        }
        static_cast<void>(TerminateProcess(GetCurrentProcess(), kTestTimeoutExitCode));
        std::abort(); // Only reached if the process could not be terminated; nothing may run on in a hung process.
    }

    std::mutex _mutex;
    std::condition_variable _changed;
    std::chrono::seconds _timeout{0}; // The deadline given to a unit when it is armed.
    std::chrono::seconds _limit{0};   // The deadline of the armed unit.
    const char* _name = nullptr;      // The armed unit, null when nothing is armed.
    std::chrono::steady_clock::time_point _deadline{};
    std::uint64_t _generation = 0u; // Advanced by every arming and disarming so the waiting thread looks again.
    bool _watching            = false;
};

// Puts the unit of work `name` under the watchdog's deadline while it lives.
class ScopedTestDeadline final
{
public:
    explicit ScopedTestDeadline(const char* name)
    {
        TestWatchdog::Instance().Arm(name);
    }

    ~ScopedTestDeadline()
    {
        TestWatchdog::Instance().Disarm();
    }

    ScopedTestDeadline(const ScopedTestDeadline&)            = delete;
    ScopedTestDeadline& operator=(const ScopedTestDeadline&) = delete;
    ScopedTestDeadline(ScopedTestDeadline&&)                 = delete;
    ScopedTestDeadline& operator=(ScopedTestDeadline&&)      = delete;
};
} // namespace DxUi::TestSupport
