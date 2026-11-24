#pragma once
// How long the GPU's engines ran this process's work, for test tools only (the hardware attribution method, plan W0.7, and
// the frame timing method): the per-process "GPU Engine" performance counters' "Running Time", read through PDH without
// elevation. Nothing here is library code.
#include <intrin.h>
#include <pdh.h>
#include <pdhmsg.h>
#include <windows.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cwchar>
#include <span>
#include <string>
#include <string_view>
#include <vector>
#include <wil/resource.h>

#pragma comment(lib, "pdh.lib")

// The query is owned in place and never copied. Owner: DxUi maintainers; review: 2027-01-31.
#pragma warning(push)
#pragma warning(disable : 4625 4626 5026 5027)
namespace NativeWorkTests
{
// How long the GPU's engines ran processes' work: the "GPU Engine" performance counters' "Running Time" (100 ns units), one
// value per engine instance ("pid_<pid>_luid_0x<high>_0x<low>_phys_<n>_eng_<n>_engtype_<type>"), read without elevation.
// Timestamps around a CPU-bound frame give the GPU's elapsed timeline, which includes the GPU waiting for the CPU's next
// submission; the running time does not. A software adapter has no instance.
//
// Scope: Open() reads this process's instances (the attribution's own GPU busy time); Open(true) reads every process's, for
// the frame timing method's admission (other processes' GPU busy time on the measured adapter, DeltaByEngine).
class GpuEngineTime final
{
public:
    struct Instance
    {
        std::wstring name;
        uint64_t value100ns = 0u;
    };
    // A snapshot keyed by engine instance name (Read), or a delta (Delta).
    struct Reading
    {
        uint64_t total100ns    = 0u;
        uint64_t threeD100ns   = 0u; // engines of type 3D
        uint32_t instances     = 0u;
        uint32_t newInstances  = 0u; // delta only: instances absent from the first snapshot
        uint32_t goneInstances = 0u; // delta only: instances absent from the second snapshot, left out
        uint32_t resets        = 0u; // delta only: instances whose running time went backwards, left out
        bool valid             = false;
        std::vector<Instance> values;
    };
    // An instance absent from the first snapshot started inside the window: its whole value lies in the window only when
    // the caller knows it started then (this process's own engines after the first snapshot). By default it is left out,
    // as is any instance absent from the second snapshot, so a process that started or ended in the window is never charged
    // with time outside it.
    enum class NewInstances : uint8_t
    {
        Drop,
        CountWhole,
    };
    // The parts of an instance name.
    struct InstanceName
    {
        DWORD pid = 0u;
        std::wstring_view luid;   // "0x<high>_0x<low>"
        std::wstring_view engine; // everything after the process: "luid_..._engtype_<type>"
        std::wstring_view type;   // "3D", "Copy", "VideoDecode"...
        bool valid = false;
    };
    [[nodiscard]] static InstanceName Parse(std::wstring_view name) noexcept
    {
        InstanceName parsed;
        constexpr std::wstring_view kPid = L"pid_", kLuid = L"_luid_", kPhys = L"_phys_", kType = L"_engtype_";
        if (! name.starts_with(kPid))
            return parsed;
        const size_t luid = name.find(kLuid);
        const size_t phys = name.find(kPhys);
        const size_t type = name.find(kType);
        if (luid == std::wstring_view::npos || phys == std::wstring_view::npos || type == std::wstring_view::npos || phys < luid)
            return parsed;
        for (size_t index = kPid.size(); index < luid; ++index)
        {
            if (name[index] < L'0' || name[index] > L'9')
                return parsed;
            parsed.pid = parsed.pid * 10u + static_cast<DWORD>(name[index] - L'0');
        }
        parsed.luid   = name.substr(luid + kLuid.size(), phys - luid - kLuid.size());
        parsed.engine = name.substr(luid + 1u);
        parsed.type   = name.substr(type + kType.size());
        parsed.valid  = true;
        return parsed;
    }

