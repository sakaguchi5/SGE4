#include "ArbitrarySparseWorklistFixture.h"
#include "../../src/backends/d3d12/runtime/Runtime.h"
#include "../../src/canonical/base/Sha256.h"

#include <algorithm>
#include <array>
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
#include <vector>

namespace
{
namespace experiment = sge4::level5::vertical2;

struct Arguments final
{
    bool forceWarp = false;
    bool enableDebugLayer = false;
    std::uint32_t width = 64;
    std::uint32_t height = 64;
    std::uint32_t warmupFrames = 6;
    std::uint32_t sampleFrames = 24;
    std::filesystem::path output = "level5_vertical_experiment_v2.csv";
};

enum class DistributionKind : std::uint32_t
{
    Prefix = 1,
    Suffix = 2,
    UniformStride = 3,
    Clustered4 = 4,
    SeededRandom = 5
};

struct Workset final
{
    DistributionKind kind = DistributionKind::Prefix;
    std::string name;
    std::vector<std::uint32_t> members;
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

struct CaseResult final
{
    Workset workset;
    double denseGpuMedianNs = 0.0;
    double sparseGpuMedianNs = 0.0;
    double denseRecordingMedianNs = 0.0;
    double sparseRecordingMedianNs = 0.0;
    double denseOverSparse = 0.0;
    std::vector<double> denseGpuSamples;
    std::vector<double> sparseGpuSamples;
    std::vector<double> denseRecordingSamples;
    std::vector<double> sparseRecordingSamples;
    std::vector<bool> denseFirst;
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
        else if (argument == "--width") result.width = ParseU32(takeValue(argument), argument);
        else if (argument == "--height") result.height = ParseU32(takeValue(argument), argument);
        else if (argument == "--warmup") result.warmupFrames = ParseU32(takeValue(argument), argument);
        else if (argument == "--samples") result.sampleFrames = ParseU32(takeValue(argument), argument);
        else if (argument == "--output") result.output = std::filesystem::path(takeValue(argument));
        else if (argument == "--quick")
        {
            result.width = 16;
            result.height = 16;
            result.warmupFrames = 2;
            result.sampleFrames = 4;
        }
        else
        {
            Fail("未知の引数です：" + std::string(argument));
        }
    }
    const std::uint64_t universe = static_cast<std::uint64_t>(result.width) * result.height;
    if (universe == 0 || universe > 65536u)
        Fail("width × heightがLevel 5実験2の範囲外です。");
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

std::vector<std::uint32_t> BuildCaseCounts(std::uint32_t universe)
{
    std::vector<std::uint32_t> values = {
        std::max(1u, universe / 64u),
        std::max(1u, universe / 16u),
        std::max(1u, universe / 4u)};
    std::ranges::sort(values);
    values.erase(std::unique(values.begin(), values.end()), values.end());
    return values;
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
    std::uint32_t activeCount)
{
    Workset result;
    result.kind = kind;
    result.name = DistributionName(kind);
    result.members = BuildMembers(kind, universe, activeCount);
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
    for (const auto activeCount : BuildCaseCounts(universe))
        for (const auto distribution : distributions)
            result.push_back(BuildWorkset(distribution, universe, activeCount));
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
    std::span<const std::uint32_t> activeMembers)
{
    auto planning = loaded.PlanningContext();
    auto input = experiment::MakeArbitraryInvocationInput(
        timelineOrdinal, planning.requiredMode, activeMembers,
        planning.requiredMode == sge4::dynamic::InvocationModeV1::ContinueHistory);
    auto invocation = experiment::BuildInvocation(
        loaded.Package(), planning.deviceEpoch, std::move(input),
        std::move(planning.previousHistory));
    if (!invocation)
        Fail("Level 5実験2 Invocation生成に失敗しました：" + invocation.error());

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
        Fail("Level 5実験2 Submitに失敗しました：" +
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

    auto temporalReadback = sge4::d3d12::ReadBuffer(
        loaded, candidate.temporalResource);
    if (!temporalReadback)
        Fail("Temporal Aggregate readbackに失敗しました：" +
            temporalReadback.error().stage + "：" + temporalReadback.error().message);
    const auto acceptedTemporal = DecodeFloat4<TemporalAggregate>(
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
    const auto texture = DecodeTextureEvidence(textureReadback.value(), candidate);

    const auto profiles = executor.ConsumeTimestampProfileSamples();
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
        "Level 5実験2 candidateのControlled Recoveryに失敗しました。");
    Require(static_cast<bool>(sge4::d3d12::AcknowledgeExternalRebind(dense.value())) &&
        static_cast<bool>(sge4::d3d12::AcknowledgeExternalRebind(sparse.value())),
        "Level 5実験2 candidateのExternal rebind確認に失敗しました。");

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

CaseResult RunCase(
    const Arguments& arguments,
    const experiment::CandidateBuild& denseCandidate,
    const experiment::CandidateBuild& sparseCandidate,
    Workset workset)
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
        Fail("Dense candidateのLoadに失敗しました：" +
            dense.error().stage + "：" + dense.error().message);

    sge4::d3d12::LoadInput sparseLoad;
    sparseLoad.initialResources = {{sparseCandidate.temporalResource, temporalSeed}};
    auto sparse = sge4::d3d12::LoadComposition(
        sparseCandidate.package.FileBytes(), executor, std::move(sparseLoad));
    if (!sparse)
        Fail("Sparse candidateのLoadに失敗しました：" +
            sparse.error().stage + "：" + sparse.error().message);

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

    const auto totalFrames = arguments.warmupFrames + arguments.sampleFrames;
    std::optional<TemporalAggregate> previousAcceptedTemporal;
    for (std::uint32_t frame = 0; frame < totalFrames; ++frame)
    {
        FrameResult denseResult;
        FrameResult sparseResult;
        if ((frame % 2u) == 0u)
        {
            denseResult = ExecuteFrame(
                dense.value(), executor, denseCandidate, frame, workset.members);
            sparseResult = ExecuteFrame(
                sparse.value(), executor, sparseCandidate, frame, workset.members);
        }
        else
        {
            sparseResult = ExecuteFrame(
                sparse.value(), executor, sparseCandidate, frame, workset.members);
            denseResult = ExecuteFrame(
                dense.value(), executor, denseCandidate, frame, workset.members);
        }
        Require(Equivalent(denseResult.observation, sparseResult.observation) &&
                NearlyEqual(denseResult.acceptedTemporal.combined,
                    sparseResult.acceptedTemporal.combined) &&
                denseResult.texture.digest == sparseResult.texture.digest &&
                std::abs(denseResult.texture.xSum - sparseResult.texture.xSum) <= 1.0e-6,
            "任意疎集合に対する候補のState／Temporal／Texture観測が一致しませんでした。");
        if (!previousAcceptedTemporal.has_value())
        {
            Require(NearlyEqual(denseResult.observation.previousStateSum, 0.0f),
                "最初のframeが明示zero Temporal seedを観測しませんでした。");
        }
        else
        {
            Require(NearlyEqual(
                    denseResult.observation.previousStateSum,
                    previousAcceptedTemporal->stateSum),
                "successful whole-submit後のTemporal rotationが観測されませんでした。");
        }
        Require(NearlyEqual(
                denseResult.observation.delta,
                denseResult.observation.stateSum -
                    denseResult.observation.previousStateSum),
            "Observation deltaがcurrent／Previous Temporal値と一致しませんでした。");
        previousAcceptedTemporal = denseResult.acceptedTemporal;
        if (frame >= arguments.warmupFrames)
        {
            denseGpu.push_back(denseResult.stateWriterProfile.gpuNanoseconds);
            sparseGpu.push_back(sparseResult.stateWriterProfile.gpuNanoseconds);
            denseRecording.push_back(
                denseResult.stateWriterProfile.commandRecordingNanoseconds);
            sparseRecording.push_back(
                sparseResult.stateWriterProfile.commandRecordingNanoseconds);
            denseFirst.push_back((frame % 2u) == 0u);
        }
    }

    CaseResult result;
    result.workset = std::move(workset);
    result.denseGpuMedianNs = Median(denseGpu);
    result.sparseGpuMedianNs = Median(sparseGpu);
    result.denseRecordingMedianNs = Median(denseRecording);
    result.sparseRecordingMedianNs = Median(sparseRecording);
    result.denseOverSparse = result.sparseGpuMedianNs > 0.0
        ? result.denseGpuMedianNs / result.sparseGpuMedianNs : 0.0;
    result.denseGpuSamples = std::move(denseGpu);
    result.sparseGpuSamples = std::move(sparseGpu);
    result.denseRecordingSamples = std::move(denseRecording);
    result.sparseRecordingSamples = std::move(sparseRecording);
    result.denseFirst = std::move(denseFirst);
    return result;
}

std::string Classify(std::span<const CaseResult> results)
{
    bool sparseWinsAll = !results.empty();
    bool denseWinsAny = false;
    bool distributionSensitive = false;
    for (const auto& result : results)
    {
        sparseWinsAll = sparseWinsAll && result.denseOverSparse > 1.05;
        denseWinsAny = denseWinsAny || result.denseOverSparse < 0.95;
    }
    std::vector<std::size_t> activeCounts;
    for (const auto& result : results)
        activeCounts.push_back(result.workset.members.size());
    std::ranges::sort(activeCounts);
    activeCounts.erase(
        std::unique(activeCounts.begin(), activeCounts.end()), activeCounts.end());
    for (const auto activeCount : activeCounts)
    {
        double minimum = std::numeric_limits<double>::max();
        double maximum = 0.0;
        for (const auto& result : results)
        {
            if (result.workset.members.size() != activeCount) continue;
            minimum = std::min(minimum, result.sparseGpuMedianNs);
            maximum = std::max(maximum, result.sparseGpuMedianNs);
        }
        if (minimum > 0.0 && maximum / minimum > 1.10)
            distributionSensitive = true;
    }
    if (denseWinsAny) return "MixedCandidateAdvantage";
    if (sparseWinsAll && distributionSensitive)
        return "ArbitrarySparseAdvantageDistributionSensitive";
    if (sparseWinsAll)
        return "ArbitrarySparseAdvantageDistributionStable";
    return "NoMaterialSeparation";
}

void WriteEvidence(
    const Arguments& arguments,
    const experiment::CandidateBuild& dense,
    const experiment::CandidateBuild& sparse,
    std::span<const CaseResult> results,
    std::string_view classification)
{
    if (arguments.output.has_parent_path())
        std::filesystem::create_directories(arguments.output.parent_path());
    std::ofstream stream(arguments.output, std::ios::binary | std::ios::trunc);
    if (!stream) Fail("Evidence fileを作成できません。");
    stream << "# SGE4 Level 5 Vertical Experiment 2\n";
    stream << "# subject=Arbitrary Sparse Worklist Distribution\n";
    stream << "# device=" << (arguments.forceWarp ? "WARP" : "Hardware") << "\n";
    stream << "# extent=" << arguments.width << 'x' << arguments.height << "\n";
    stream << "# universe=" << arguments.width * arguments.height << "\n";
    stream << "# warmup_frames_per_case=" << arguments.warmupFrames << "\n";
    stream << "# sample_frames_per_case=" << arguments.sampleFrames << "\n";
    stream << "# random_seed_scheme=L5VERT21-xorshift64star-universe-active\n";
    stream << "# dense_composition=" << sge4::base::ToHex(dense.package.SemanticDigest()) << "\n";
    stream << "# sparse_composition=" << sge4::base::ToHex(sparse.package.SemanticDigest()) << "\n";
    stream << "# classification=" << classification << "\n";
    stream << "# owner_decision=DeferredByOwner\n";
    stream << "distribution,workset_digest,active_count,active_ratio,span,"
              "contiguous_runs,mean_gap,max_gap,sample_index,execution_order,"
              "dense_gpu_ns,sparse_gpu_ns,dense_recording_ns,sparse_recording_ns,"
              "dense_gpu_median_ns,sparse_gpu_median_ns,dense_over_sparse,"
              "dense_recording_median_ns,sparse_recording_median_ns\n";
    stream << std::fixed << std::setprecision(3);
    const auto universe = static_cast<double>(arguments.width * arguments.height);
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
            stream << result.workset.name << ','
                   << result.workset.digestHex << ','
                   << result.workset.members.size() << ','
                   << static_cast<double>(result.workset.members.size()) / universe << ','
                   << result.workset.span << ','
                   << result.workset.contiguousRuns << ','
                   << result.workset.meanGap << ','
                   << result.workset.maxGap << ','
                   << sample << ','
                   << (result.denseFirst[sample] ? "A-B" : "B-A") << ','
                   << result.denseGpuSamples[sample] << ','
                   << result.sparseGpuSamples[sample] << ','
                   << result.denseRecordingSamples[sample] << ','
                   << result.sparseRecordingSamples[sample] << ','
                   << result.denseGpuMedianNs << ','
                   << result.sparseGpuMedianNs << ','
                   << result.denseOverSparse << ','
                   << result.denseRecordingMedianNs << ','
                   << result.sparseRecordingMedianNs << '\n';
        }
    }
}

void PrintResults(
    const Arguments& arguments,
    std::span<const CaseResult> results,
    std::string_view classification)
{
    std::cout << "SGE4 Level 5 垂直実験2に合格しました。\n";
    std::cout << "Device：" << (arguments.forceWarp ? "WARP" : "実Hardware") << '\n';
    std::cout << "Universe：" << arguments.width * arguments.height
              << " (" << arguments.width << " x " << arguments.height << ")\n";
    std::cout << "Candidate A：Dense Direct + identity index list\n";
    std::cout << "Candidate B：Verified Compact Sparse Worklist\n";
    std::cout << std::fixed << std::setprecision(3);
    for (const auto& result : results)
    {
        std::cout << "[MEASURED] distribution=" << result.workset.name
                  << " K=" << result.workset.members.size()
                  << " runs=" << result.workset.contiguousRuns
                  << " dense=" << result.denseGpuMedianNs << " ns"
                  << " sparse=" << result.sparseGpuMedianNs << " ns"
                  << " dense/sparse=" << result.denseOverSparse << '\n';
    }
    std::cout << "[CLASSIFICATION] " << classification << '\n';
    std::cout << "[OWNER_DECISION] DeferredByOwner\n";
    std::cout << "Evidence：" << arguments.output.string() << '\n';
}
}

