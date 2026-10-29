#pragma once

#include "../../Samples/ComplexUi/ComplexUiScene.h"
#include "../../Samples/EmbeddedControls/GraphicsFixture.h"
#include <algorithm>
#include <array>
#include <atomic>
#include <bit>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <limits>
#include <numeric>
#include <random>
#include <string_view>
#include <vector>

// Opt-in diagnostic for the QPC resolution floor of a clean/no-op Prepare call.
// Kept outside Embedded.Tests.BenchmarkMain.h so the canonical six-input workload stays unchanged.
// DxUi.EmbeddedTests.exe --benchmark-clean-prepare-floor <report.json> <seed>
// DxUi.EmbeddedTests.exe --test-clean-prepare-timer-oracle
namespace CleanPrepareTimerDiagnostic
{
inline constexpr std::string_view kDiagnosticId   = "dxui-clean-prepare-timer-v1";
inline constexpr std::string_view kFixtureId      = "dxui-complex-ui-v2";
inline constexpr std::string_view kSceneSource    = "Samples/ComplexUi/ComplexUiScene.h";
inline constexpr std::string_view kGraphicsSource = "Samples/EmbeddedControls/GraphicsFixture.h";
inline constexpr std::array<size_t, 4> kBatchSizes{1u, 8u, 64u, 512u};
inline constexpr size_t kRepetitionsPerSize = 16u;
inline constexpr size_t kWarmupCalls        = 20u;

struct ViewSnapshot
{
    DxUi::EmbeddedStatistics statistics{};
    bool needsPreparation   = false;
    bool needsAnimation     = false;
    uint64_t sourceHash     = 0;
    uint64_t sceneStateHash = 0;
};

struct BatchSample
{
    size_t batchSize          = 0;
    size_t repetition         = 0;
    size_t groupPosition      = 0;
    bool preparationFirst     = false;
    bool isPreparation        = false;
    uint64_t callCount        = 0;
    uint64_t loopIterations   = 0;
    int64_t elapsedTicks      = 0;
    double ticksPerCall       = 0;
    double nanosecondsPerCall = 0;
    ViewSnapshot before{};
    ViewSnapshot after{};
};

[[nodiscard]] inline uint64_t HashSceneSource(const ComplexUiModel& model) noexcept
{
    // FNV-1a over the model's UTF-16 code units, encoded little-endian.
    uint64_t hash  = 14695981039346656037ull;
    const auto add = [&hash](uint8_t value)
    {
        hash ^= value;
        hash *= 1099511628211ull;
    };
    add(model.multilineGrid ? 1u : 0u);
    for (const auto& name : model.names)
    {
        for (const wchar_t unit : name)
        {
            const auto value = static_cast<uint16_t>(unit);
            add(static_cast<uint8_t>(value & 0xFFu));
            add(static_cast<uint8_t>(value >> 8u));
        }
        add(0u);
        add(0u);
    }
    for (const auto& text : model.cellTexts)
    {
        for (const wchar_t unit : text)
        {
            const auto value = static_cast<uint16_t>(unit);
            add(static_cast<uint8_t>(value & 0xFFu));
            add(static_cast<uint8_t>(value >> 8u));
        }
        add(0u);
        add(0u);
    }
    return hash;
}

[[nodiscard]] inline uint64_t HashSceneState(const ComplexUiScene& scene) noexcept
{
    uint64_t hash    = HashSceneSource(scene.model);
    const auto add64 = [&hash](uint64_t value)
    {
        for (unsigned int shift = 0; shift < 64u; shift += 8u)
        {
            hash ^= static_cast<uint8_t>(value >> shift);
            hash *= 1099511628211ull;
        }
    };
    for (const auto* slider : scene.sliders)
        add64(std::bit_cast<uint64_t>(slider->GetValue()));
    for (const auto* progress : scene.progress)
        add64(std::bit_cast<uint64_t>(progress->GetValue()));
    const auto& selection = scene.grid->GetSelectionModel();
    for (const uint64_t rowId : selection.GetOrderedSelection())
        add64(rowId);
    add64(selection.GetCount());
    add64(selection.GetAnchor().value_or(std::numeric_limits<uint64_t>::max()));
    add64(scene.grid->GetFocusedRowId().value_or(std::numeric_limits<uint64_t>::max()));
    return hash;
}

[[nodiscard]] inline ViewSnapshot CaptureSnapshot(const ComplexUiScene& scene) noexcept
{
    return {scene.view.GetStatistics(), scene.view.NeedsPreparation(), scene.view.NeedsAnimation(), HashSceneSource(scene.model), HashSceneState(scene)};
}

[[nodiscard]] inline bool SamePreparedState(const ViewSnapshot& left, const ViewSnapshot& right) noexcept
{
    const auto& a = left.statistics;
    const auto& b = right.statistics;
    return a.preparations == b.preparations && a.composites == b.composites && a.surfaceBytes == b.surfaceBytes &&
           a.replacementPeakBytes == b.replacementPeakBytes && a.surfaceAllocations == b.surfaceAllocations && a.cachedBrushes == b.cachedBrushes &&
           a.cachedTextFormats == b.cachedTextFormats && left.needsPreparation == right.needsPreparation && left.needsAnimation == right.needsAnimation &&
           left.sourceHash == right.sourceHash && left.sceneStateHash == right.sceneStateHash;
}

[[nodiscard]] __declspec(noinline) inline size_t RunEmptyLoopReference(size_t calls) noexcept
{
    size_t completed = 0;
    for (size_t index = 0; index < calls; ++index)
    {
        std::atomic_signal_fence(std::memory_order_seq_cst);
        ++completed;
    }
    std::atomic_signal_fence(std::memory_order_seq_cst);
    return completed;
}

struct QpcSample
{
    int64_t ticks = 0;
    bool valid    = false;
};

[[nodiscard]] inline QpcSample ReadQpc() noexcept
{
    LARGE_INTEGER value{};
    const BOOL valid = QueryPerformanceCounter(&value);
    return {value.QuadPart, valid != FALSE};
}

[[nodiscard]] constexpr bool AccumulateCleanNoopResult(bool allNoopResults, HRESULT result) noexcept
{
    return allNoopResults && result == S_FALSE;
}

[[nodiscard]] inline BatchSample MeasureBatch(
    ComplexUiScene& scene, size_t batchSize, size_t repetition, size_t groupPosition, bool preparationFirst, bool isPreparation, uint64_t frequency)
{
    BatchSample result{};
    result.batchSize        = batchSize;
    result.repetition       = repetition;
    result.groupPosition    = groupPosition;
    result.preparationFirst = preparationFirst;
    result.isPreparation    = isPreparation;
    result.callCount        = batchSize;
    result.before           = CaptureSnapshot(scene);
    const QpcSample start   = ReadQpc();
    HRESULT lastResult      = S_OK;
    bool allNoopResults     = true;
    if (isPreparation)
    {
        for (size_t call = 0; call < batchSize; ++call)
        {
            lastResult     = scene.view.Prepare(1280u, 720u);
            allNoopResults = AccumulateCleanNoopResult(allNoopResults, lastResult);
            if (FAILED(lastResult))
                break;
            ++result.loopIterations;
        }
    }
    else
        result.loopIterations = RunEmptyLoopReference(batchSize);
    const QpcSample end = ReadQpc();
    result.elapsedTicks = end.ticks - start.ticks;
    result.after        = CaptureSnapshot(scene);
    if (isPreparation)
        Check(allNoopResults, "every clean Prepare diagnostic batch call returns S_FALSE");
    Check(start.valid && end.valid, "clean Prepare diagnostic QPC samples succeed");
    Check(SUCCEEDED(lastResult), "clean Prepare diagnostic calls succeed");
    Check(result.loopIterations == batchSize, "clean Prepare diagnostic exact batch call count");
    Check(end.ticks >= start.ticks, "clean Prepare diagnostic QPC is monotonic");
    Check(result.after.statistics.preparations == result.before.statistics.preparations, "clean Prepare diagnostic adds no preparation");
    Check(result.after.statistics.composites == result.before.statistics.composites, "clean Prepare diagnostic adds no composition");
    Check(SamePreparedState(result.before, result.after), "clean Prepare diagnostic retains exact prepared state and source");
    result.ticksPerCall       = static_cast<double>(result.elapsedTicks) / static_cast<double>(batchSize);
    result.nanosecondsPerCall = result.ticksPerCall * 1.0e9 / static_cast<double>(frequency);
    return result;
}

inline void ValidateSamples(const std::vector<BatchSample>& samples, uint64_t frequency, size_t repetitions)
{
    constexpr size_t kKindsPerGroup = 2u;
    Check(frequency > 0, "clean Prepare diagnostic positive QPC frequency");
    Check(samples.size() == kBatchSizes.size() * repetitions * kKindsPerGroup, "clean Prepare diagnostic complete sample inventory");
    uint64_t preparationCalls     = 0;
    uint64_t referenceCalls       = 0;
    int64_t totalPreparationTicks = 0;
    int64_t totalReferenceTicks   = 0;
    for (const auto& sample : samples)
    {
        Check(std::ranges::find(kBatchSizes, sample.batchSize) != kBatchSizes.end(), "clean Prepare diagnostic known batch size");
        Check(sample.repetition < repetitions, "clean Prepare diagnostic repetition index");
        Check(sample.callCount == sample.batchSize && sample.loopIterations == sample.callCount, "clean Prepare diagnostic calls and loop count agree");
        Check(sample.elapsedTicks >= 0, "clean Prepare diagnostic sample QPC ticks nonnegative");
        const double expectedTicks = static_cast<double>(sample.elapsedTicks) / static_cast<double>(sample.callCount);
        const double expectedNs    = expectedTicks * 1.0e9 / static_cast<double>(frequency);
        Check(std::abs(sample.ticksPerCall - expectedTicks) <= 1.0e-12, "clean Prepare diagnostic ticks normalization");
        Check(std::abs(sample.nanosecondsPerCall - expectedNs) <= (std::max)(1.0e-12, std::abs(expectedNs) * 1.0e-12),
              "clean Prepare diagnostic nanoseconds normalization");
        if (sample.isPreparation)
        {
            preparationCalls += sample.callCount;
            totalPreparationTicks += sample.elapsedTicks;
        }
        else
        {
            referenceCalls += sample.callCount;
            totalReferenceTicks += sample.elapsedTicks;
        }
    }
    const uint64_t expectedCalls = static_cast<uint64_t>(repetitions) * std::accumulate(kBatchSizes.begin(), kBatchSizes.end(), uint64_t{0});
    Check(preparationCalls == expectedCalls && referenceCalls == expectedCalls, "clean Prepare diagnostic aggregate calls and ticks retained");
    Check(totalPreparationTicks >= 0 && totalReferenceTicks >= 0, "clean Prepare diagnostic aggregate QPC ticks nonnegative");
    for (const size_t batchSize : kBatchSizes)
    {
        for (const bool preparation : {false, true})
        {
            size_t sampleCount       = 0;
            uint64_t callSum         = 0;
            int64_t tickSum          = 0;
            double normalizedTickSum = 0;
            for (const auto& sample : samples)
            {
                if (sample.batchSize == batchSize && sample.isPreparation == preparation)
                {
                    ++sampleCount;
                    callSum += sample.callCount;
                    tickSum += sample.elapsedTicks;
                    normalizedTickSum += sample.ticksPerCall * static_cast<double>(sample.callCount);
                }
            }
            Check(sampleCount == repetitions && callSum == static_cast<uint64_t>(batchSize * repetitions),
                  "clean Prepare diagnostic per-group sample and call sums");
            Check(std::abs(static_cast<double>(tickSum) - normalizedTickSum) <= (std::max)(1.0, std::abs(normalizedTickSum) * 1.0e-12),
                  "clean Prepare diagnostic per-group QPC tick sum normalization");
        }
    }
}

inline void PrepareFixture(ComplexUiScene& scene, GraphicsFixture& gpu)
{
    gpu.width  = 1280u;
    gpu.height = 720u;
    Hr(gpu.Create(), "clean Prepare diagnostic WARP device");
    Hr(scene.Initialize(gpu.device.get()), "clean Prepare diagnostic complex UI fixture");
    Hr(PrepareRichEmbeddedContent(scene.view, 1280u, 720u), "clean Prepare diagnostic initial fixture preparation");
    gpu.Bind();
    Hr(scene.view.Composite(gpu.context.get(), gpu.Viewport()), "clean Prepare diagnostic initial fixture composition");
    Check(! scene.view.NeedsPreparation(), "clean Prepare diagnostic fixture starts prepared");
    const auto prepared       = CaptureSnapshot(scene);
    bool allWarmupNoopResults = true;
    for (size_t index = 0; index < kWarmupCalls; ++index)
    {
        const HRESULT warmupResult = scene.view.Prepare(1280u, 720u);
        allWarmupNoopResults       = AccumulateCleanNoopResult(allWarmupNoopResults, warmupResult);
        Check(allWarmupNoopResults, "every clean Prepare diagnostic warmup returns S_FALSE");
    }
    Check(SamePreparedState(prepared, CaptureSnapshot(scene)), "clean Prepare diagnostic warmup keeps prepared state and source");
}

inline void RunOracle()
{
    constexpr uint64_t kOracleFrequency = 10'000'000u;
    Check(AccumulateCleanNoopResult(true, S_FALSE), "clean Prepare no-op result tracker accepts S_FALSE");
    const bool earlierSuccessThenNoop = AccumulateCleanNoopResult(AccumulateCleanNoopResult(true, S_OK), S_FALSE);
    Check(! earlierSuccessThenNoop, "clean Prepare result tracker rejects earlier S_OK followed by final S_FALSE");
    Check(! AccumulateCleanNoopResult(AccumulateCleanNoopResult(true, E_FAIL), S_FALSE), "clean Prepare result tracker retains an earlier failure");
    Check(! AccumulateCleanNoopResult(true, static_cast<HRESULT>(2)), "clean Prepare result tracker rejects other successful results");
    Check(! AccumulateCleanNoopResult(false, S_FALSE), "clean Prepare result tracker never resets a rejected result");

    std::vector<BatchSample> oracleSamples;
    oracleSamples.reserve(kBatchSizes.size() * 2u);
    for (const size_t batchSize : kBatchSizes)
    {
        for (const bool preparation : {false, true})
        {
            BatchSample sample{};
            sample.batchSize          = batchSize;
            sample.callCount          = batchSize;
            sample.loopIterations     = batchSize;
            sample.isPreparation      = preparation;
            sample.elapsedTicks       = static_cast<int64_t>(batchSize * 11u);
            sample.ticksPerCall       = static_cast<double>(sample.elapsedTicks) / static_cast<double>(sample.callCount);
            sample.nanosecondsPerCall = sample.ticksPerCall * 1.0e9 / static_cast<double>(kOracleFrequency);
            oracleSamples.push_back(sample);
        }
    }
    ValidateSamples(oracleSamples, kOracleFrequency, 1u);

    GraphicsFixture gpu;
    ComplexUiScene scene;
    PrepareFixture(scene, gpu);
    const auto prepared = CaptureSnapshot(scene);
    LARGE_INTEGER frequency{};
    Check(QueryPerformanceFrequency(&frequency) != FALSE && frequency.QuadPart > 0, "clean Prepare diagnostic oracle QPC frequency");
    std::vector<BatchSample> measured;
    measured.reserve(kBatchSizes.size() * 2u);
    for (const size_t batchSize : kBatchSizes)
    {
        measured.push_back(MeasureBatch(scene, batchSize, 0u, 0u, true, true, static_cast<uint64_t>(frequency.QuadPart)));
        measured.push_back(MeasureBatch(scene, batchSize, 0u, 0u, true, false, static_cast<uint64_t>(frequency.QuadPart)));
        Check(SamePreparedState(prepared, CaptureSnapshot(scene)), "clean Prepare diagnostic oracle keeps exact prepared state and source");
    }
    ValidateSamples(measured, static_cast<uint64_t>(frequency.QuadPart), 1u);
    Check(prepared.sceneStateHash == CaptureSnapshot(scene).sceneStateHash, "clean Prepare diagnostic oracle retains fixture source and UI state");
}

[[nodiscard]] inline bool TryParseSeed(std::wstring_view text, uint64_t& seed) noexcept
{
    if (text.empty())
        return false;
    uint64_t value = 0;
    for (const wchar_t ch : text)
    {
        if (ch < L'0' || ch > L'9')
            return false;
        const uint64_t digit = static_cast<uint64_t>(ch - L'0');
        if (value > (std::numeric_limits<uint64_t>::max() - digit) / 10u)
            return false;
        value = value * 10u + digit;
    }
    seed = value;
    return true;
}

inline void WriteReport(
    const std::filesystem::path& path, uint64_t seed, uint64_t frequency, const std::array<size_t, 4>& baseOrder, const std::vector<BatchSample>& samples)
{
    std::ofstream output(path, std::ios::binary | std::ios::noreplace);
    Check(static_cast<bool>(output), "clean Prepare diagnostic report opens");
    output << std::setprecision(17) << "{\"schema\":1,\"diagnosticId\":\"" << kDiagnosticId << "\",\"fixtureId\":\"" << kFixtureId << "\",\"sceneSource\":\""
           << kSceneSource << "\",\"graphicsSource\":\"" << kGraphicsSource
           << "\",\"sourceHashAlgorithm\":\"fnv1a64-utf16le\",\"sourceHash64\":" << samples.front().before.sourceHash
           << ",\"sceneStateHashAlgorithm\":\"fnv1a64-scene-values\",\"sceneStateHash64\":" << samples.front().before.sceneStateHash << ",\"seed\":" << seed
           << ",\"qpcFrequency\":" << frequency << ",\"qpcTickNanoseconds\":" << 1.0e9 / static_cast<double>(frequency)
           << ",\"measurementScope\":\"loop bracket including HRESULT checks, S_FALSE result classification and loop bookkeeping; emptyLoop is separate "
              "context, never subtracted\""
           << ",\"isolatedMethodLatency\":false"
           << ",\"warmupNoopCalls\":" << kWarmupCalls << ",\"repetitionsPerBatchSize\":" << kRepetitionsPerSize
           << ",\"batchSizes\":[1,8,64,512],\"baseOrder\":[" << baseOrder[0] << ',' << baseOrder[1] << ',' << baseOrder[2] << ',' << baseOrder[3]
           << "],\"preparedHistory\":\"ComplexUiScene has no edit history; model labels, slider/progress values and grid selection state are bound by "
              "sceneStateHash64 and checked unchanged.\",\"groupSums\":[";
    bool firstGroup = true;
    for (const size_t batchSize : kBatchSizes)
    {
        for (const bool preparation : {false, true})
        {
            size_t sampleCount = 0;
            uint64_t callSum   = 0;
            int64_t tickSum    = 0;
            for (const auto& sample : samples)
            {
                if (sample.batchSize == batchSize && sample.isPreparation == preparation)
                {
                    ++sampleCount;
                    callSum += sample.callCount;
                    tickSum += sample.elapsedTicks;
                }
            }
            if (! firstGroup)
                output << ',';
            firstGroup = false;
            output << "{\"batchSize\":" << batchSize << ",\"kind\":\"" << (preparation ? "prepare" : "emptyLoop") << "\",\"sampleCount\":" << sampleCount
                   << ",\"calls\":" << callSum << ",\"elapsedTicksSum\":" << tickSum
                   << ",\"ticksPerCall\":" << static_cast<double>(tickSum) / static_cast<double>(callSum) << '}';
        }
    }
    output << "],\"samples\":[";
    for (size_t index = 0; index < samples.size(); ++index)
    {
        const auto& sample = samples[index];
        if (index)
            output << ',';
        output << "{\"batchSize\":" << sample.batchSize << ",\"repetition\":" << sample.repetition << ",\"groupPosition\":" << sample.groupPosition
               << ",\"preparationFirst\":" << (sample.preparationFirst ? "true" : "false") << ",\"kind\":\"" << (sample.isPreparation ? "prepare" : "emptyLoop")
               << "\",\"callCount\":" << sample.callCount << ",\"loopIterations\":" << sample.loopIterations << ",\"elapsedTicks\":" << sample.elapsedTicks
               << ",\"ticksPerCall\":" << sample.ticksPerCall << ",\"nanosecondsPerCall\":" << sample.nanosecondsPerCall
               << ",\"preparationsBefore\":" << sample.before.statistics.preparations << ",\"preparationsAfter\":" << sample.after.statistics.preparations
               << ",\"compositesBefore\":" << sample.before.statistics.composites << ",\"compositesAfter\":" << sample.after.statistics.composites
               << ",\"surfaceBytes\":" << sample.after.statistics.surfaceBytes << ",\"surfaceAllocations\":" << sample.after.statistics.surfaceAllocations
               << ",\"cachedBrushes\":" << sample.after.statistics.cachedBrushes << ",\"cachedTextFormats\":" << sample.after.statistics.cachedTextFormats
               << ",\"needsPreparationBefore\":" << (sample.before.needsPreparation ? "true" : "false")
               << ",\"needsPreparationAfter\":" << (sample.after.needsPreparation ? "true" : "false") << ",\"sourceHashBefore\":" << sample.before.sourceHash
               << ",\"sourceHashAfter\":" << sample.after.sourceHash << ",\"sceneStateHashBefore\":" << sample.before.sceneStateHash
               << ",\"sceneStateHashAfter\":" << sample.after.sceneStateHash << '}';
    }
    output << "]}\n";
    output.close();
    Check(static_cast<bool>(output), "clean Prepare diagnostic report flushes");
}

inline void Run(const wchar_t* outputPath, uint64_t seed)
{
    GraphicsFixture gpu;
    ComplexUiScene scene;
    PrepareFixture(scene, gpu);
    LARGE_INTEGER frequencyValue{};
    Check(QueryPerformanceFrequency(&frequencyValue) != FALSE && frequencyValue.QuadPart > 0, "clean Prepare diagnostic QPC frequency");
    const auto frequency = static_cast<uint64_t>(frequencyValue.QuadPart);
    std::array<size_t, 4> baseOrder{0u, 1u, 2u, 3u};
    std::mt19937_64 random(seed);
    std::shuffle(baseOrder.begin(), baseOrder.end(), random);
    std::vector<BatchSample> samples;
    samples.reserve(kBatchSizes.size() * kRepetitionsPerSize * 2u);
    for (size_t repetition = 0; repetition < kRepetitionsPerSize; ++repetition)
    {
        // Each four-repetition cycle rotates every batch size through all four positions.
        for (size_t position = 0; position < kBatchSizes.size(); ++position)
        {
            const size_t orderIndex = (position + repetition % kBatchSizes.size()) % kBatchSizes.size();
            const size_t sizeIndex  = baseOrder[orderIndex];
            const size_t batchSize  = kBatchSizes[sizeIndex];
            const bool prepareFirst = ((repetition + sizeIndex) % 2u) == 0u;
            samples.push_back(MeasureBatch(scene, batchSize, repetition, position, prepareFirst, prepareFirst, frequency));
            samples.push_back(MeasureBatch(scene, batchSize, repetition, position, prepareFirst, ! prepareFirst, frequency));
        }
    }
    ValidateSamples(samples, frequency, kRepetitionsPerSize);
    const auto finalState = CaptureSnapshot(scene);
    Check(samples.front().before.sourceHash == finalState.sourceHash, "clean Prepare diagnostic source hash remains exact");
    Check(samples.front().before.sceneStateHash == finalState.sceneStateHash, "clean Prepare diagnostic source and UI state remain exact");
    WriteReport(outputPath, seed, frequency, baseOrder, samples);
}
} // namespace CleanPrepareTimerDiagnostic