    // Opens the query; a process's engine instances exist once its device has been created.
    bool Open(bool allProcesses = false)
    {
        PDH_HQUERY query = nullptr;
        if (PdhOpenQueryW(nullptr, 0u, &query) != ERROR_SUCCESS)
            return false;
        query_.reset(query);
        const std::wstring path = allProcesses ? std::wstring(L"\\GPU Engine(*)\\Running Time")
                                               : L"\\GPU Engine(pid_" + std::to_wstring(GetCurrentProcessId()) + L"_*)\\Running Time";
        return PdhAddEnglishCounterW(query_.get(), path.c_str(), 0u, &counter_) == ERROR_SUCCESS;
    }
    [[nodiscard]] Reading Read()
    {
        Reading reading;
        if (! query_ || PdhCollectQueryData(query_.get()) != ERROR_SUCCESS)
            return reading;
        DWORD size  = 0u;
        DWORD count = 0u;
        if (PdhGetRawCounterArrayW(counter_, &size, &count, nullptr) != static_cast<PDH_STATUS>(PDH_MORE_DATA) || size == 0u)
            return reading;
        std::vector<std::byte> buffer(size);
        auto* const items = reinterpret_cast<PDH_RAW_COUNTER_ITEM_W*>(buffer.data());
        if (PdhGetRawCounterArrayW(counter_, &size, &count, items) != ERROR_SUCCESS)
            return reading;
        for (DWORD index = 0u; index < count; ++index)
        {
            const PDH_RAW_COUNTER& raw = items[index].RawValue;
            // PDH_RAW_COUNTER::CStatus is `volatile` in the SDK: one explicit load, no ARM64 /volatile:iso dependence (C4746).
            const auto status = static_cast<DWORD>(__iso_volatile_load32(reinterpret_cast<const volatile int*>(&raw.CStatus)));
            if ((status != PDH_CSTATUS_VALID_DATA && status != PDH_CSTATUS_NEW_DATA) || ! items[index].szName || raw.FirstValue < 0)
                continue;
            reading.values.push_back(Instance{items[index].szName, static_cast<uint64_t>(raw.FirstValue)});
            ++reading.instances;
        }
        std::sort(reading.values.begin(), reading.values.end(), [](const Instance& left, const Instance& right) { return left.name < right.name; });
        reading.valid = true;
        return reading;
    }
    [[nodiscard]] static const Instance* Find(const Reading& reading, std::wstring_view name) noexcept
    {
        const auto found = std::lower_bound(reading.values.begin(), reading.values.end(), name, [](const Instance& value, std::wstring_view key) {
            return std::wstring_view(value.name) < key;
        });
        return found != reading.values.end() && found->name == name ? &*found : nullptr;
    }
    // Running time between two snapshots, engine instance by instance (instances come and go, so totals cannot be subtracted).
    [[nodiscard]] static Reading Delta(const Reading& start, const Reading& end, NewInstances newInstances = NewInstances::Drop)
    {
        Reading delta;
        delta.valid = start.valid && end.valid;
        if (! delta.valid)
            return delta;
        for (const Instance& earlier : start.values)
            delta.goneInstances += Find(end, earlier.name) ? 0u : 1u;
        for (const Instance& later : end.values)
        {
            const Instance* const earlier = Find(start, later.name);
            uint64_t value                = later.value100ns;
            if (! earlier)
            {
                ++delta.newInstances;
                if (newInstances == NewInstances::Drop)
                    continue;
            }
            else if (later.value100ns < earlier->value100ns)
            {
                ++delta.resets;
                continue;
            }
            else
                value -= earlier->value100ns;
            delta.total100ns += value;
            if (Parse(later.name).type == L"3D")
                delta.threeD100ns += value;
            delta.values.push_back(Instance{later.name, value});
            ++delta.instances;
        }
        return delta;
    }

    // Busy time per engine of one adapter between two all-process snapshots (Open(true)): this process's and every other
    // process's, leaving out the excluded processes (a measured child, an observer). Instances absent from either snapshot
    // are left out and counted in the reading's newInstances and goneInstances.
    struct EngineBusy
    {
        std::wstring engine; // "luid_..._phys_<n>_eng_<n>_engtype_<type>"
        std::wstring type;
        uint64_t own100ns    = 0u;
        uint64_t others100ns = 0u;
    };
    [[nodiscard]] static std::vector<EngineBusy> DeltaByEngine(
        const Reading& start, const Reading& end, std::wstring_view luid, DWORD ownProcess, std::span<const DWORD> excludedProcesses, Reading* counts = nullptr)
    {
        const Reading delta = Delta(start, end, NewInstances::Drop);
        if (counts)
            *counts = Reading{delta.total100ns, delta.threeD100ns, delta.instances, delta.newInstances, delta.goneInstances, delta.resets, delta.valid, {}};
        std::vector<EngineBusy> engines;
        for (const Instance& instance : delta.values)
        {
            const InstanceName name = Parse(instance.name);
            if (! name.valid || name.luid.size() != luid.size() || _wcsnicmp(name.luid.data(), luid.data(), luid.size()) != 0 ||
                std::find(excludedProcesses.begin(), excludedProcesses.end(), name.pid) != excludedProcesses.end())
                continue;
            auto engine = std::find_if(engines.begin(), engines.end(), [&](const EngineBusy& value) { return value.engine == name.engine; });
            if (engine == engines.end())
            {
                engines.push_back(EngineBusy{std::wstring(name.engine), std::wstring(name.type), 0u, 0u});
                engine = std::prev(engines.end());
            }
            (name.pid == ownProcess ? engine->own100ns : engine->others100ns) += instance.value100ns;
        }
        return engines;
    }

private:
    using unique_pdh_query = wil::unique_any<PDH_HQUERY, decltype(&::PdhCloseQuery), ::PdhCloseQuery>;
    unique_pdh_query query_;
    PDH_HCOUNTER counter_ = nullptr;
};
} // namespace NativeWorkTests
#pragma warning(pop)
