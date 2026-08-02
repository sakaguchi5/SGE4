#include "DensityUniverseDistributionFixture.h"
#include "../../src/backends/d3d12/runtime/Runtime.h"
#include "../../src/canonical/base/Sha256.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace
{
namespace experiment = sge4::level5::vertical2;

struct Arguments final
{
    bool forceWarp = false;
    bool enableDebugLayer = false;
    bool boundaryRequalification = false;
    std::uint32_t width = 64;
    std::uint32_t height = 64;
    std::uint32_t globalWarmupMinFrames = 12;
    std::uint32_t globalWarmupMaxFrames = 64;
    std::uint32_t warmupMinFrames = 16;
    std::uint32_t warmupMaxFrames = 64;
    std::uint32_t warmupWindowFrames = 8;
    std::uint32_t sampleFrames = 16;
    double warmupTolerance = 0.05;
    double measurementRegimeThreshold = 1.20;
    std::filesystem::path output = "level5_vertical_experiment_v2b.csv";
};

enum class DistributionKind : std::uint32_t
{
    Prefix = 1,
    Suffix = 2,
    UniformStride = 3,
    Clustered4 = 4,
    SeededRandom = 5,
    Full = 6
};

struct Workset final
{
    DistributionKind kind = DistributionKind::Prefix;
    std::string name;
    std::vector<std::uint32_t> members;
    std::uint32_t densityNumerator = 0;
    std::uint32_t densityDenominator = 1;
    std::string digestHex;
    std::uint32_t contiguousRuns = 0;
    std::uint32_t span = 0;
    double meanGap = 0.0;
    std::uint32_t maxGap = 0;
};

struct Observation final
{
    float stateSum = 0.0f;
    float previousStateSum = 0.0f;
    float delta = 0.0f;
    float universe = 0.0f;
};

struct TemporalAggregate final
{
    float stateSum = 0.0f;
    float reserved = 0.0f;
    float combined = 0.0f;
    float universe = 0.0f;
};

struct TextureEvidence final
{
    sge4::base::Digest256 digest{};
    double xSum = 0.0;
};

static_assert(sizeof(Observation) == 16);
static_assert(sizeof(TemporalAggregate) == 16);

struct FrameResult final
{
    Observation observation;
    TemporalAggregate acceptedTemporal;
    TextureEvidence texture;
    sge4::d3d12::TimestampProfileSample stateWriterProfile;
};

enum class FrameEvidenceMode : std::uint32_t
{
    Full = 1,
    WarmupMinimal = 2
};

enum class MeasurementRegimeStatus : std::uint32_t
{
    AcceptedStable = 1,
    MeasurementRegimeTransition = 2,
    SemanticQualificationOnly = 3
};

struct WarmupQualification final
{
    std::uint32_t framesUsed = 0;
    double denseRegimeRatio = 0.0;
    double sparseRegimeRatio = 0.0;
    double elapsedSeconds = 0.0;
};

struct RegimeStatistics final
{
    double firstHalfMedianNs = 0.0;
    double lastHalfMedianNs = 0.0;
    double symmetricRatio = 0.0;
    double leadingMedianNs = 0.0;
    double trailingMedianNs = 0.0;
    double edgeSymmetricRatio = 0.0;
};

struct CaseResult final
{
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    std::uint32_t universe = 0;
    std::string denseComposition;
    std::string sparseComposition;
    Workset workset;
    MeasurementRegimeStatus measurementStatus =
        MeasurementRegimeStatus::AcceptedStable;
    std::uint32_t warmupFramesUsed = 0;
    double warmupDenseRegimeRatio = 0.0;
    double warmupSparseRegimeRatio = 0.0;
    RegimeStatistics denseMeasurementRegime;
    RegimeStatistics sparseMeasurementRegime;
    double denseGpuMedianNs = 0.0;
    double sparseGpuMedianNs = 0.0;
    double denseRecordingMedianNs = 0.0;
    double sparseRecordingMedianNs = 0.0;
    double denseOverSparse = 0.0;
    double loadSeconds = 0.0;
    double fullQualificationSeconds = 0.0;
    double warmupSeconds = 0.0;
    double measurementSeconds = 0.0;
    std::vector<double> denseGpuSamples;
    std::vector<double> sparseGpuSamples;
    std::vector<double> denseRecordingSamples;
    std::vector<double> sparseRecordingSamples;
    std::vector<bool> denseFirst;
};

struct RunTiming final
{
    double candidateBuildSeconds = 0.0;
    double recoverySeconds = 0.0;
    double globalWarmupSeconds = 0.0;
    double caseLoadSeconds = 0.0;
    double fullQualificationSeconds = 0.0;
    double caseWarmupSeconds = 0.0;
    double measurementSeconds = 0.0;
    double evidenceWriteSeconds = 0.0;
    double totalSeconds = 0.0;
};

[[noreturn]] void Fail(std::string message)
{
    throw std::runtime_error(std::move(message));
}

void Require(bool condition, std::string message)
{
    if (!condition) Fail(std::move(message));
}

std::uint32_t ParseU32(std::string_view text, std::string_view option)
{
    try
    {
        const auto value = std::stoul(std::string(text));
        if (value == 0 || value > 65536u)
            Fail(std::string(option) + "の値が範囲外です。");
        return static_cast<std::uint32_t>(value);
    }
    catch (const std::exception&)
    {
        Fail(std::string(option) + "の値を解釈できません。");
    }
}

double ParseDouble(std::string_view text, std::string_view option)
{
    try
    {
        const auto value = std::stod(std::string(text));
        if (!std::isfinite(value) || value <= 0.0)
            Fail(std::string(option) + "の値が範囲外です。");
        return value;
    }
    catch (const std::exception&)
    {
        Fail(std::string(option) + "の値を解釈できません。");
    }
}

Arguments ParseArguments(int argc, char** argv)
{
    Arguments result;
    for (int index = 1; index < argc; ++index)
    {
        const std::string_view argument = argv[index];
        auto takeValue = [&](std::string_view option) -> std::string_view {
            if (index + 1 >= argc) Fail(std::string(option) + "に値がありません。");
            return argv[++index];
        };
        if (argument == "--warp") result.forceWarp = true;
        else if (argument == "--hardware") result.forceWarp = false;
        else if (argument == "--debug-layer") result.enableDebugLayer = true;
        else if (argument == "--boundary-requalification")
            result.boundaryRequalification = true;
        else if (argument == "--width") result.width = ParseU32(takeValue(argument), argument);
        else if (argument == "--height") result.height = ParseU32(takeValue(argument), argument);
        else if (argument == "--global-warmup")
            result.globalWarmupMinFrames = ParseU32(takeValue(argument), argument);
        else if (argument == "--global-warmup-max")
            result.globalWarmupMaxFrames = ParseU32(takeValue(argument), argument);
        else if (argument == "--warmup")
            result.warmupMinFrames = ParseU32(takeValue(argument), argument);
        else if (argument == "--warmup-max")
            result.warmupMaxFrames = ParseU32(takeValue(argument), argument);
        else if (argument == "--warmup-window")
            result.warmupWindowFrames = ParseU32(takeValue(argument), argument);
        else if (argument == "--warmup-tolerance")
            result.warmupTolerance = ParseDouble(takeValue(argument), argument);
        else if (argument == "--regime-threshold")
            result.measurementRegimeThreshold = ParseDouble(takeValue(argument), argument);
        else if (argument == "--samples") result.sampleFrames = ParseU32(takeValue(argument), argument);
        else if (argument == "--output") result.output = std::filesystem::path(takeValue(argument));
        else if (argument == "--quick")
        {
            result.width = 16;
            result.height = 16;
            result.globalWarmupMinFrames = 4;
            result.globalWarmupMaxFrames = 16;
            result.warmupMinFrames = 8;
            result.warmupMaxFrames = 24;
            result.warmupWindowFrames = 4;
            result.sampleFrames = 8;
        }
        else
        {
            Fail("未知の引数です：" + std::string(argument));
        }
    }
    const std::uint64_t universe = static_cast<std::uint64_t>(result.width) * result.height;
    if (universe == 0 || universe > 65536u || universe % 8u != 0u)
        Fail("width × heightは8で割り切れる1～65536のUniverseでなければなりません。");
    if (result.globalWarmupMaxFrames < result.globalWarmupMinFrames ||
        result.warmupMaxFrames < result.warmupMinFrames)
        Fail("warmup最大frame数は最小frame数以上でなければなりません。");
    if (result.warmupWindowFrames < 4u || result.warmupWindowFrames % 2u != 0u ||
        result.warmupWindowFrames > result.globalWarmupMaxFrames ||
        result.warmupWindowFrames > result.warmupMaxFrames)
        Fail("warmup windowは4以上の偶数で、各warmup最大frame数以下でなければなりません。");
    if (result.sampleFrames < 8u || result.sampleFrames % 2u != 0u)
        Fail("measurement sample数は8以上の偶数でなければなりません。");
    if (result.warmupTolerance > 0.25)
        Fail("warmup toleranceは0.25以下でなければなりません。");
    if (result.measurementRegimeThreshold < 1.05 ||
        result.measurementRegimeThreshold > 4.0)
        Fail("measurement regime thresholdは1.05～4.0でなければなりません。");
    if (result.boundaryRequalification && result.forceWarp)
        Fail("Boundary Requalificationは実Hardware専用です。");
    return result;
}

template<class T>
T DecodeFloat4(std::span<const std::byte> bytes, std::string_view label)
{
    static_assert(sizeof(T) == 16);
    Require(bytes.size() == sizeof(T),
        std::string(label) + " Bufferのbyte数が一致しません。");
    T result;
    std::memcpy(&result, bytes.data(), sizeof(result));
    return result;
}

bool NearlyEqual(float left, float right, float tolerance = 0.01f)
{
    const auto scale = std::max({1.0f, std::abs(left), std::abs(right)});
    return std::abs(left - right) <= tolerance * scale;
}

bool Equivalent(const Observation& left, const Observation& right)
{
    return NearlyEqual(left.stateSum, right.stateSum) &&
        NearlyEqual(left.previousStateSum, right.previousStateSum) &&
        NearlyEqual(left.delta, right.delta) &&
        NearlyEqual(left.universe, right.universe);
}

TextureEvidence DecodeTextureEvidence(
    const sge4::d3d12::Texture2DReadback& readback,
    const experiment::CandidateBuild& candidate)
{
    Require(readback.format ==
            sge4::package::d3d12_v13::Format::R32G32B32A32Float &&
            readback.width == candidate.width &&
            readback.height == candidate.height &&
            readback.rowBytes == candidate.width * 16u &&
            readback.bytes.size() ==
                static_cast<std::size_t>(readback.rowBytes) * readback.height,
        "RGBA32F Texture readbackの形状が一致しません。");
    TextureEvidence result;
    result.digest = sge4::base::Sha256(readback.bytes);
    for (std::size_t offset = 0; offset < readback.bytes.size(); offset += 16u)
    {
        float x = 0.0f;
        std::memcpy(&x, readback.bytes.data() + offset, sizeof(x));
        Require(std::isfinite(x), "Texture Observationに非有限値が含まれます。");
        result.xSum += static_cast<double>(x);
    }
    return result;
}

double Median(std::vector<double> values)
{
    Require(!values.empty(), "Median対象sampleが空です。");
    std::ranges::sort(values);
    const auto middle = values.size() / 2;
    if (values.size() % 2 == 1) return values[middle];
    return (values[middle - 1] + values[middle]) * 0.5;
}

double SymmetricRatio(double left, double right)
{
    Require(left > 0.0 && right > 0.0,
        "測定レジーム比較の中央値がzero以下です。");
    return std::max(left / right, right / left);
}

RegimeStatistics AnalyzeRegime(std::span<const double> samples)
{
    Require(samples.size() >= 4u && samples.size() % 2u == 0u,
        "測定レジーム分析には4件以上の偶数sampleが必要です。");
    const auto middle = samples.size() / 2u;
    RegimeStatistics result;
    result.firstHalfMedianNs = Median(std::vector<double>(
        samples.begin(), samples.begin() + static_cast<std::ptrdiff_t>(middle)));
    result.lastHalfMedianNs = Median(std::vector<double>(
        samples.begin() + static_cast<std::ptrdiff_t>(middle), samples.end()));
    result.symmetricRatio = SymmetricRatio(
        result.firstHalfMedianNs, result.lastHalfMedianNs);
    const auto edgeCount = std::min<std::size_t>(4u, samples.size() / 2u);
    result.leadingMedianNs = Median(std::vector<double>(
        samples.begin(), samples.begin() + static_cast<std::ptrdiff_t>(edgeCount)));
    result.trailingMedianNs = Median(std::vector<double>(
        samples.end() - static_cast<std::ptrdiff_t>(edgeCount), samples.end()));
    result.edgeSymmetricRatio = SymmetricRatio(
        result.leadingMedianNs, result.trailingMedianNs);
    return result;
}

bool HasMeasurementRegimeTransition(
    const RegimeStatistics& dense,
    const RegimeStatistics& sparse,
    double threshold)
{
    return dense.symmetricRatio > threshold ||
        sparse.symmetricRatio > threshold ||
        dense.edgeSymmetricRatio > threshold ||
        sparse.edgeSymmetricRatio > threshold;
}

std::string MeasurementStatusName(MeasurementRegimeStatus status)
{
    switch (status)
    {
    case MeasurementRegimeStatus::AcceptedStable: return "AcceptedStable";
    case MeasurementRegimeStatus::MeasurementRegimeTransition:
        return "MeasurementRegimeTransition";
    case MeasurementRegimeStatus::SemanticQualificationOnly:
        return "SemanticQualificationOnly";
    }
    Fail("未知の測定レジーム状態です。");
}

double ElapsedSeconds(
    std::chrono::steady_clock::time_point begin,
    std::chrono::steady_clock::time_point end)
{
    return std::chrono::duration<double>(end - begin).count();
}

struct DensityPoint final
{
    std::uint32_t numerator = 0;
    std::uint32_t denominator = 1;
};

constexpr std::array<DensityPoint, 7> DensityPoints = {{
    {1, 4}, {3, 8}, {1, 2}, {5, 8}, {3, 4}, {7, 8}, {1, 1}}};

std::uint32_t ActiveCount(std::uint32_t universe, DensityPoint density)
{
    const auto product = static_cast<std::uint64_t>(universe) * density.numerator;
    Require(product % density.denominator == 0,
        "UniverseがDensityの正確なActive件数を表現できません。");
    return static_cast<std::uint32_t>(product / density.denominator);
}

std::string DistributionName(DistributionKind kind)
{
    switch (kind)
    {
    case DistributionKind::Prefix: return "Prefix";
    case DistributionKind::Suffix: return "Suffix";
    case DistributionKind::UniformStride: return "UniformStride";
    case DistributionKind::Clustered4: return "Clustered4";
    case DistributionKind::SeededRandom: return "SeededRandom";
    case DistributionKind::Full: return "Full";
    }
    Fail("未知のWorkset分布です。");
}

std::uint64_t NextRandom(std::uint64_t& state) noexcept
{
    state ^= state >> 12u;
    state ^= state << 25u;
    state ^= state >> 27u;
    return state * 2685821657736338717ull;
}

std::vector<std::uint32_t> BuildMembers(
    DistributionKind kind,
    std::uint32_t universe,
    std::uint32_t activeCount)
{
    Require(activeCount > 0 && activeCount <= universe,
        "WorksetのActive件数がUniverse範囲外です。");
    std::vector<std::uint32_t> members;
    members.reserve(activeCount);
    switch (kind)
    {
    case DistributionKind::Prefix:
    case DistributionKind::Full:
        for (std::uint32_t index = 0; index < activeCount; ++index)
            members.push_back(index);
        break;
    case DistributionKind::Suffix:
        for (std::uint32_t index = universe - activeCount; index < universe; ++index)
            members.push_back(index);
        break;
    case DistributionKind::UniformStride:
        for (std::uint32_t index = 0; index < activeCount; ++index)
            members.push_back(static_cast<std::uint32_t>(
                (static_cast<std::uint64_t>(index) * universe) / activeCount));
        break;
    case DistributionKind::Clustered4:
    {
        const auto clusterCount = std::min(4u, activeCount);
        const auto regionWidth = universe / clusterCount;
        std::uint32_t remaining = activeCount;
        for (std::uint32_t cluster = 0; cluster < clusterCount; ++cluster)
        {
            const auto clustersLeft = clusterCount - cluster;
            const auto count = (remaining + clustersLeft - 1u) / clustersLeft;
            remaining -= count;
            const auto regionBegin = cluster * regionWidth;
            const auto regionEnd = cluster + 1u == clusterCount
                ? universe : (cluster + 1u) * regionWidth;
            Require(count <= regionEnd - regionBegin,
                "Clustered worksetをUniverseへ配置できません。");
            const auto start = regionBegin + ((regionEnd - regionBegin) - count) / 2u;
            for (std::uint32_t offset = 0; offset < count; ++offset)
                members.push_back(start + offset);
        }
        std::ranges::sort(members);
        break;
    }
    case DistributionKind::SeededRandom:
    {
        members.resize(universe);
        for (std::uint32_t index = 0; index < universe; ++index)
            members[index] = index;
        std::uint64_t state = 0x4c35564552543231ull ^
            (static_cast<std::uint64_t>(universe) << 32u) ^ activeCount;
        for (std::uint32_t index = 0; index < activeCount; ++index)
        {
            const auto range = universe - index;
            const auto selected = index + static_cast<std::uint32_t>(NextRandom(state) % range);
            std::swap(members[index], members[selected]);
        }
        members.resize(activeCount);
        std::ranges::sort(members);
        break;
    }
    }
    Require(members.size() == activeCount &&
            std::ranges::is_sorted(members) &&
            std::adjacent_find(members.begin(), members.end()) == members.end() &&
            members.back() < universe,
        "WorksetがCanonical昇順の一意member集合になりませんでした。");
    return members;
}

Workset BuildWorkset(
    DistributionKind kind,
    std::uint32_t universe,
    std::uint32_t activeCount,
    DensityPoint density)
{
    Workset result;
    result.kind = kind;
    result.name = DistributionName(kind);
    result.members = BuildMembers(kind, universe, activeCount);
    result.densityNumerator = density.numerator;
    result.densityDenominator = density.denominator;
    std::vector<std::byte> bytes(
        result.members.size() * sizeof(std::uint32_t));
    for (std::size_t index = 0; index < result.members.size(); ++index)
    {
        const auto member = result.members[index];
        const auto offset = index * sizeof(std::uint32_t);
        bytes[offset + 0] = static_cast<std::byte>(member & 0xffu);
        bytes[offset + 1] = static_cast<std::byte>((member >> 8u) & 0xffu);
        bytes[offset + 2] = static_cast<std::byte>((member >> 16u) & 0xffu);
        bytes[offset + 3] = static_cast<std::byte>((member >> 24u) & 0xffu);
    }
    result.digestHex = sge4::base::ToHex(sge4::base::Sha256(bytes));
    result.span = result.members.back() - result.members.front() + 1u;
    result.contiguousRuns = 1;
    std::uint64_t totalGap = 0;
    for (std::size_t index = 1; index < result.members.size(); ++index)
    {
        const auto gap = result.members[index] - result.members[index - 1];
        totalGap += gap;
        result.maxGap = std::max(result.maxGap, gap);
        if (gap != 1u) ++result.contiguousRuns;
    }
    if (result.members.size() > 1)
        result.meanGap = static_cast<double>(totalGap) /
            static_cast<double>(result.members.size() - 1u);
    return result;
}

std::vector<Workset> BuildWorksets(std::uint32_t universe)
{
    constexpr std::array distributions = {
        DistributionKind::Prefix,
        DistributionKind::Suffix,
        DistributionKind::UniformStride,
        DistributionKind::Clustered4,
        DistributionKind::SeededRandom};
    std::vector<Workset> result;
    for (const auto density : DensityPoints)
    {
        const auto activeCount = ActiveCount(universe, density);
        if (density.numerator == density.denominator)
        {
            result.push_back(BuildWorkset(
                DistributionKind::Full, universe, activeCount, density));
            continue;
        }
        for (const auto distribution : distributions)
            result.push_back(BuildWorkset(
                distribution, universe, activeCount, density));
    }
    Require(result.size() == 31,
        "Density × Distribution交差面のcase数が31になりませんでした。");
    return result;
}

std::vector<Workset> BuildBoundaryRequalificationWorksets(
    std::uint32_t universe)
{
    constexpr std::array distributions = {
        DistributionKind::Prefix,
        DistributionKind::Suffix,
        DistributionKind::UniformStride,
        DistributionKind::Clustered4,
        DistributionKind::SeededRandom};
    std::vector<Workset> result;
    auto append = [&](DistributionKind distribution, DensityPoint density) {
        result.push_back(BuildWorkset(
            distribution, universe, ActiveCount(universe, density), density));
    };

    // 初回Surfaceで測定レジーム遷移が観測された地点を、境界とは別の
    // sentinelとして再資格する。既存93 case Evidenceは置換しない。
    if (universe == 1024u)
        append(DistributionKind::Clustered4, {3, 4});
    else if (universe == 4096u)
    {
        append(DistributionKind::Suffix, {1, 4});
        append(DistributionKind::SeededRandom, {1, 2});
    }
    else if (universe == 16384u)
        append(DistributionKind::UniformStride, {3, 8});
    else
    {
        Fail("Boundary RequalificationはUniverse 1024／4096／16384専用です。");
    }

    constexpr DensityPoint BoundaryDensity{7, 8};
    for (const auto distribution : distributions)
        append(distribution, BoundaryDensity);
    append(DistributionKind::Full, {1, 1});

    const auto expected = universe == 4096u ? 8u : 7u;
    Require(result.size() == expected,
        "Boundary Requalificationのcase数が想定と一致しませんでした。");
    return result;
}

sge4::d3d12::TimestampProfileSample FindStateWriterProfile(
    std::span<const sge4::d3d12::TimestampProfileSample> samples,
    std::string_view executionDigestHex)
{
    std::optional<sge4::d3d12::TimestampProfileSample> found;
    for (const auto& sample : samples)
    {
        if (sge4::base::ToHex(sample.packageExecutionDigest) != executionDigestHex)
            continue;
        if (found.has_value())
            Fail("State writer timestamp sampleが重複しました。");
        found = sample;
    }
    if (!found.has_value())
        Fail("State writer timestamp sampleが取得できませんでした。");
    Require(found->gpuNanoseconds > 0.0,
        "State writer GPU timestampがzeroです。");
    return *found;
}

FrameResult ExecuteFrame(
    sge4::d3d12::LoadedComposition& loaded,
    sge4::d3d12::Executor& executor,
    const experiment::CandidateBuild& candidate,
    std::uint64_t timelineOrdinal,
    std::span<const std::uint32_t> activeMembers,
    FrameEvidenceMode evidenceMode = FrameEvidenceMode::Full)
{
    auto planning = loaded.PlanningContext();
    auto input = experiment::MakeArbitraryInvocationInput(
        timelineOrdinal, planning.requiredMode, activeMembers,
        planning.requiredMode == sge4::dynamic::InvocationModeV1::ContinueHistory);
    auto invocation = experiment::BuildInvocation(
        loaded.Package(), planning.deviceEpoch, std::move(input),
        std::move(planning.previousHistory));
    if (!invocation)
        Fail("Level 5実験2b Invocation生成に失敗しました：" + invocation.error());

    if (candidate.kind == experiment::CandidateKind::VerifiedSparseWorklist)
    {
        Require(invocation.value().CompactWorklist().memberIndices ==
                std::vector<std::uint32_t>(activeMembers.begin(), activeMembers.end()),
            "Frozen Compact Worklistが指定した任意疎集合と一致しません。");
    }
    else
    {
        Require(invocation.value().CompactWorklist().memberIndices.empty(),
            "Dense Direct candidateへCompact Worklistが混入しました。");
    }

    sge4::d3d12::FrameInput frame;
    frame.frameNumber = timelineOrdinal;
    if (candidate.kind == experiment::CandidateKind::DenseDirect)
    {
        frame.leafDynamicData.push_back({
            candidate.stateLeaf, 1,
            experiment::IdentityIndexListBytes(candidate.width * candidate.height)});
    }
    auto submission = sge4::d3d12::Submit(
        loaded, std::move(invocation).value(), std::move(frame));
    if (!submission)
        Fail("Level 5実験2b Submitに失敗しました：" +
            submission.error().stage + "：" + submission.error().message);
    Require(submission.value().verifiedDynamicRouteCount == 2,
        "二つのverified Dynamic routeが実行されませんでした。");
    if (candidate.kind == experiment::CandidateKind::VerifiedSparseWorklist)
    {
        Require(submission.value().verifiedIndirectDispatchCount == 1 &&
            submission.value().verifiedIndirectWorkCount ==
                static_cast<std::uint32_t>(activeMembers.size()) &&
            submission.value().verifiedCompactWorklistBindingCount == 1 &&
            submission.value().verifiedCompactWorklistIndexCount ==
                static_cast<std::uint32_t>(activeMembers.size()),
            "任意疎集合がCompact WorklistとDispatchIndirectへ接続されませんでした。");
    }
    else
    {
        Require(submission.value().verifiedIndirectDispatchCount == 0 &&
            submission.value().verifiedCompactWorklistBindingCount == 0,
            "Dense Direct candidateへverified sparse実行が混入しました。");
    }

    auto readback = sge4::d3d12::ReadBuffer(loaded, candidate.observationResource);
    if (!readback)
        Fail("Observation readbackに失敗しました：" +
            readback.error().stage + "：" + readback.error().message);
    const auto observation = DecodeFloat4<Observation>(
        readback.value().bytes, "Observation");

    TemporalAggregate acceptedTemporal{};
    TextureEvidence texture{};
    if (evidenceMode == FrameEvidenceMode::Full)
    {
        auto temporalReadback = sge4::d3d12::ReadBuffer(
            loaded, candidate.temporalResource);
        if (!temporalReadback)
            Fail("Temporal Aggregate readbackに失敗しました：" +
                temporalReadback.error().stage + "：" + temporalReadback.error().message);
        acceptedTemporal = DecodeFloat4<TemporalAggregate>(
            temporalReadback.value().bytes, "Temporal Aggregate");
        Require(NearlyEqual(acceptedTemporal.stateSum, observation.stateSum) &&
                NearlyEqual(acceptedTemporal.combined, observation.stateSum) &&
                NearlyEqual(acceptedTemporal.universe, observation.universe),
            "受理済みTemporal Aggregateがcurrent State Observationと一致しませんでした。");

        auto textureReadback = sge4::d3d12::ReadTexture2D(
            loaded, candidate.textureResource);
        if (!textureReadback)
            Fail("RGBA32F Texture readbackに失敗しました：" +
                textureReadback.error().stage + "：" + textureReadback.error().message);
        texture = DecodeTextureEvidence(textureReadback.value(), candidate);
    }

    const auto profiles = executor.ConsumeTimestampProfileSamples();
    if (evidenceMode == FrameEvidenceMode::Full)
        Require(profiles.size() == 4,
            "全4 Leaf completion後のtimestamp sampleが揃いませんでした。");
    return {observation, acceptedTemporal, texture,
        FindStateWriterProfile(profiles, candidate.stateWriterExecutionDigestHex)};
}

void VerifyCandidateStructure(
    const experiment::CandidateBuild& dense,
    const experiment::CandidateBuild& sparse)
{
    const auto& denseContract =
        dense.package.VerifiedComposition().ValidatedContract().Contract();
    const auto& sparseContract =
        sparse.package.VerifiedComposition().ValidatedContract().Contract();
    Require(denseContract.identity == sparseContract.identity,
        "Dense／Sparse candidateのComposition Contract identityが一致しません。");
    Require(denseContract.leaves.size() == sparseContract.leaves.size() &&
        denseContract.resources.size() == sparseContract.resources.size(),
        "Dense／Sparse candidateのLeaf／Resource構造が一致しません。");
    for (std::size_t index = 0; index < denseContract.leaves.size(); ++index)
    {
        Require(denseContract.leaves[index].executionDigest ==
                sparseContract.leaves[index].executionDigest &&
            denseContract.leaves[index].fileDigest ==
                sparseContract.leaves[index].fileDigest,
            "Dense／Sparse candidateが同じFrozen Leaf集合を共有しません。");
    }
    const auto& denseIndirect = dense.package.DynamicContract().indirectDispatch;
    const auto& sparseIndirect = sparse.package.DynamicContract().indirectDispatch;
    Require(denseIndirect.mode == sge4::composition::IndirectExecutionModeV1::None &&
        denseIndirect.compactWorklistMode ==
            sge4::composition::CompactWorklistModeV1::None &&
        sparseIndirect.mode ==
            sge4::composition::IndirectExecutionModeV1::VerifiedDispatch &&
        sparseIndirect.compactWorklistMode ==
            sge4::composition::CompactWorklistModeV1::VerifiedU32 &&
        sparseIndirect.targetIndexListDynamicSlot == 1,
        "候補差分がVerified Compact Worklist routeへ限定されていません。");
    Require(dense.package.SemanticDigest() != sparse.package.SemanticDigest(),
        "Dense／Sparse candidateのFrozen semantic identityが分離されませんでした。");
}

void VerifyControlledRecovery(
    const Arguments& arguments,
    const experiment::CandidateBuild& denseCandidate,
    const experiment::CandidateBuild& sparseCandidate,
    const Workset& workset)
{
    sge4::d3d12::ExecutorOptions options;
    options.forceWarp = arguments.forceWarp;
    options.enableDebugLayer = arguments.enableDebugLayer;
    options.enableTimestampProfiling = true;
    sge4::d3d12::Executor executor(options);
    const auto temporalSeed = experiment::ZeroTemporalSeed();

    sge4::d3d12::LoadInput denseLoad;
    denseLoad.initialResources = {{denseCandidate.temporalResource, temporalSeed}};
    auto dense = sge4::d3d12::LoadComposition(
        denseCandidate.package.FileBytes(), executor, std::move(denseLoad));
    if (!dense)
        Fail("Recovery Dense candidateのLoadに失敗しました：" +
            dense.error().stage + "：" + dense.error().message);

    sge4::d3d12::LoadInput sparseLoad;
    sparseLoad.initialResources = {{sparseCandidate.temporalResource, temporalSeed}};
    auto sparse = sge4::d3d12::LoadComposition(
        sparseCandidate.package.FileBytes(), executor, std::move(sparseLoad));
    if (!sparse)
        Fail("Recovery Sparse candidateのLoadに失敗しました：" +
            sparse.error().stage + "：" + sparse.error().message);

    const auto denseBefore = ExecuteFrame(
        dense.value(), executor, denseCandidate, 0, workset.members);
    const auto sparseBefore = ExecuteFrame(
        sparse.value(), executor, sparseCandidate, 0, workset.members);
    Require(Equivalent(denseBefore.observation, sparseBefore.observation) &&
            NearlyEqual(denseBefore.acceptedTemporal.combined,
                sparseBefore.acceptedTemporal.combined) &&
            denseBefore.texture.digest == sparseBefore.texture.digest,
        "Recovery前の任意疎集合Observation／Temporal／Textureが一致しません。");

    auto denseRecovery = sge4::d3d12::Recover(
        dense.value(), sge4::runtime::DeviceRecoveryMode::ControlledRebuild);
    auto sparseRecovery = sge4::d3d12::Recover(
        sparse.value(), sge4::runtime::DeviceRecoveryMode::ControlledRebuild);
    Require(denseRecovery && sparseRecovery &&
        denseRecovery.value().newEpoch > denseRecovery.value().previousEpoch &&
        sparseRecovery.value().newEpoch > sparseRecovery.value().previousEpoch,
        "Level 5実験2b candidateのControlled Recoveryに失敗しました。");
    Require(static_cast<bool>(sge4::d3d12::AcknowledgeExternalRebind(dense.value())) &&
        static_cast<bool>(sge4::d3d12::AcknowledgeExternalRebind(sparse.value())),
        "Level 5実験2b candidateのExternal rebind確認に失敗しました。");

    const auto denseAfter = ExecuteFrame(
        dense.value(), executor, denseCandidate, 1, workset.members);
    const auto sparseAfter = ExecuteFrame(
        sparse.value(), executor, sparseCandidate, 1, workset.members);
    Require(Equivalent(denseAfter.observation, sparseAfter.observation) &&
            NearlyEqual(denseAfter.acceptedTemporal.combined,
                sparseAfter.acceptedTemporal.combined) &&
            denseAfter.texture.digest == sparseAfter.texture.digest,
        "RecoverySeed後の任意疎集合Observation／Temporal／Textureが一致しません。");
    Require(NearlyEqual(denseAfter.observation.previousStateSum, 0.0f),
        "Recovery後にTemporal historyが明示seedへ戻りませんでした。");
}

struct PairExecution final
{
    FrameResult dense;
    FrameResult sparse;
    bool denseFirst = false;
};

PairExecution ExecutePair(
    sge4::d3d12::LoadedComposition& dense,
    sge4::d3d12::LoadedComposition& sparse,
    sge4::d3d12::Executor& executor,
    const experiment::CandidateBuild& denseCandidate,
    const experiment::CandidateBuild& sparseCandidate,
    std::uint64_t timelineOrdinal,
    std::span<const std::uint32_t> activeMembers,
    FrameEvidenceMode evidenceMode,
    std::optional<float>& previousStateSum)
{
    PairExecution result;
    result.denseFirst = timelineOrdinal % 2u == 0u;
    if (result.denseFirst)
    {
        result.dense = ExecuteFrame(
            dense, executor, denseCandidate, timelineOrdinal, activeMembers, evidenceMode);
        result.sparse = ExecuteFrame(
            sparse, executor, sparseCandidate, timelineOrdinal, activeMembers, evidenceMode);
    }
    else
    {
        result.sparse = ExecuteFrame(
            sparse, executor, sparseCandidate, timelineOrdinal, activeMembers, evidenceMode);
        result.dense = ExecuteFrame(
            dense, executor, denseCandidate, timelineOrdinal, activeMembers, evidenceMode);
    }

    Require(Equivalent(result.dense.observation, result.sparse.observation),
        "任意疎集合に対する候補のState Observationが一致しませんでした。");
    if (evidenceMode == FrameEvidenceMode::Full)
    {
        Require(NearlyEqual(result.dense.acceptedTemporal.combined,
                    result.sparse.acceptedTemporal.combined) &&
                result.dense.texture.digest == result.sparse.texture.digest &&
                std::abs(result.dense.texture.xSum - result.sparse.texture.xSum) <= 1.0e-6,
            "任意疎集合に対する候補のTemporal／Texture観測が一致しませんでした。");
    }
    if (!previousStateSum.has_value())
    {
        Require(NearlyEqual(result.dense.observation.previousStateSum, 0.0f),
            "最初のframeが明示zero Temporal seedを観測しませんでした。");
    }
    else
    {
        Require(NearlyEqual(
                result.dense.observation.previousStateSum, *previousStateSum),
            "successful whole-submit後のTemporal rotationが観測されませんでした。");
    }
    Require(NearlyEqual(
            result.dense.observation.delta,
            result.dense.observation.stateSum -
                result.dense.observation.previousStateSum),
        "Observation deltaがcurrent／Previous Temporal値と一致しませんでした。");
    previousStateSum = result.dense.observation.stateSum;
    return result;
}

WarmupQualification RunAdaptiveWarmup(
    sge4::d3d12::LoadedComposition& dense,
    sge4::d3d12::LoadedComposition& sparse,
    sge4::d3d12::Executor& executor,
    const experiment::CandidateBuild& denseCandidate,
    const experiment::CandidateBuild& sparseCandidate,
    std::span<const std::uint32_t> activeMembers,
    std::uint32_t minimumFrames,
    std::uint32_t maximumFrames,
    std::uint32_t windowFrames,
    double tolerance,
    bool requirePerformanceConvergence,
    std::uint64_t timelineBase,
    std::optional<float>& previousStateSum,
    std::string_view label)
{
    const auto begin = std::chrono::steady_clock::now();
    std::vector<double> denseGpu;
    std::vector<double> sparseGpu;
    denseGpu.reserve(maximumFrames);
    sparseGpu.reserve(maximumFrames);
    double denseRatio = std::numeric_limits<double>::infinity();
    double sparseRatio = std::numeric_limits<double>::infinity();

    for (std::uint32_t frame = 0; frame < maximumFrames; ++frame)
    {
        const auto pair = ExecutePair(
            dense, sparse, executor, denseCandidate, sparseCandidate,
            timelineBase + frame, activeMembers, FrameEvidenceMode::WarmupMinimal,
            previousStateSum);
        denseGpu.push_back(pair.dense.stateWriterProfile.gpuNanoseconds);
        sparseGpu.push_back(pair.sparse.stateWriterProfile.gpuNanoseconds);

        const auto framesUsed = frame + 1u;
        if (framesUsed < minimumFrames || framesUsed < windowFrames)
            continue;
        const auto denseStats = AnalyzeRegime(std::span<const double>(
            denseGpu.data() + denseGpu.size() - windowFrames, windowFrames));
        const auto sparseStats = AnalyzeRegime(std::span<const double>(
            sparseGpu.data() + sparseGpu.size() - windowFrames, windowFrames));
        denseRatio = denseStats.symmetricRatio;
        sparseRatio = sparseStats.symmetricRatio;
        if (!requirePerformanceConvergence)
        {
            return {framesUsed, denseRatio, sparseRatio,
                ElapsedSeconds(begin, std::chrono::steady_clock::now())};
        }
        if (denseRatio <= 1.0 + tolerance &&
            sparseRatio <= 1.0 + tolerance)
        {
            return {framesUsed, denseRatio, sparseRatio,
                ElapsedSeconds(begin, std::chrono::steady_clock::now())};
        }
    }
    Fail(std::string(label) + "が最大" + std::to_string(maximumFrames) +
        " frame以内に均質な測定レジームへ収束しませんでした。" +
        " denseRatio=" + std::to_string(denseRatio) +
        " sparseRatio=" + std::to_string(sparseRatio));
}

WarmupQualification RunGlobalWarmup(
    const Arguments& arguments,
    sge4::d3d12::Executor& executor,
    const experiment::CandidateBuild& denseCandidate,
    const experiment::CandidateBuild& sparseCandidate)
{
    const auto begin = std::chrono::steady_clock::now();
    const auto universe = arguments.width * arguments.height;
    const auto full = BuildWorkset(
        DistributionKind::Full, universe, universe, DensityPoint{1, 1});
    const auto temporalSeed = experiment::ZeroTemporalSeed();
    sge4::d3d12::LoadInput denseLoad;
    denseLoad.initialResources = {{denseCandidate.temporalResource, temporalSeed}};
    auto dense = sge4::d3d12::LoadComposition(
        denseCandidate.package.FileBytes(), executor, std::move(denseLoad));
    if (!dense) Fail("Global warmup Dense Loadに失敗しました：" + dense.error().message);
    sge4::d3d12::LoadInput sparseLoad;
    sparseLoad.initialResources = {{sparseCandidate.temporalResource, temporalSeed}};
    auto sparse = sge4::d3d12::LoadComposition(
        sparseCandidate.package.FileBytes(), executor, std::move(sparseLoad));
    if (!sparse) Fail("Global warmup Sparse Loadに失敗しました：" + sparse.error().message);
    std::optional<float> previousStateSum;
    auto qualification = RunAdaptiveWarmup(
        dense.value(), sparse.value(), executor,
        denseCandidate, sparseCandidate, full.members,
        arguments.globalWarmupMinFrames,
        arguments.globalWarmupMaxFrames,
        arguments.warmupWindowFrames,
        arguments.warmupTolerance,
        !arguments.forceWarp,
        0,
        previousStateSum,
        "Universe専用global warmup");
    qualification.elapsedSeconds = ElapsedSeconds(
        begin, std::chrono::steady_clock::now());
    return qualification;
}

CaseResult RunCase(
    const Arguments& arguments,
    sge4::d3d12::Executor& executor,
    const experiment::CandidateBuild& denseCandidate,
    const experiment::CandidateBuild& sparseCandidate,
    Workset workset)
{
    const auto loadBegin = std::chrono::steady_clock::now();
    const auto temporalSeed = experiment::ZeroTemporalSeed();
    sge4::d3d12::LoadInput denseLoad;
    denseLoad.initialResources = {{denseCandidate.temporalResource, temporalSeed}};
    auto dense = sge4::d3d12::LoadComposition(
        denseCandidate.package.FileBytes(), executor, std::move(denseLoad));
    if (!dense)
        Fail("Dense candidateのLoadに失敗しました：" +
            dense.error().stage + "：" + dense.error().message);

    sge4::d3d12::LoadInput sparseLoad;
    sparseLoad.initialResources = {{sparseCandidate.temporalResource, temporalSeed}};
    auto sparse = sge4::d3d12::LoadComposition(
        sparseCandidate.package.FileBytes(), executor, std::move(sparseLoad));
    if (!sparse)
        Fail("Sparse candidateのLoadに失敗しました：" +
            sparse.error().stage + "：" + sparse.error().message);
    const auto loadSeconds = ElapsedSeconds(
        loadBegin, std::chrono::steady_clock::now());

    std::optional<float> previousStateSum;
    const auto warmup = RunAdaptiveWarmup(
        dense.value(), sparse.value(), executor,
        denseCandidate, sparseCandidate, workset.members,
        arguments.warmupMinFrames,
        arguments.warmupMaxFrames,
        arguments.warmupWindowFrames,
        arguments.warmupTolerance,
        !arguments.forceWarp,
        0,
        previousStateSum,
        "case-local warmup");

    std::vector<double> denseGpu;
    std::vector<double> sparseGpu;
    std::vector<double> denseRecording;
    std::vector<double> sparseRecording;
    std::vector<bool> denseFirst;
    denseGpu.reserve(arguments.sampleFrames);
    sparseGpu.reserve(arguments.sampleFrames);
    denseRecording.reserve(arguments.sampleFrames);
    sparseRecording.reserve(arguments.sampleFrames);
    denseFirst.reserve(arguments.sampleFrames);

    const auto measurementBegin = std::chrono::steady_clock::now();
    for (std::uint32_t sample = 0; sample < arguments.sampleFrames; ++sample)
    {
        const auto timelineOrdinal =
            static_cast<std::uint64_t>(warmup.framesUsed) + sample;
        const auto pair = ExecutePair(
            dense.value(), sparse.value(), executor,
            denseCandidate, sparseCandidate,
            timelineOrdinal, workset.members, FrameEvidenceMode::Full,
            previousStateSum);
        denseGpu.push_back(pair.dense.stateWriterProfile.gpuNanoseconds);
        sparseGpu.push_back(pair.sparse.stateWriterProfile.gpuNanoseconds);
        denseRecording.push_back(
            pair.dense.stateWriterProfile.commandRecordingNanoseconds);
        sparseRecording.push_back(
            pair.sparse.stateWriterProfile.commandRecordingNanoseconds);
        denseFirst.push_back(pair.denseFirst);
    }
    const auto measurementSeconds = ElapsedSeconds(
        measurementBegin, std::chrono::steady_clock::now());

    CaseResult result;
    result.width = arguments.width;
    result.height = arguments.height;
    result.universe = arguments.width * arguments.height;
    result.denseComposition = sge4::base::ToHex(denseCandidate.package.SemanticDigest());
    result.sparseComposition = sge4::base::ToHex(sparseCandidate.package.SemanticDigest());
    result.workset = std::move(workset);
    result.warmupFramesUsed = warmup.framesUsed;
    result.warmupDenseRegimeRatio = warmup.denseRegimeRatio;
    result.warmupSparseRegimeRatio = warmup.sparseRegimeRatio;
    result.denseMeasurementRegime = AnalyzeRegime(denseGpu);
    result.sparseMeasurementRegime = AnalyzeRegime(sparseGpu);
    result.measurementStatus = arguments.forceWarp
        ? MeasurementRegimeStatus::SemanticQualificationOnly
        : (HasMeasurementRegimeTransition(
                result.denseMeasurementRegime,
                result.sparseMeasurementRegime,
                arguments.measurementRegimeThreshold)
            ? MeasurementRegimeStatus::MeasurementRegimeTransition
            : MeasurementRegimeStatus::AcceptedStable);
    result.denseGpuMedianNs = Median(denseGpu);
    result.sparseGpuMedianNs = Median(sparseGpu);
    result.denseRecordingMedianNs = Median(denseRecording);
    result.sparseRecordingMedianNs = Median(sparseRecording);
    result.denseOverSparse = result.sparseGpuMedianNs > 0.0
        ? result.denseGpuMedianNs / result.sparseGpuMedianNs : 0.0;
    result.loadSeconds = loadSeconds;
    result.warmupSeconds = warmup.elapsedSeconds;
    result.measurementSeconds = measurementSeconds;
    result.denseGpuSamples = std::move(denseGpu);
    result.sparseGpuSamples = std::move(sparseGpu);
    result.denseRecordingSamples = std::move(denseRecording);
    result.sparseRecordingSamples = std::move(sparseRecording);
    result.denseFirst = std::move(denseFirst);
    return result;
}

CaseResult RunBoundaryRequalificationCase(
    const Arguments& arguments,
    sge4::d3d12::Executor& executor,
    const experiment::CandidateBuild& denseCandidate,
    const experiment::CandidateBuild& sparseCandidate,
    Workset workset)
{
    const auto loadBegin = std::chrono::steady_clock::now();
    const auto temporalSeed = experiment::ZeroTemporalSeed();
    sge4::d3d12::LoadInput denseLoad;
    denseLoad.initialResources = {{denseCandidate.temporalResource, temporalSeed}};
    auto dense = sge4::d3d12::LoadComposition(
        denseCandidate.package.FileBytes(), executor, std::move(denseLoad));
    if (!dense)
        Fail("Boundary Dense candidateのLoadに失敗しました：" +
            dense.error().stage + "：" + dense.error().message);

    sge4::d3d12::LoadInput sparseLoad;
    sparseLoad.initialResources = {{sparseCandidate.temporalResource, temporalSeed}};
    auto sparse = sge4::d3d12::LoadComposition(
        sparseCandidate.package.FileBytes(), executor, std::move(sparseLoad));
    if (!sparse)
        Fail("Boundary Sparse candidateのLoadに失敗しました：" +
            sparse.error().stage + "：" + sparse.error().message);
    const auto loadSeconds = ElapsedSeconds(
        loadBegin, std::chrono::steady_clock::now());

    std::optional<float> previousStateSum;
    const auto initialQualificationBegin = std::chrono::steady_clock::now();
    (void)ExecutePair(
        dense.value(), sparse.value(), executor,
        denseCandidate, sparseCandidate, 0,
        workset.members, FrameEvidenceMode::Full, previousStateSum);
    auto fullQualificationSeconds = ElapsedSeconds(
        initialQualificationBegin, std::chrono::steady_clock::now());

    const auto warmup = RunAdaptiveWarmup(
        dense.value(), sparse.value(), executor,
        denseCandidate, sparseCandidate, workset.members,
        arguments.warmupMinFrames,
        arguments.warmupMaxFrames,
        arguments.warmupWindowFrames,
        arguments.warmupTolerance,
        true,
        1,
        previousStateSum,
        "boundary case-local warmup");

    std::vector<double> denseGpu;
    std::vector<double> sparseGpu;
    std::vector<double> denseRecording;
    std::vector<double> sparseRecording;
    std::vector<bool> denseFirst;
    denseGpu.reserve(arguments.sampleFrames);
    sparseGpu.reserve(arguments.sampleFrames);
    denseRecording.reserve(arguments.sampleFrames);
    sparseRecording.reserve(arguments.sampleFrames);
    denseFirst.reserve(arguments.sampleFrames);

    const auto measurementBegin = std::chrono::steady_clock::now();
    const auto measurementBase = 1ull + warmup.framesUsed;
    for (std::uint32_t sample = 0; sample < arguments.sampleFrames; ++sample)
    {
        const auto pair = ExecutePair(
            dense.value(), sparse.value(), executor,
            denseCandidate, sparseCandidate,
            measurementBase + sample,
            workset.members,
            FrameEvidenceMode::WarmupMinimal,
            previousStateSum);
        denseGpu.push_back(pair.dense.stateWriterProfile.gpuNanoseconds);
        sparseGpu.push_back(pair.sparse.stateWriterProfile.gpuNanoseconds);
        denseRecording.push_back(
            pair.dense.stateWriterProfile.commandRecordingNanoseconds);
        sparseRecording.push_back(
            pair.sparse.stateWriterProfile.commandRecordingNanoseconds);
        denseFirst.push_back(pair.denseFirst);
    }
    const auto measurementSeconds = ElapsedSeconds(
        measurementBegin, std::chrono::steady_clock::now());

    const auto finalQualificationBegin = std::chrono::steady_clock::now();
    (void)ExecutePair(
        dense.value(), sparse.value(), executor,
        denseCandidate, sparseCandidate,
        measurementBase + arguments.sampleFrames,
        workset.members, FrameEvidenceMode::Full, previousStateSum);
    fullQualificationSeconds += ElapsedSeconds(
        finalQualificationBegin, std::chrono::steady_clock::now());

    CaseResult result;
    result.width = arguments.width;
    result.height = arguments.height;
    result.universe = arguments.width * arguments.height;
    result.denseComposition = sge4::base::ToHex(denseCandidate.package.SemanticDigest());
    result.sparseComposition = sge4::base::ToHex(sparseCandidate.package.SemanticDigest());
    result.workset = std::move(workset);
    result.warmupFramesUsed = warmup.framesUsed;
    result.warmupDenseRegimeRatio = warmup.denseRegimeRatio;
    result.warmupSparseRegimeRatio = warmup.sparseRegimeRatio;
    result.denseMeasurementRegime = AnalyzeRegime(denseGpu);
    result.sparseMeasurementRegime = AnalyzeRegime(sparseGpu);
    result.measurementStatus = HasMeasurementRegimeTransition(
            result.denseMeasurementRegime,
            result.sparseMeasurementRegime,
            arguments.measurementRegimeThreshold)
        ? MeasurementRegimeStatus::MeasurementRegimeTransition
        : MeasurementRegimeStatus::AcceptedStable;
    result.denseGpuMedianNs = Median(denseGpu);
    result.sparseGpuMedianNs = Median(sparseGpu);
    result.denseRecordingMedianNs = Median(denseRecording);
    result.sparseRecordingMedianNs = Median(sparseRecording);
    result.denseOverSparse = result.sparseGpuMedianNs > 0.0
        ? result.denseGpuMedianNs / result.sparseGpuMedianNs : 0.0;
    result.loadSeconds = loadSeconds;
    result.fullQualificationSeconds = fullQualificationSeconds;
    result.warmupSeconds = warmup.elapsedSeconds;
    result.measurementSeconds = measurementSeconds;
    result.denseGpuSamples = std::move(denseGpu);
    result.sparseGpuSamples = std::move(sparseGpu);
    result.denseRecordingSamples = std::move(denseRecording);
    result.sparseRecordingSamples = std::move(sparseRecording);
    result.denseFirst = std::move(denseFirst);
    return result;
}

const CaseResult* FindFullCase(std::span<const CaseResult> results)
{
    for (const auto& result : results)
        if (result.workset.kind == DistributionKind::Full) return &result;
    return nullptr;
}

const CaseResult* FindSeriesPoint(
    std::span<const CaseResult> results,
    DistributionKind distribution,
    DensityPoint density)
{
    if (density.numerator == density.denominator)
        return FindFullCase(results);
    for (const auto& result : results)
    {
        if (result.workset.kind == distribution &&
            result.workset.densityNumerator == density.numerator &&
            result.workset.densityDenominator == density.denominator)
            return &result;
    }
    return nullptr;
}

struct SeriesSummary final
{
    double lower = -1.0;
    double upper = -1.0;
    std::size_t stablePoints = 0;
    std::size_t unstablePoints = 0;
    bool complete = true;
    bool monotonicityViolation = false;
    std::string status;
};

SeriesSummary SummarizeSeries(
    std::span<const CaseResult> results,
    DistributionKind distribution,
    bool performanceEligible = true)
{
    SeriesSummary summary;
    if (!performanceEligible)
    {
        summary.complete = false;
        summary.status = "SemanticQualificationOnly";
        return summary;
    }
    std::vector<const CaseResult*> points;
    points.reserve(DensityPoints.size());
    for (const auto density : DensityPoints)
    {
        const auto* point = FindSeriesPoint(results, distribution, density);
        Require(point != nullptr, "交差系列のDensity点が欠落しています。");
        points.push_back(point);
        if (point->measurementStatus == MeasurementRegimeStatus::AcceptedStable)
            ++summary.stablePoints;
        else
            ++summary.unstablePoints;
    }
    if (summary.unstablePoints != 0)
    {
        summary.complete = false;
        summary.status = "MeasurementRegimeIncomplete";
        return summary;
    }

    double previousRatio = -1.0;
    for (std::size_t index = 0; index < DensityPoints.size(); ++index)
    {
        const auto density = DensityPoints[index];
        const auto* point = points[index];
        const auto activeRatio =
            static_cast<double>(density.numerator) / density.denominator;
        if (previousRatio > 0.0 &&
            point->denseOverSparse > previousRatio * 1.05)
            summary.monotonicityViolation = true;
        previousRatio = point->denseOverSparse;
        if (point->denseOverSparse > 1.05)
        {
            summary.lower = activeRatio;
            continue;
        }
        if (summary.lower >= 0.0)
        {
            summary.upper = activeRatio;
            break;
        }
    }
    summary.status = summary.lower >= 0.0 && summary.upper > summary.lower
        ? "Bracketed"
        : (summary.lower >= 0.0
            ? "SparseThroughMeasuredRange" : "NoSparseAdvantage");
    return summary;
}

std::string Classify(
    const Arguments& arguments,
    std::span<const CaseResult> results)
{
    if (arguments.forceWarp) return "SemanticQualificationOnly";
    if (arguments.boundaryRequalification)
    {
        constexpr std::array distributions = {
            DistributionKind::Prefix,
            DistributionKind::Suffix,
            DistributionKind::UniformStride,
            DistributionKind::Clustered4,
            DistributionKind::SeededRandom};
        const auto* full = FindFullCase(results);
        if (!full || full->measurementStatus !=
                MeasurementRegimeStatus::AcceptedStable)
            return "BoundaryRequalificationIncomplete";
        bool changed = full->denseOverSparse < 0.95 ||
            full->denseOverSparse > 1.05;
        for (const auto distribution : distributions)
        {
            const auto* boundary = FindSeriesPoint(
                results, distribution, DensityPoint{7, 8});
            if (!boundary || boundary->measurementStatus !=
                    MeasurementRegimeStatus::AcceptedStable)
                return "BoundaryRequalificationIncomplete";
            changed = changed || boundary->denseOverSparse <= 1.05;
        }
        for (const auto& result : results)
        {
            if (result.measurementStatus !=
                    MeasurementRegimeStatus::AcceptedStable)
                return "BoundaryRequalificationIncomplete";
            const bool boundaryDensity =
                result.workset.densityNumerator == 7u &&
                result.workset.densityDenominator == 8u;
            if (result.workset.kind != DistributionKind::Full &&
                !boundaryDensity &&
                result.denseOverSparse <= 1.05)
                changed = true;
        }
        return changed ? "BoundaryResultChanged"
                       : "BoundaryCrossoverRequalified";
    }
    constexpr std::array distributions = {
        DistributionKind::Prefix,
        DistributionKind::Suffix,
        DistributionKind::UniformStride,
        DistributionKind::Clustered4,
        DistributionKind::SeededRandom};
    std::size_t bracketed = 0;
    bool incomplete = false;
    for (const auto distribution : distributions)
    {
        const auto summary = SummarizeSeries(results, distribution);
        incomplete = incomplete || !summary.complete;
        if (summary.complete && summary.lower >= 0.0 &&
            summary.upper > summary.lower)
            ++bracketed;
    }
    if (incomplete) return "CrossoverSurfaceIncomplete";
    if (bracketed == distributions.size()) return "CrossoverSurfaceBracketed";
    if (bracketed > 0) return "PartialCrossoverSurface";
    const bool sparseWinsAny = std::ranges::any_of(results,
        [](const CaseResult& result) {
            return result.measurementStatus ==
                    MeasurementRegimeStatus::AcceptedStable &&
                result.denseOverSparse > 1.05;
        });
    if (sparseWinsAny) return "SparseDominatesMeasuredSurface";
    return "NoMaterialSeparation";
}

std::filesystem::path SurfacePath(const std::filesystem::path& rawPath)
{
    auto result = rawPath;
    const auto stem = rawPath.stem().string() + "_surface";
    result.replace_filename(stem + rawPath.extension().string());
    return result;
}

std::filesystem::path TimingPath(const std::filesystem::path& rawPath)
{
    auto result = rawPath;
    const auto stem = rawPath.stem().string() + "_timing";
    result.replace_filename(stem + rawPath.extension().string());
    return result;
}

std::string_view EvidenceRole(const Arguments& arguments)
{
    if (arguments.forceWarp) return "SemanticQualificationOnly";
    if (arguments.boundaryRequalification) return "BoundaryRequalification";
    return "PerformanceSurface";
}

void WriteEvidence(
    const Arguments& arguments,
    std::span<const CaseResult> results,
    const WarmupQualification& globalWarmup,
    std::string_view classification)
{
    if (arguments.output.has_parent_path())
        std::filesystem::create_directories(arguments.output.parent_path());
    std::ofstream stream(arguments.output, std::ios::binary | std::ios::trunc);
    if (!stream) Fail("Evidence fileを作成できません。");
    stream << "# SGE4 Level 5 Vertical Experiment 2b\n";
    stream << "# subject="
           << (arguments.boundaryRequalification
                ? "Density x Universe x Distribution Boundary Requalification"
                : "Density x Universe x Distribution Crossover Surface")
           << "\n";
    stream << "# device=" << (arguments.forceWarp ? "WARP" : "Hardware") << "\n";
    stream << "# extent=" << arguments.width << 'x' << arguments.height << "\n";
    stream << "# universe=" << arguments.width * arguments.height << "\n";
    stream << "# global_warmup_min_frames=" << arguments.globalWarmupMinFrames << "\n";
    stream << "# global_warmup_max_frames=" << arguments.globalWarmupMaxFrames << "\n";
    stream << "# global_warmup_frames_used=" << globalWarmup.framesUsed << "\n";
    stream << "# global_warmup_dense_regime_ratio=" << globalWarmup.denseRegimeRatio << "\n";
    stream << "# global_warmup_sparse_regime_ratio=" << globalWarmup.sparseRegimeRatio << "\n";
    stream << "# warmup_min_frames_per_case=" << arguments.warmupMinFrames << "\n";
    stream << "# warmup_max_frames_per_case=" << arguments.warmupMaxFrames << "\n";
    stream << "# warmup_window_frames=" << arguments.warmupWindowFrames << "\n";
    stream << "# warmup_tolerance=" << arguments.warmupTolerance << "\n";
    stream << "# sample_frames_per_case=" << arguments.sampleFrames << "\n";
    stream << "# measurement_regime_threshold="
           << arguments.measurementRegimeThreshold << "\n";
    stream << "# density_points="
           << (arguments.boundaryRequalification
                ? "sentinel-dependent;0.875;1.000"
                : "0.250;0.375;0.500;0.625;0.750;0.875;1.000")
           << "\n";
    stream << "# random_seed_scheme=L5VERT21-xorshift64star-universe-active\n";
    stream << "# classification=" << classification << "\n";
    stream << "# owner_decision=DeferredByOwner\n";
    stream << "# evidence_role=" << EvidenceRole(arguments) << "\n";
    stream << "# measurement_readback_mode="
           << (arguments.boundaryRequalification
                ? "ObservationOnlyWithFullStartEndQualification"
                : "FullEverySample")
           << "\n";
    stream << "width,height,universe,density_numerator,density_denominator,"
              "distribution,workset_digest,active_count,active_ratio,span,"
              "contiguous_runs,mean_gap,max_gap,sample_index,execution_order,"
              "dense_gpu_ns,sparse_gpu_ns,dense_recording_ns,sparse_recording_ns,"
              "dense_gpu_median_ns,sparse_gpu_median_ns,dense_over_sparse,"
              "dense_recording_median_ns,sparse_recording_median_ns,"
              "measurement_status,warmup_frames_used,warmup_dense_regime_ratio,"
              "warmup_sparse_regime_ratio,dense_first_half_median_ns,"
              "dense_last_half_median_ns,dense_measurement_regime_ratio,"
              "dense_leading_median_ns,dense_trailing_median_ns,"
              "dense_edge_regime_ratio,"
              "sparse_first_half_median_ns,sparse_last_half_median_ns,"
              "sparse_measurement_regime_ratio,sparse_leading_median_ns,"
              "sparse_trailing_median_ns,sparse_edge_regime_ratio,"
              "load_seconds,full_qualification_seconds,warmup_seconds,"
              "measurement_seconds,dense_composition,sparse_composition\n";
    stream << std::fixed << std::setprecision(3);
    for (const auto& result : results)
    {
        const auto sampleCount = result.denseGpuSamples.size();
        Require(sampleCount == result.sparseGpuSamples.size() &&
                sampleCount == result.denseRecordingSamples.size() &&
                sampleCount == result.sparseRecordingSamples.size() &&
                sampleCount == result.denseFirst.size(),
            "Evidence raw sample列の件数が一致しません。");
        for (std::size_t sample = 0; sample < sampleCount; ++sample)
        {
            stream << result.width << ',' << result.height << ',' << result.universe << ','
                   << result.workset.densityNumerator << ','
                   << result.workset.densityDenominator << ','
                   << result.workset.name << ',' << result.workset.digestHex << ','
                   << result.workset.members.size() << ','
                   << static_cast<double>(result.workset.members.size()) / result.universe << ','
                   << result.workset.span << ',' << result.workset.contiguousRuns << ','
                   << result.workset.meanGap << ',' << result.workset.maxGap << ','
                   << sample << ',' << (result.denseFirst[sample] ? "A-B" : "B-A") << ','
                   << result.denseGpuSamples[sample] << ','
                   << result.sparseGpuSamples[sample] << ','
                   << result.denseRecordingSamples[sample] << ','
                   << result.sparseRecordingSamples[sample] << ','
                   << result.denseGpuMedianNs << ',' << result.sparseGpuMedianNs << ','
                   << result.denseOverSparse << ','
                   << result.denseRecordingMedianNs << ','
                   << result.sparseRecordingMedianNs << ','
                   << MeasurementStatusName(result.measurementStatus) << ','
                   << result.warmupFramesUsed << ','
                   << result.warmupDenseRegimeRatio << ','
                   << result.warmupSparseRegimeRatio << ','
                   << result.denseMeasurementRegime.firstHalfMedianNs << ','
                   << result.denseMeasurementRegime.lastHalfMedianNs << ','
                   << result.denseMeasurementRegime.symmetricRatio << ','
                   << result.denseMeasurementRegime.leadingMedianNs << ','
                   << result.denseMeasurementRegime.trailingMedianNs << ','
                   << result.denseMeasurementRegime.edgeSymmetricRatio << ','
                   << result.sparseMeasurementRegime.firstHalfMedianNs << ','
                   << result.sparseMeasurementRegime.lastHalfMedianNs << ','
                   << result.sparseMeasurementRegime.symmetricRatio << ','
                   << result.sparseMeasurementRegime.leadingMedianNs << ','
                   << result.sparseMeasurementRegime.trailingMedianNs << ','
                   << result.sparseMeasurementRegime.edgeSymmetricRatio << ','
                   << result.loadSeconds << ',' << result.fullQualificationSeconds << ','
                   << result.warmupSeconds << ','
                   << result.measurementSeconds << ','
                   << result.denseComposition << ',' << result.sparseComposition << '\n';
        }
    }

    const auto surfacePath = SurfacePath(arguments.output);
    std::ofstream surface(surfacePath, std::ios::binary | std::ios::trunc);
    if (!surface) Fail("Surface Evidence fileを作成できません。");
    surface << "# SGE4 Level 5 Vertical Experiment 2b Surface Summary\n";
    surface << "# classification=" << classification << "\n";
    surface << "# owner_decision=DeferredByOwner\n";
    surface << "# evidence_role=" << EvidenceRole(arguments) << "\n";
    surface << "width,height,universe,distribution,crossover_lower_active_ratio,"
               "crossover_upper_active_ratio,full_dense_over_sparse,"
               "stable_density_points,unstable_density_points,series_complete,"
               "monotonicity_violation,status\n";
    surface << std::fixed << std::setprecision(6);
    const auto* full = FindFullCase(results);
    Require(full != nullptr, "Surface Evidence用Full caseがありません。");
    constexpr std::array distributions = {
        DistributionKind::Prefix,
        DistributionKind::Suffix,
        DistributionKind::UniformStride,
        DistributionKind::Clustered4,
        DistributionKind::SeededRandom};
    for (const auto distribution : distributions)
    {
        if (arguments.boundaryRequalification)
        {
            const auto* boundary = FindSeriesPoint(
                results, distribution, DensityPoint{7, 8});
            Require(boundary != nullptr,
                "Boundary Surface Evidence用87.5% caseがありません。");
            const auto stablePoints =
                static_cast<std::size_t>(boundary->measurementStatus ==
                    MeasurementRegimeStatus::AcceptedStable) +
                static_cast<std::size_t>(full->measurementStatus ==
                    MeasurementRegimeStatus::AcceptedStable);
            const auto bracketed = stablePoints == 2u &&
                boundary->denseOverSparse > 1.05 &&
                full->denseOverSparse >= 0.95 &&
                full->denseOverSparse <= 1.05;
            const auto status = stablePoints != 2u
                ? "BoundaryRequalificationIncomplete"
                : (bracketed ? "BoundaryBracketRequalified"
                             : "BoundaryResultChanged");
            surface << arguments.width << ',' << arguments.height << ','
                    << arguments.width * arguments.height << ','
                    << DistributionName(distribution) << ','
                    << (bracketed ? 0.875 : -1.0) << ','
                    << (bracketed ? 1.0 : -1.0) << ','
                    << (full->measurementStatus ==
                            MeasurementRegimeStatus::AcceptedStable
                        ? full->denseOverSparse : -1.0) << ','
                    << stablePoints << ',' << 2u - stablePoints << ','
                    << (stablePoints == 2u ? "true" : "false") << ','
                    << "false," << status << '\n';
            continue;
        }
        const auto summary = SummarizeSeries(
            results, distribution, !arguments.forceWarp);
        surface << arguments.width << ',' << arguments.height << ','
                << arguments.width * arguments.height << ','
                << DistributionName(distribution) << ','
                << summary.lower << ',' << summary.upper << ','
                << (!arguments.forceWarp &&
                        full->measurementStatus ==
                            MeasurementRegimeStatus::AcceptedStable
                    ? full->denseOverSparse : -1.0) << ',' 
                << summary.stablePoints << ',' << summary.unstablePoints << ','
                << (summary.complete ? "true" : "false") << ','
                << (summary.monotonicityViolation ? "true" : "false") << ','
                << summary.status << '\n';
    }
}

void WriteTimingEvidence(
    const Arguments& arguments,
    std::span<const CaseResult> results,
    const WarmupQualification& globalWarmup,
    const RunTiming& timing,
    std::string_view classification)
{
    const auto path = TimingPath(arguments.output);
    std::ofstream stream(path, std::ios::binary | std::ios::trunc);
    if (!stream) Fail("Timing Evidence fileを作成できません。");
    const auto unstableCases = std::ranges::count_if(results,
        [](const CaseResult& result) {
            return result.measurementStatus ==
                MeasurementRegimeStatus::MeasurementRegimeTransition;
        });
    stream << "# SGE4 Level 5 Vertical Experiment 2b Timing Breakdown\n";
    stream << "# classification=" << classification << "\n";
    stream << "# owner_decision=DeferredByOwner\n";
    stream << "# evidence_role=" << EvidenceRole(arguments) << "\n";
    stream << "width,height,universe,case_count,accepted_case_count,"
              "unstable_case_count,global_warmup_frames_used,"
              "candidate_build_seconds,recovery_seconds,global_warmup_seconds,"
              "case_load_seconds,full_qualification_seconds,case_warmup_seconds,"
              "measurement_seconds,"
              "evidence_write_seconds,total_seconds\n";
    stream << std::fixed << std::setprecision(6)
           << arguments.width << ',' << arguments.height << ','
           << arguments.width * arguments.height << ',' << results.size() << ','
           << results.size() - unstableCases << ',' << unstableCases << ','
           << globalWarmup.framesUsed << ','
           << timing.candidateBuildSeconds << ',' << timing.recoverySeconds << ','
           << timing.globalWarmupSeconds << ',' << timing.caseLoadSeconds << ','
           << timing.fullQualificationSeconds << ','
           << timing.caseWarmupSeconds << ',' << timing.measurementSeconds << ','
           << timing.evidenceWriteSeconds << ',' << timing.totalSeconds << '\n';
}

void PrintResults(
    const Arguments& arguments,
    std::span<const CaseResult> results,
    std::string_view classification)
{
    std::cout << (arguments.boundaryRequalification
        ? "SGE4 Level 5 垂直実験2b Boundary Requalificationに合格しました。\n"
        : "SGE4 Level 5 垂直実験2bに合格しました。\n");
    std::cout << "Device：" << (arguments.forceWarp ? "WARP" : "実Hardware") << '\n';
    std::cout << "Universe：" << arguments.width * arguments.height
              << " (" << arguments.width << " x " << arguments.height << ")\n";
    std::cout << "Candidate A：Dense Direct + identity index list\n";
    std::cout << "Candidate B：Verified Compact Sparse Worklist\n";
    std::cout << std::fixed << std::setprecision(3);
    for (const auto& result : results)
    {
        const auto density = static_cast<double>(result.workset.densityNumerator) /
            result.workset.densityDenominator;
        std::cout << "[MEASURED] density=" << density
                  << " distribution=" << result.workset.name
                  << " K=" << result.workset.members.size()
                  << " dense=" << result.denseGpuMedianNs << " ns"
                  << " sparse=" << result.sparseGpuMedianNs << " ns"
                  << " dense/sparse=" << result.denseOverSparse
                  << " status=" << MeasurementStatusName(result.measurementStatus)
                  << " regime=" << result.denseMeasurementRegime.symmetricRatio
                  << '/' << result.sparseMeasurementRegime.symmetricRatio
                  << " edge=" << result.denseMeasurementRegime.edgeSymmetricRatio
                  << '/' << result.sparseMeasurementRegime.edgeSymmetricRatio << '\n';
    }
    std::cout << "[CLASSIFICATION] " << classification << '\n';
    std::cout << "[OWNER_DECISION] DeferredByOwner\n";
    std::cout << "Evidence：" << arguments.output.string() << '\n';
    std::cout << "Surface：" << SurfacePath(arguments.output).string() << '\n';
    std::cout << "Timing：" << TimingPath(arguments.output).string() << '\n';
}
}