int main(int argc, char** argv)
{
    try
    {
        const auto arguments = ParseArguments(argc, argv);
        auto dense = experiment::BuildCandidate(
            experiment::CandidateKind::DenseDirect,
            arguments.width, arguments.height);
        if (!dense) Fail("Dense candidate生成に失敗しました：" + dense.error());
        auto sparse = experiment::BuildCandidate(
            experiment::CandidateKind::VerifiedSparseWorklist,
            arguments.width, arguments.height);
        if (!sparse) Fail("Sparse Worklist candidate生成に失敗しました：" + sparse.error());

        VerifyCandidateStructure(dense.value(), sparse.value());
        const auto universe = arguments.width * arguments.height;
        const auto recoveryWorkset = BuildWorkset(
            DistributionKind::SeededRandom, universe,
            std::max(1u, universe / 16u));
        VerifyControlledRecovery(
            arguments, dense.value(), sparse.value(), recoveryWorkset);
        std::cout << "任意疎集合pipelineのControlled Recoveryに合格しました。\n";

        std::vector<CaseResult> results;
        for (auto workset : BuildWorksets(universe))
        {
            auto result = RunCase(
                arguments, dense.value(), sparse.value(), std::move(workset));
            std::cout << "distribution=" << result.workset.name
                      << " K=" << result.workset.members.size()
                      << "の観測同値・Temporal履歴・測定に合格しました。\n";
            results.push_back(std::move(result));
        }
        const auto classification = Classify(results);
        WriteEvidence(
            arguments, dense.value(), sparse.value(), results, classification);
        PrintResults(arguments, results, classification);
        return 0;
    }
    catch (const std::exception& exception)
    {
        std::cerr << "SGE4 Level 5 垂直実験2に失敗しました："
                  << exception.what() << '\n';
        return 1;
    }
}