int main(int argc, char** argv)
{
    const auto processBegin = std::chrono::steady_clock::now();
    try
    {
        const auto arguments = ParseArguments(argc, argv);
        RunTiming timing;

        const auto candidateBuildBegin = std::chrono::steady_clock::now();
        auto dense = experiment::BuildCandidate(
            experiment::CandidateKind::DenseDirect,
            arguments.width, arguments.height);
        if (!dense) Fail("Dense candidate生成に失敗しました：" + dense.error());
        auto sparse = experiment::BuildCandidate(
            experiment::CandidateKind::VerifiedSparseWorklist,
            arguments.width, arguments.height);
        if (!sparse) Fail("Sparse Worklist candidate生成に失敗しました：" + sparse.error());
        timing.candidateBuildSeconds = ElapsedSeconds(
            candidateBuildBegin, std::chrono::steady_clock::now());

        VerifyCandidateStructure(dense.value(), sparse.value());
        const auto universe = arguments.width * arguments.height;
        const auto recoveryDensity = DensityPoint{1, 2};
        const auto recoveryWorkset = BuildWorkset(
            DistributionKind::SeededRandom, universe,
            ActiveCount(universe, recoveryDensity), recoveryDensity);
        const auto recoveryBegin = std::chrono::steady_clock::now();
        VerifyControlledRecovery(
            arguments, dense.value(), sparse.value(), recoveryWorkset);
        timing.recoverySeconds = ElapsedSeconds(
            recoveryBegin, std::chrono::steady_clock::now());
        std::cout << "Density交差面pipelineのControlled Recoveryに合格しました。\n";

        sge4::d3d12::ExecutorOptions options;
        options.forceWarp = arguments.forceWarp;
        options.enableDebugLayer = arguments.enableDebugLayer;
        options.enableTimestampProfiling = true;
        sge4::d3d12::Executor executor(options);
        const auto globalWarmup = RunGlobalWarmup(
            arguments, executor, dense.value(), sparse.value());
        timing.globalWarmupSeconds = globalWarmup.elapsedSeconds;
        std::cout << "Universe専用global warmupを"
                  << globalWarmup.framesUsed << " frame実行しました（"
                  << (arguments.forceWarp ? "意味資格" : "性能収束")
                  << "）。\n";

        std::vector<CaseResult> results;
        auto worksets = arguments.boundaryRequalification
            ? BuildBoundaryRequalificationWorksets(universe)
            : BuildWorksets(universe);
        for (auto workset : std::move(worksets))
        {
            auto result = arguments.boundaryRequalification
                ? RunBoundaryRequalificationCase(
                    arguments, executor, dense.value(), sparse.value(),
                    std::move(workset))
                : RunCase(
                    arguments, executor, dense.value(), sparse.value(),
                    std::move(workset));
            timing.caseLoadSeconds += result.loadSeconds;
            timing.fullQualificationSeconds += result.fullQualificationSeconds;
            timing.caseWarmupSeconds += result.warmupSeconds;
            timing.measurementSeconds += result.measurementSeconds;
            std::cout << "density=" << result.workset.densityNumerator << '/'
                      << result.workset.densityDenominator
                      << " distribution=" << result.workset.name
                      << " warmup=" << result.warmupFramesUsed
                      << " status=" << MeasurementStatusName(result.measurementStatus)
                      << "の観測同値・Temporal履歴・測定を完了しました。\n";
            results.push_back(std::move(result));
        }
        const auto classification = Classify(arguments, results);
        const auto evidenceBegin = std::chrono::steady_clock::now();
        WriteEvidence(arguments, results, globalWarmup, classification);
        timing.evidenceWriteSeconds = ElapsedSeconds(
            evidenceBegin, std::chrono::steady_clock::now());
        timing.totalSeconds = ElapsedSeconds(
            processBegin, std::chrono::steady_clock::now());
        WriteTimingEvidence(
            arguments, results, globalWarmup, timing, classification);
        PrintResults(arguments, results, classification);
        return 0;
    }
    catch (const std::exception& exception)
    {
        std::cerr << "SGE4 Level 5 垂直実験2bに失敗しました："
                  << exception.what() << '\n';
        return 1;
    }
}
