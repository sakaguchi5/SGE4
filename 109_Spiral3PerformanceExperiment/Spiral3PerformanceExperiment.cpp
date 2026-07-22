#ifndef NOMINMAX
#define NOMINMAX
#endif

#include "Spiral3PerformanceExperiment.h"

#include "../00_Foundation/FileIO.h"
#include "../00_Foundation/Sha256.h"
#include "../09_FrozenPackageCore/PackageReader.h"
#include "../14_D3D12Backend/D3D12Backend.h"
#include "../17_CompositionContract/CompositionContract.h"
#include "../18_CompositionPlan/CompositionPlan.h"
#include "../19_CompositionVerifier/CompositionVerifier.h"
#include "../22_CompositionRuntime/CompositionRuntime.h"
#include "../80_HierarchySemantic/HierarchySemantic.h"
#include "../81_Spiral2Contracts/Spiral2Contracts.h"
#include "../82_Spiral2Corpus/Spiral2Corpus.h"
#include "../100_ReuseSweepSemantic/ReuseSweepSemantic.h"
#include "../102_Spiral3Corpus/Spiral3Corpus.h"
#include "../104_ReuseRepresentationPlanner/ReuseRepresentationPlanner.h"
#include "../105_ReuseRepresentationVerifier/ReuseRepresentationVerifier.h"
#include "../106_Spiral3LeafCompiler/Spiral3LeafCompiler.h"
#include "../107_Spiral3Scenarios/Spiral3Scenarios.h"
#include "../108_Spiral3Execution/Spiral3Execution.h"

#if defined(_WIN32)
#include <Windows.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl/client.h>
#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "dxguid.lib")
#endif

#ifdef min
#undef min
#endif
#ifdef max
#undef max
#endif

#include <algorithm>
#include <array>
#include <bit>
#include <chrono>
#include <cctype>
#include <cmath>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <map>
#include <numeric>
#include <optional>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string_view>
#include <tuple>
#include <utility>

namespace sge4_5::spiral3::performance
{
namespace
{
using Clock = std::chrono::steady_clock;
namespace can = composition::canonical;
namespace canrt = composition::canonical::runtime_v1;
constexpr std::string_view EvidenceMagic = "SGE4-5.Spiral3.MeasurementEvidence.V1";
constexpr std::string_view MeasurementProfileDomain = "SGE4-5.Spiral3.MeasurementProfile.V1";

void Require(bool condition, std::string_view message)
{
    if (!condition) throw std::runtime_error(std::string(message));
}

std::uint64_t Nanoseconds(Clock::time_point begin, Clock::time_point end)
{
    return static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(end - begin).count());
}

void WriteString(base::BinaryWriter& writer, std::string_view value)
{
    writer.WriteU32(static_cast<std::uint32_t>(value.size()));
    writer.WriteBytes(std::as_bytes(std::span(value.data(), value.size())));
}

struct ReadText final { std::string value; };
base::Result<ReadText,std::string> ReadString(base::BinaryReader& reader)
{
    auto size = reader.ReadU32();
    if (!size) return base::Result<ReadText,std::string>::Failure(size.Error());
    auto bytes = reader.ReadBytes(size.Value());
    if (!bytes) return base::Result<ReadText,std::string>::Failure(bytes.Error());
    return base::Result<ReadText,std::string>::Success(ReadText{
        std::string(reinterpret_cast<const char*>(bytes.Value().data()), bytes.Value().size())});
}

void WriteDigest(base::BinaryWriter& writer, const base::Digest256& value)
{
    writer.WriteBytes(value);
}

base::Result<base::Digest256,std::string> ReadDigest(base::BinaryReader& reader)
{
    auto bytes = reader.ReadBytes(32);
    if (!bytes) return base::Result<base::Digest256,std::string>::Failure(bytes.Error());
    base::Digest256 result{};
    std::memcpy(result.data(), bytes.Value().data(), result.size());
    return base::Result<base::Digest256,std::string>::Success(result);
}

void WriteDouble(base::BinaryWriter& writer, double value)
{
    writer.WriteU64(std::bit_cast<std::uint64_t>(value));
}

base::Result<double,std::string> ReadDouble(base::BinaryReader& reader)
{
    auto value = reader.ReadU64();
    if (!value) return base::Result<double,std::string>::Failure(value.Error());
    return base::Result<double,std::string>::Success(std::bit_cast<double>(value.Value()));
}

std::string Hex(const base::Digest256& value)
{
    static constexpr char digits[] = "0123456789abcdef";
    std::string result(value.size() * 2, '0');
    for (std::size_t i = 0; i < value.size(); ++i)
    {
        const auto byte = std::to_integer<unsigned char>(value[i]);
        result[i * 2] = digits[byte >> 4];
        result[i * 2 + 1] = digits[byte & 15];
    }
    return result;
}

std::string JsonEscape(std::string_view value)
{
    std::string result;
    for (const char c : value)
    {
        if (c == '\\') result += "\\\\";
        else if (c == '"') result += "\\\"";
        else if (c == '\n') result += "\\n";
        else if (c == '\r') result += "\\r";
        else result += c;
    }
    return result;
}

const char* KindName(candidate::CandidateKindV1 kind)
{
    switch (kind)
    {
    case candidate::CandidateKindV1::MatrixHierarchy: return "A.MatrixBeforeHierarchy";
    case candidate::CandidateKindV1::DirectPgaHierarchy: return "B.DirectPgaThroughConsumer";
    case candidate::CandidateKindV1::HybridHierarchy: return "C.MatrixAfterHierarchy";
    default: return "Unknown";
    }
}

char KindLetter(candidate::CandidateKindV1 kind)
{
    switch (kind)
    {
    case candidate::CandidateKindV1::MatrixHierarchy: return 'A';
    case candidate::CandidateKindV1::DirectPgaHierarchy: return 'B';
    case candidate::CandidateKindV1::HybridHierarchy: return 'C';
    default: return '?';
    }
}

candidate::CandidateKindV1 RoleKind(std::string_view role)
{
    if (!role.empty() && role[0] == 'A') return candidate::CandidateKindV1::MatrixHierarchy;
    if (!role.empty() && role[0] == 'B') return candidate::CandidateKindV1::DirectPgaHierarchy;
    if (!role.empty() && role[0] == 'C') return candidate::CandidateKindV1::HybridHierarchy;
    return static_cast<candidate::CandidateKindV1>(0);
}

bool IsCandidateKind(candidate::CandidateKindV1 kind)
{
    return kind == candidate::CandidateKindV1::MatrixHierarchy ||
           kind == candidate::CandidateKindV1::DirectPgaHierarchy ||
           kind == candidate::CandidateKindV1::HybridHierarchy;
}

constexpr std::array<std::array<candidate::CandidateKindV1,3>,6> CandidateOrders{{
    {{candidate::CandidateKindV1::MatrixHierarchy, candidate::CandidateKindV1::DirectPgaHierarchy, candidate::CandidateKindV1::HybridHierarchy}},
    {{candidate::CandidateKindV1::MatrixHierarchy, candidate::CandidateKindV1::HybridHierarchy, candidate::CandidateKindV1::DirectPgaHierarchy}},
    {{candidate::CandidateKindV1::DirectPgaHierarchy, candidate::CandidateKindV1::MatrixHierarchy, candidate::CandidateKindV1::HybridHierarchy}},
    {{candidate::CandidateKindV1::DirectPgaHierarchy, candidate::CandidateKindV1::HybridHierarchy, candidate::CandidateKindV1::MatrixHierarchy}},
    {{candidate::CandidateKindV1::HybridHierarchy, candidate::CandidateKindV1::MatrixHierarchy, candidate::CandidateKindV1::DirectPgaHierarchy}},
    {{candidate::CandidateKindV1::HybridHierarchy, candidate::CandidateKindV1::DirectPgaHierarchy, candidate::CandidateKindV1::MatrixHierarchy}}
}};

std::string OrderName(const std::array<candidate::CandidateKindV1,3>& order)
{
    std::string value;
    for (const auto kind : order) value.push_back(KindLetter(kind));
    return value;
}

bool IsCanonicalOrder(std::string_view value)
{
    return std::any_of(CandidateOrders.begin(), CandidateOrders.end(), [&](const auto& order){return OrderName(order) == value;});
}

std::string ClassificationName(CrossoverClassificationV1 value)
{
    switch (value)
    {
    case CrossoverClassificationV1::NoObservedWinnerTransition: return "NoObservedWinnerTransition";
    case CrossoverClassificationV1::SingleStableWinnerTransition: return "SingleStableWinnerTransition";
    case CrossoverClassificationV1::MultipleWinnerTransitions: return "MultipleWinnerTransitions";
    default: return "Unknown";
    }
}

base::Digest256 PackageTargetProfileIdentity(const leaf::FrozenReuseLeafV1& value)
{
    auto parsedPackage = package::PackageReader::Read(value.packageBytes);
    if (!parsedPackage) throw std::runtime_error("could not parse Frozen Leaf Package: " + parsedPackage.Error().message);
    Require(parsedPackage.Value().Header().targetSchemaVersion == 17, "Spiral 3 measurement requires D3D12 Schema 17");
    Require(parsedPackage.Value().Header().minimumRuntimeVersion == 17, "Spiral 3 measurement requires Runtime 17");
    return parsedPackage.Value().Header().targetProfileDigest;
}

#if defined(_WIN32)
using Microsoft::WRL::ComPtr;
std::string WideToUtf8(const wchar_t* text)
{
    if (!text) return {};
    const int count = WideCharToMultiByte(CP_UTF8, 0, text, -1, nullptr, 0, nullptr, nullptr);
    if (count <= 1) return {};
    std::string value(static_cast<std::size_t>(count), '\0');
    WideCharToMultiByte(CP_UTF8, 0, text, -1, value.data(), count, nullptr, nullptr);
    value.pop_back();
    return value;
}

void Check(HRESULT result, std::string_view stage)
{
    if (FAILED(result))
    {
        std::ostringstream stream;
        stream << stage << " HRESULT=0x" << std::hex << static_cast<unsigned long>(result);
        throw std::runtime_error(stream.str());
    }
}

AdapterProfileV1 CaptureAdapterProfile(std::uint32_t requestedIndex)
{
    ComPtr<IDXGIFactory6> factory;
    Check(CreateDXGIFactory2(0, IID_PPV_ARGS(&factory)), "CreateDXGIFactory2");
    ComPtr<IDXGIAdapter1> adapter;
    ComPtr<ID3D12Device> device;
    std::uint32_t hardwareIndex = 0;
    for (UINT index = 0;; ++index)
    {
        ComPtr<IDXGIAdapter1> candidateAdapter;
        const HRESULT found = factory->EnumAdapterByGpuPreference(
            index, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, IID_PPV_ARGS(&candidateAdapter));
        if (found == DXGI_ERROR_NOT_FOUND) break;
        if (FAILED(found)) continue;
        DXGI_ADAPTER_DESC1 description{};
        candidateAdapter->GetDesc1(&description);
        if ((description.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) != 0) continue;
        ComPtr<ID3D12Device> candidateDevice;
        if (FAILED(D3D12CreateDevice(candidateAdapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&candidateDevice)))) continue;
        if (hardwareIndex++ != requestedIndex) continue;
        adapter = candidateAdapter;
        device = candidateDevice;
        break;
    }
    Require(device != nullptr, "no compatible non-software D3D12 hardware adapter was found");

    AdapterProfileV1 profile;
    DXGI_ADAPTER_DESC1 description{};
    adapter->GetDesc1(&description);
    profile.description = WideToUtf8(description.Description);
    profile.vendorId = description.VendorId;
    profile.deviceId = description.DeviceId;
    profile.subsystemId = description.SubSysId;
    profile.revision = description.Revision;
    profile.luidLow = description.AdapterLuid.LowPart;
    profile.luidHigh = description.AdapterLuid.HighPart;
    LARGE_INTEGER driver{};
    if (SUCCEEDED(adapter->CheckInterfaceSupport(__uuidof(IDXGIDevice), &driver)))
        profile.driverVersion = static_cast<std::uint64_t>(driver.QuadPart);
    D3D12_FEATURE_DATA_ARCHITECTURE1 architecture{};
    if (SUCCEEDED(device->CheckFeatureSupport(D3D12_FEATURE_ARCHITECTURE1, &architecture, sizeof(architecture))))
    {
        profile.uma = architecture.UMA != FALSE;
        profile.cacheCoherentUma = architecture.CacheCoherentUMA != FALSE;
    }
    D3D12_COMMAND_QUEUE_DESC queueDescription{};
    queueDescription.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    ComPtr<ID3D12CommandQueue> queue;
    Check(device->CreateCommandQueue(&queueDescription, IID_PPV_ARGS(&queue)), "CreateCommandQueue");
    Check(queue->GetTimestampFrequency(&profile.timestampFrequency), "GetTimestampFrequency");

    base::BinaryWriter writer;
    WriteString(writer, profile.description);
    writer.WriteU32(profile.vendorId);
    writer.WriteU32(profile.deviceId);
    writer.WriteU32(profile.subsystemId);
    writer.WriteU32(profile.revision);
    writer.WriteU32(profile.luidLow);
    writer.WriteI32(profile.luidHigh);
    writer.WriteU64(profile.driverVersion);
    writer.WriteU64(profile.timestampFrequency);
    writer.WriteU32(profile.uma ? 1u : 0u);
    writer.WriteU32(profile.cacheCoherentUma ? 1u : 0u);
    profile.fingerprint = base::Sha256(writer.Bytes());
    return profile;
}
#endif

can::EndpointReferenceDeclaration Ref(const std::string& leafRole, const std::string& endpoint)
{
    return {leafRole, endpoint};
}

can::LeafPackageDeclaration Decl(const leaf::FrozenReuseLeafV1& value)
{
    can::LeafPackageDeclaration declaration;
    declaration.stableKey = value.role;
    declaration.packageBytes = value.packageBytes;
    switch (value.operation)
    {
    case candidate::OperationKindV1::DynamicInvocationSource:
        declaration.endpoints = {{0,"motors"},{1,"reference"}};
        break;
    case candidate::OperationKindV1::ConsumerPointSource:
        declaration.endpoints = {{0,"points"}};
        break;
    case candidate::OperationKindV1::MatrixConsumerApply:
    case candidate::OperationKindV1::DirectPgaConsumerApply:
        declaration.endpoints = {{0,"transforms"},{1,"points"},{2,"output"}};
        break;
    default:
        declaration.endpoints = {{0,"input"},{1,"output"}};
        break;
    }
    return declaration;
}

struct CandidateScenario final
{
    candidate::CandidateKindV1 kind{};
    std::vector<leaf::FrozenReuseLeafV1> leaves;
    std::vector<std::byte> frozenCompositionBytes;
    base::Digest256 frozenCompositionDigest{};
    std::string referenceFlowKey;
    std::string pointFlowKey;
    std::string outputFlowKey;
    std::uint64_t compositionFreezeNanoseconds = 0;
};

std::pair<std::size_t,std::size_t> CandidateRange(candidate::CandidateKindV1 kind)
{
    switch (kind)
    {
    case candidate::CandidateKindV1::MatrixHierarchy: return {2,3};
    case candidate::CandidateKindV1::DirectPgaHierarchy: return {5,2};
    case candidate::CandidateKindV1::HybridHierarchy: return {7,3};
    default: throw std::runtime_error("invalid candidate kind");
    }
}

CandidateScenario BuildCandidateScenario(
    const scenarios::FrozenReuseComparisonScenarioV1& comparison,
    candidate::CandidateKindV1 kind)
{
    Require(comparison.leaves.size() == 11, "candidate measurement requires the qualified eleven-Leaf comparison");
    CandidateScenario result;
    result.kind = kind;
    result.leaves.push_back(comparison.leaves[0]);
    result.leaves.push_back(comparison.leaves[1]);
    const auto [first,count] = CandidateRange(kind);
    for (std::size_t index = 0; index < count; ++index)
        result.leaves.push_back(comparison.leaves[first + index]);

    const char letter = static_cast<char>(std::tolower(static_cast<unsigned char>(KindLetter(kind))));
    const std::string prefix = std::string("spiral3/measurement/") + letter;
    result.referenceFlowKey = prefix + "/reference";
    result.pointFlowKey = prefix + "/points";
    result.outputFlowKey = prefix + "/output";

    const auto freezeBegin = Clock::now();
    can::ContractBuildInput input;
    for (const auto& item : result.leaves) input.leaves.push_back(Decl(item));
    auto flow = [&](std::string key,
                    std::string producer,
                    std::string endpoint,
                    std::initializer_list<can::EndpointReferenceDeclaration> consumers,
                    can::ResourceBoundary boundary = can::ResourceBoundary::Internal)
    {
        can::ResourceFlowDeclaration declaration;
        declaration.stableKey = std::move(key);
        declaration.boundary = boundary;
        declaration.producer = Ref(producer, endpoint);
        declaration.consumers = consumers;
        input.resources.push_back(std::move(declaration));
    };

    const auto& source = result.leaves[0];
    const auto& points = result.leaves[1];
    const auto& firstCandidate = result.leaves[2];
    const auto& consumer = result.leaves.back();
    flow(prefix + "/motors", source.role, "motors", {Ref(firstCandidate.role,"input")});
    flow(result.referenceFlowKey, source.role, "reference", {}, can::ResourceBoundary::CompositionOutput);
    flow(result.pointFlowKey, points.role, "points", {Ref(consumer.role,"points")});
    for (std::size_t index = 2; index + 1 < result.leaves.size(); ++index)
    {
        const auto& current = result.leaves[index];
        const auto& next = result.leaves[index + 1];
        const std::string nextEndpoint =
            next.operation == candidate::OperationKindV1::MatrixConsumerApply ||
            next.operation == candidate::OperationKindV1::DirectPgaConsumerApply ? "transforms" : "input";
        flow(prefix + "/intermediate/" + std::to_string(index - 2),
             current.role, "output", {Ref(next.role,nextEndpoint)});
    }
    flow(result.outputFlowKey, consumer.role, "output", {}, can::ResourceBoundary::CompositionOutput);

    auto contract = can::BuildCompositionContract(std::move(input));
    if (!contract) throw std::runtime_error(contract.Error().stage + ": " + contract.Error().message);
    auto plan = can::planning::ProposeCompositionPlan(contract.Value());
    if (!plan) throw std::runtime_error(plan.Error().stage + ": " + plan.Error().message);
    auto verified = can::verification::VerifyAndSeal(contract.Value(), plan.Value());
    if (!verified) throw std::runtime_error(verified.Error().stage + ": " + verified.Error().message);
    auto frozen = can::verification::FreezeVerifiedComposition(contract.Value(), verified.Value());
    if (!frozen) throw std::runtime_error(frozen.Error().stage + ": " + frozen.Error().message);
    result.frozenCompositionBytes = std::move(frozen.Value());
    result.frozenCompositionDigest = base::Sha256(result.frozenCompositionBytes);
    result.compositionFreezeNanoseconds = Nanoseconds(freezeBegin, Clock::now());

    auto read = can::verification::ReadVerifiedFrozenComposition(result.frozenCompositionBytes);
    if (!read) throw std::runtime_error(read.Error().stage + ": " + read.Error().message);
    const auto& frozenContract = read.Value().ValidatedContract().Contract();
    Require(frozenContract.leaves.size() == result.leaves.size(), "candidate Composition Leaf count mismatch");
    Require(frozenContract.resources.size() == result.leaves.size() + 1, "candidate Composition Flow count mismatch");
    return result;
}

struct PreparedCandidate final
{
    CandidatePreparationV1 evidence;
};

PreparedCandidate PrepareCandidate(
    const spiral2::hierarchy::RigidHierarchySemanticV1& hierarchy,
    const corpus::ReuseCaseV1& reuseCase,
    candidate::CandidateKindV1 kind)
{
    PreparedCandidate result;
    result.evidence.kind = kind;

    const auto generationBegin = Clock::now();
    auto raw = planner::PlanCandidateGraphV1(hierarchy, reuseCase.semantic, kind);
    const auto generationEnd = Clock::now();
    if (!raw) throw std::runtime_error(raw.Error());

    const auto verifierBegin = Clock::now();
    auto verified = verification::VerifyCandidateGraphV1(hierarchy, reuseCase.semantic, raw.Value());
    const auto verifierEnd = Clock::now();
    if (!verified) throw std::runtime_error(verified.Error());

    const auto freezeBegin = Clock::now();
    auto group = leaf::CompileVerifiedReuseLeafGroupV1(hierarchy, reuseCase.semantic, verified.Value());
    if (!group) throw std::runtime_error(group.Error().stage + ": " + group.Error().message);
    base::Digest256 targetIdentity{};
    bool hasTarget = false;
    for (std::size_t index = 0; index < group.Value().leaves.size(); ++index)
    {
        const auto& item = group.Value().leaves[index];
        auto valid = leaf::ValidateFrozenReuseLeafV1(item);
        if (!valid) throw std::runtime_error(valid.Error().stage + ": " + valid.Error().message);
        result.evidence.candidatePackageBytes += item.packageBytes.size();
        if (index + 1 < group.Value().leaves.size())
            result.evidence.logicalIntermediateBytes +=
                static_cast<std::uint64_t>(item.primaryOutputElementCount) * item.primaryOutputStride;
        const auto packageTarget = PackageTargetProfileIdentity(item);
        if (!hasTarget) { targetIdentity = packageTarget; hasTarget = true; }
        else Require(targetIdentity == packageTarget, "candidate Leaves disagree on Target profile identity");
    }
    const auto freezeEnd = Clock::now();

    result.evidence.generationNanoseconds = Nanoseconds(generationBegin, generationEnd);
    result.evidence.verifierNanoseconds = Nanoseconds(verifierBegin, verifierEnd);
    result.evidence.leafFreezeNanoseconds = Nanoseconds(freezeBegin, freezeEnd);
    result.evidence.dynamicInvocationBytes =
        static_cast<std::uint64_t>(hierarchy.boneCount) * 32u +
        static_cast<std::uint64_t>(reuseCase.semantic.pointCount) * 16u;
    result.evidence.pointSourceBytes = static_cast<std::uint64_t>(reuseCase.semantic.pointCount) * 16u;
    result.evidence.plannedDispatchCount = verified.Value().Plan().candidateLeafDispatchCount;
    result.evidence.packageDispatchCount = static_cast<std::uint32_t>(group.Value().leaves.size());
    result.evidence.candidateLeafCount = static_cast<std::uint32_t>(group.Value().leaves.size());
    result.evidence.verifiedCertificate = verified.Value().CertificateIdentity();
    result.evidence.verifiedPlanIdentity = verified.Value().Plan().planIdentity;
    result.evidence.targetProfileIdentity = targetIdentity;
    return result;
}

std::vector<corpus::ObservedPointV1> DecodePoints(std::span<const std::byte> bytes, std::uint32_t pointCount)
{
    Require(bytes.size() == static_cast<std::size_t>(pointCount) * 16u, "candidate output byte count mismatch");
    base::BinaryReader reader(bytes);
    std::vector<corpus::ObservedPointV1> result;
    result.reserve(pointCount);
    for (std::uint32_t index = 0; index < pointCount; ++index)
    {
        auto x = reader.ReadU32();
        auto y = reader.ReadU32();
        auto z = reader.ReadU32();
        auto w = reader.ReadU32();
        Require(x && y && z && w, "candidate output is truncated");
        result.push_back({
            std::bit_cast<float>(x.Value()), std::bit_cast<float>(y.Value()),
            std::bit_cast<float>(z.Value()), std::bit_cast<float>(w.Value())});
    }
    return result;
}

struct CandidateRuntime final
{
    CandidateScenario scenario;
    canrt::LoadedStaticComposition loaded;
    std::uint64_t nextFrame = 0;

    CandidateRuntime(CandidateScenario scenarioValue, canrt::LoadedStaticComposition loadedValue)
        : scenario(std::move(scenarioValue)), loaded(std::move(loadedValue)) {}
};

struct CandidateExecution final
{
    std::vector<LeafTimingSampleV1> leaves;
    CandidateTimingSampleV1 candidate;
};

CandidateExecution ExecuteCandidate(
    std::uint32_t orderPosition,
    const spiral2::hierarchy::RigidHierarchySemanticV1& hierarchy,
    const corpus::ReuseCaseV1& reuseCase,
    const spiral2::contracts::DynamicLocalMotorPaletteV1& palette,
    std::span<const corpus::ObservedPointV1> reference,
    CandidateRuntime& runtime,
    d3d12::D3D12Backend& backend)
{
    const auto candidateBegin = Clock::now();
    Require(base::Sha256(runtime.loaded.Artifact().Artifact().FileBytes()) == runtime.scenario.frozenCompositionDigest,
            "loaded candidate Composition digest mismatch");
    const auto& contract = runtime.loaded.Artifact().ValidatedContract().Contract();
    const auto source = scenarios::FindLeafByAuthorKeyV1(contract, runtime.scenario.leaves[0].role);
    const auto referenceFlow = scenarios::FindFlowByAuthorKeyV1(contract, runtime.scenario.referenceFlowKey);
    const auto pointsFlow = scenarios::FindFlowByAuthorKeyV1(contract, runtime.scenario.pointFlowKey);
    const auto outputFlow = scenarios::FindFlowByAuthorKeyV1(contract, runtime.scenario.outputFlowKey);
    Require(source.IsValid() && referenceFlow.IsValid() && pointsFlow.IsValid() && outputFlow.IsValid(),
            "candidate measurement Composition identity lookup failed");

    const auto motorBytes = spiral2::contracts::SerializeDynamicLocalMotorPaletteV1(palette);
    const auto referenceBytes = corpus::SerializePointsV1(reference);
    const auto pointBytes = corpus::SerializePointsV1(reuseCase.localPoints);
    Require(runtime.scenario.leaves[0].expectedDynamicSlotCount == 2,
            "dynamic source does not expose exactly two qualified slots");
    Require(runtime.scenario.leaves[0].lockedDynamicBytes == motorBytes.size() &&
            runtime.scenario.leaves[0].lockedReferenceBytes == referenceBytes.size(),
            "dynamic source slot size differs from Frozen authority");

    canrt::StaticCompositionFrameInvocation invocation;
    invocation.frameNumber = runtime.nextFrame++;
    invocation.dynamicData = {{source,0u,motorBytes},{source,1u,referenceBytes}};
    auto submitted = canrt::SubmitStaticComposition(runtime.loaded, invocation);
    if (!submitted) throw std::runtime_error(submitted.Error().stage + ": " + submitted.Error().message);
    Require(submitted.Value().executionOrder.size() == runtime.scenario.leaves.size(),
            "candidate Composition executed an unexpected Leaf count");

    auto referenceRead = canrt::ReadStaticCompositionBuffer(runtime.loaded, referenceFlow);
    auto pointsRead = canrt::ReadStaticCompositionBuffer(runtime.loaded, pointsFlow);
    auto outputRead = canrt::ReadStaticCompositionBuffer(runtime.loaded, outputFlow);
    if (!referenceRead) throw std::runtime_error(referenceRead.Error().stage + ": " + referenceRead.Error().message);
    if (!pointsRead) throw std::runtime_error(pointsRead.Error().stage + ": " + pointsRead.Error().message);
    if (!outputRead) throw std::runtime_error(outputRead.Error().stage + ": " + outputRead.Error().message);
    Require(referenceRead.Value().bytes == referenceBytes, "dynamic reference source bytes changed");
    Require(pointsRead.Value().bytes == pointBytes, "Package-owned point source bytes changed");

    auto observedPoints = DecodePoints(outputRead.Value().bytes, reuseCase.semantic.pointCount);
    auto observation = execution::ObserveReuseComparisonV1(
        reference, observedPoints, observedPoints, observedPoints,
        hierarchy.boneCount, reuseCase.reuseCount);
    if (!observation) throw std::runtime_error(observation.Error());
    Require(observation.Value().mismatchCount == 0 && observation.Value().nonFiniteCount == 0,
            "candidate output violates the qualified Observation Contract");

    auto profiles = backend.ConsumeTimestampProfileSamples();
    Require(profiles.size() == runtime.scenario.leaves.size(), "candidate timestamp count mismatch");
    std::set<std::string> matchedRoles;
    CandidateExecution result;
    result.candidate.kind = runtime.scenario.kind;
    result.candidate.orderPosition = orderPosition;
    for (const auto& profile : profiles)
    {
        auto leafIt = std::find_if(runtime.scenario.leaves.begin(), runtime.scenario.leaves.end(),
            [&](const auto& value){return value.executionDigest == profile.packageExecutionDigest;});
        Require(leafIt != runtime.scenario.leaves.end(), "timestamp profile Package identity is not in candidate Composition");
        Require(matchedRoles.insert(leafIt->role).second, "duplicate timestamp profile for candidate Leaf");
        LeafTimingSampleV1 leafTiming;
        leafTiming.role = leafIt->role;
        leafTiming.kind = RoleKind(leafIt->role);
        leafTiming.commandRecordingNanoseconds = profile.commandRecordingNanoseconds;
        leafTiming.gpuNanoseconds = profile.gpuNanoseconds;
        leafTiming.dispatchCount = profile.dispatchCount;
        leafTiming.barrierCount = profile.barrierCount;
        result.leaves.push_back(leafTiming);
        if (leafTiming.kind == runtime.scenario.kind)
        {
            result.candidate.commandRecordingNanoseconds += leafTiming.commandRecordingNanoseconds;
            result.candidate.gpuTotalNanoseconds += leafTiming.gpuNanoseconds;
            result.candidate.dispatchCount += leafTiming.dispatchCount;
            result.candidate.barrierCount += leafTiming.barrierCount;
        }
    }
    Require(result.candidate.dispatchCount > 0, "candidate timestamp profile did not include candidate work");
    result.candidate.numericMaxAbsoluteError = observation.Value().maxAbsoluteError;
    result.candidate.numericMaxRelativeError = observation.Value().maxRelativeError;
    result.candidate.numericMaxUlpDistance = observation.Value().maxUlpDistance;
    result.candidate.endToEndNanoseconds = static_cast<double>(Nanoseconds(candidateBegin, Clock::now()));
    return result;
}

MeasurementSampleV1 ExecuteSample(
    std::uint32_t run,
    std::uint32_t cycle,
    const std::array<candidate::CandidateKindV1,3>& order,
    const spiral2::hierarchy::RigidHierarchySemanticV1& hierarchy,
    const corpus::ReuseCaseV1& reuseCase,
    const spiral2::contracts::DynamicLocalMotorPaletteV1& palette,
    std::span<const corpus::ObservedPointV1> reference,
    std::array<CandidateRuntime*,3> runtimes,
    d3d12::D3D12Backend& backend)
{
    MeasurementSampleV1 result;
    result.run = run;
    result.cycle = cycle;
    result.candidateOrder = OrderName(order);
    for (std::uint32_t position = 0; position < 3; ++position)
    {
        auto runtime = std::find_if(runtimes.begin(), runtimes.end(),
            [&](const auto* value){return value->scenario.kind == order[position];});
        Require(runtime != runtimes.end(), "candidate runtime was not found for order");
        auto executed = ExecuteCandidate(position, hierarchy, reuseCase, palette, reference, **runtime, backend);
        result.leaves.insert(result.leaves.end(), executed.leaves.begin(), executed.leaves.end());
        result.candidates.push_back(executed.candidate);
    }
    return result;
}

void WritePreparation(base::BinaryWriter& writer, const CandidatePreparationV1& value)
{
    writer.WriteU32(static_cast<std::uint32_t>(value.kind));
    writer.WriteU64(value.generationNanoseconds);
    writer.WriteU64(value.verifierNanoseconds);
    writer.WriteU64(value.leafFreezeNanoseconds);
    writer.WriteU64(value.compositionFreezeNanoseconds);
    writer.WriteU64(value.materializationNanoseconds);
    writer.WriteU64(value.candidatePackageBytes);
    writer.WriteU64(value.sourcePackageBytes);
    writer.WriteU64(value.frozenCompositionBytes);
    writer.WriteU64(value.dynamicInvocationBytes);
    writer.WriteU64(value.pointSourceBytes);
    writer.WriteU64(value.logicalIntermediateBytes);
    writer.WriteU32(value.plannedDispatchCount);
    writer.WriteU32(value.packageDispatchCount);
    writer.WriteU32(value.candidateLeafCount);
    writer.WriteU32(value.compositionLeafCount);
    WriteDigest(writer, value.verifiedCertificate);
    WriteDigest(writer, value.verifiedPlanIdentity);
    WriteDigest(writer, value.targetProfileIdentity);
    WriteDigest(writer, value.candidateCompositionDigest);
}

base::Result<CandidatePreparationV1,std::string> ReadPreparation(base::BinaryReader& reader)
{
    CandidatePreparationV1 value;
    auto kind = reader.ReadU32();
    auto generation = reader.ReadU64();
    auto verifier = reader.ReadU64();
    auto leafFreeze = reader.ReadU64();
    auto compositionFreeze = reader.ReadU64();
    auto materialization = reader.ReadU64();
    auto packageBytes = reader.ReadU64();
    auto sourceBytes = reader.ReadU64();
    auto frozenBytes = reader.ReadU64();
    auto dynamicBytes = reader.ReadU64();
    auto pointBytes = reader.ReadU64();
    auto intermediateBytes = reader.ReadU64();
    auto plannedDispatches = reader.ReadU32();
    auto packageDispatches = reader.ReadU32();
    auto candidateLeaves = reader.ReadU32();
    auto compositionLeaves = reader.ReadU32();
    auto certificate = ReadDigest(reader);
    auto plan = ReadDigest(reader);
    auto target = ReadDigest(reader);
    auto composition = ReadDigest(reader);
    if (!kind || !generation || !verifier || !leafFreeze || !compositionFreeze || !materialization ||
        !packageBytes || !sourceBytes || !frozenBytes || !dynamicBytes || !pointBytes || !intermediateBytes ||
        !plannedDispatches || !packageDispatches || !candidateLeaves || !compositionLeaves ||
        !certificate || !plan || !target || !composition)
        return base::Result<CandidatePreparationV1,std::string>::Failure("candidate preparation section is truncated");
    value.kind = static_cast<candidate::CandidateKindV1>(kind.Value());
    value.generationNanoseconds = generation.Value();
    value.verifierNanoseconds = verifier.Value();
    value.leafFreezeNanoseconds = leafFreeze.Value();
    value.compositionFreezeNanoseconds = compositionFreeze.Value();
    value.materializationNanoseconds = materialization.Value();
    value.candidatePackageBytes = packageBytes.Value();
    value.sourcePackageBytes = sourceBytes.Value();
    value.frozenCompositionBytes = frozenBytes.Value();
    value.dynamicInvocationBytes = dynamicBytes.Value();
    value.pointSourceBytes = pointBytes.Value();
    value.logicalIntermediateBytes = intermediateBytes.Value();
    value.plannedDispatchCount = plannedDispatches.Value();
    value.packageDispatchCount = packageDispatches.Value();
    value.candidateLeafCount = candidateLeaves.Value();
    value.compositionLeafCount = compositionLeaves.Value();
    value.verifiedCertificate = certificate.Value();
    value.verifiedPlanIdentity = plan.Value();
    value.targetProfileIdentity = target.Value();
    value.candidateCompositionDigest = composition.Value();
    return base::Result<CandidatePreparationV1,std::string>::Success(value);
}

void WriteLeaf(base::BinaryWriter& writer, const LeafTimingSampleV1& value)
{
    WriteString(writer, value.role);
    writer.WriteU32(static_cast<std::uint32_t>(value.kind));
    WriteDouble(writer, value.commandRecordingNanoseconds);
    WriteDouble(writer, value.gpuNanoseconds);
    writer.WriteU32(value.dispatchCount);
    writer.WriteU32(value.barrierCount);
}

base::Result<LeafTimingSampleV1,std::string> ReadLeaf(base::BinaryReader& reader)
{
    LeafTimingSampleV1 value;
    auto role = ReadString(reader);
    auto kind = reader.ReadU32();
    auto command = ReadDouble(reader);
    auto gpu = ReadDouble(reader);
    auto dispatches = reader.ReadU32();
    auto barriers = reader.ReadU32();
    if (!role || !kind || !command || !gpu || !dispatches || !barriers)
        return base::Result<LeafTimingSampleV1,std::string>::Failure("Leaf timing section is truncated");
    value.role = std::move(role.Value().value);
    value.kind = static_cast<candidate::CandidateKindV1>(kind.Value());
    value.commandRecordingNanoseconds = command.Value();
    value.gpuNanoseconds = gpu.Value();
    value.dispatchCount = dispatches.Value();
    value.barrierCount = barriers.Value();
    return base::Result<LeafTimingSampleV1,std::string>::Success(std::move(value));
}

void WriteCandidateSample(base::BinaryWriter& writer, const CandidateTimingSampleV1& value)
{
    writer.WriteU32(static_cast<std::uint32_t>(value.kind));
    writer.WriteU32(value.orderPosition);
    WriteDouble(writer, value.endToEndNanoseconds);
    WriteDouble(writer, value.commandRecordingNanoseconds);
    WriteDouble(writer, value.gpuTotalNanoseconds);
    WriteDouble(writer, value.numericMaxAbsoluteError);
    WriteDouble(writer, value.numericMaxRelativeError);
    writer.WriteU64(value.numericMaxUlpDistance);
    writer.WriteU32(value.dispatchCount);
    writer.WriteU32(value.barrierCount);
}

base::Result<CandidateTimingSampleV1,std::string> ReadCandidateSample(base::BinaryReader& reader)
{
    CandidateTimingSampleV1 value;
    auto kind = reader.ReadU32();
    auto position = reader.ReadU32();
    auto endToEnd = ReadDouble(reader);
    auto command = ReadDouble(reader);
    auto gpu = ReadDouble(reader);
    auto absolute = ReadDouble(reader);
    auto relative = ReadDouble(reader);
    auto ulp = reader.ReadU64();
    auto dispatches = reader.ReadU32();
    auto barriers = reader.ReadU32();
    if (!kind || !position || !endToEnd || !command || !gpu || !absolute || !relative || !ulp || !dispatches || !barriers)
        return base::Result<CandidateTimingSampleV1,std::string>::Failure("candidate timing section is truncated");
    value.kind = static_cast<candidate::CandidateKindV1>(kind.Value());
    value.orderPosition = position.Value();
    value.endToEndNanoseconds = endToEnd.Value();
    value.commandRecordingNanoseconds = command.Value();
    value.gpuTotalNanoseconds = gpu.Value();
    value.numericMaxAbsoluteError = absolute.Value();
    value.numericMaxRelativeError = relative.Value();
    value.numericMaxUlpDistance = ulp.Value();
    value.dispatchCount = dispatches.Value();
    value.barrierCount = barriers.Value();
    return base::Result<CandidateTimingSampleV1,std::string>::Success(value);
}

base::Result<MeasurementEvidenceV1,std::string> DeserializeEvidence(std::span<const std::byte> bytes)
{
    base::BinaryReader reader(bytes);
    auto magic = ReadString(reader);
    if (!magic || magic.Value().value != EvidenceMagic)
        return base::Result<MeasurementEvidenceV1,std::string>::Failure("measurement evidence magic mismatch");
    MeasurementEvidenceV1 evidence;
    auto version = reader.ReadU32();
    auto capture = reader.ReadU64();
    if (!version || !capture || version.Value() != MeasurementEvidenceSchemaVersionV1)
        return base::Result<MeasurementEvidenceV1,std::string>::Failure("measurement evidence version mismatch");
    evidence.schemaVersion = version.Value();
    evidence.captureUnixNanoseconds = capture.Value();

    auto warmup = reader.ReadU32();
    auto measurement = reader.ReadU32();
    auto runs = reader.ReadU32();
    auto adapterIndex = reader.ReadU32();
    auto frameIndex = reader.ReadU32();
    auto clockPolicy = ReadString(reader);
    if (!warmup || !measurement || !runs || !adapterIndex || !frameIndex || !clockPolicy)
        return base::Result<MeasurementEvidenceV1,std::string>::Failure("measurement config is truncated");
    evidence.config = {warmup.Value(),measurement.Value(),runs.Value(),adapterIndex.Value(),frameIndex.Value(),clockPolicy.Value().value};

    auto description = ReadString(reader);
    auto vendor = reader.ReadU32();
    auto device = reader.ReadU32();
    auto subsystem = reader.ReadU32();
    auto revision = reader.ReadU32();
    auto luidLow = reader.ReadU32();
    auto luidHigh = reader.ReadI32();
    auto driver = reader.ReadU64();
    auto frequency = reader.ReadU64();
    auto uma = reader.ReadU32();
    auto coherent = reader.ReadU32();
    auto fingerprint = ReadDigest(reader);
    auto target = ReadDigest(reader);
    auto observation = ReadDigest(reader);
    auto profile = ReadDigest(reader);
    if (!description || !vendor || !device || !subsystem || !revision || !luidLow || !luidHigh ||
        !driver || !frequency || !uma || !coherent || !fingerprint || !target || !observation || !profile)
        return base::Result<MeasurementEvidenceV1,std::string>::Failure("adapter/profile section is truncated");
    evidence.adapter.description = description.Value().value;
    evidence.adapter.vendorId = vendor.Value();
    evidence.adapter.deviceId = device.Value();
    evidence.adapter.subsystemId = subsystem.Value();
    evidence.adapter.revision = revision.Value();
    evidence.adapter.luidLow = luidLow.Value();
    evidence.adapter.luidHigh = luidHigh.Value();
    evidence.adapter.driverVersion = driver.Value();
    evidence.adapter.timestampFrequency = frequency.Value();
    evidence.adapter.uma = uma.Value() != 0;
    evidence.adapter.cacheCoherentUma = coherent.Value() != 0;
    evidence.adapter.fingerprint = fingerprint.Value();
    evidence.targetProfileIdentity = target.Value();
    evidence.observationContractIdentity = observation.Value();
    evidence.measurementProfileIdentity = profile.Value();

    auto caseCount = reader.ReadU32();
    if (!caseCount) return base::Result<MeasurementEvidenceV1,std::string>::Failure("case count is truncated");
    for (std::uint32_t caseIndex = 0; caseIndex < caseCount.Value(); ++caseIndex)
    {
        MeasurementCaseV1 item;
        auto id = ReadString(reader);
        auto bones = reader.ReadU32();
        auto depth = reader.ReadU32();
        auto reuse = reader.ReadU32();
        auto points = reader.ReadU32();
        auto hierarchy = ReadDigest(reader);
        auto semantic = ReadDigest(reader);
        auto corpus = ReadDigest(reader);
        auto invocation = ReadDigest(reader);
        auto scenario = ReadDigest(reader);
        auto preparationCount = reader.ReadU32();
        if (!id || !bones || !depth || !reuse || !points || !hierarchy || !semantic || !corpus || !invocation || !scenario || !preparationCount)
            return base::Result<MeasurementEvidenceV1,std::string>::Failure("case header is truncated");
        item.id = id.Value().value;
        item.boneCount = bones.Value();
        item.hierarchyDepthCount = depth.Value();
        item.reuseCount = reuse.Value();
        item.pointCount = points.Value();
        item.hierarchyIdentity = hierarchy.Value();
        item.reuseSemanticIdentity = semantic.Value();
        item.localPointCorpusIdentity = corpus.Value();
        item.invocationIdentity = invocation.Value();
        item.scenarioIdentity = scenario.Value();
        for (std::uint32_t index = 0; index < preparationCount.Value(); ++index)
        {
            auto preparation = ReadPreparation(reader);
            if (!preparation) return base::Result<MeasurementEvidenceV1,std::string>::Failure(preparation.Error());
            item.preparation.push_back(preparation.Value());
        }
        auto sampleCount = reader.ReadU32();
        if (!sampleCount) return base::Result<MeasurementEvidenceV1,std::string>::Failure("sample count is truncated");
        for (std::uint32_t sampleIndex = 0; sampleIndex < sampleCount.Value(); ++sampleIndex)
        {
            MeasurementSampleV1 sample;
            auto run = reader.ReadU32();
            auto cycle = reader.ReadU32();
            auto order = ReadString(reader);
            auto leafCount = reader.ReadU32();
            if (!run || !cycle || !order || !leafCount)
                return base::Result<MeasurementEvidenceV1,std::string>::Failure("sample header is truncated");
            sample.run = run.Value();
            sample.cycle = cycle.Value();
            sample.candidateOrder = order.Value().value;
            for (std::uint32_t index = 0; index < leafCount.Value(); ++index)
            {
                auto timing = ReadLeaf(reader);
                if (!timing) return base::Result<MeasurementEvidenceV1,std::string>::Failure(timing.Error());
                sample.leaves.push_back(std::move(timing.Value()));
            }
            auto candidateCount = reader.ReadU32();
            if (!candidateCount) return base::Result<MeasurementEvidenceV1,std::string>::Failure("candidate sample count is truncated");
            for (std::uint32_t index = 0; index < candidateCount.Value(); ++index)
            {
                auto timing = ReadCandidateSample(reader);
                if (!timing) return base::Result<MeasurementEvidenceV1,std::string>::Failure(timing.Error());
                sample.candidates.push_back(timing.Value());
            }
            item.samples.push_back(std::move(sample));
        }
        evidence.cases.push_back(std::move(item));
    }
    if (reader.Remaining() != 0)
        return base::Result<MeasurementEvidenceV1,std::string>::Failure("measurement evidence has trailing bytes");
    return base::Result<MeasurementEvidenceV1,std::string>::Success(std::move(evidence));
}

double Median(std::vector<double> values)
{
    if (values.empty()) return 0.0;
    std::sort(values.begin(), values.end());
    const auto middle = values.size() / 2;
    return values.size() % 2 ? values[middle] : (values[middle - 1] + values[middle]) * 0.5;
}

std::vector<double> GpuValues(const MeasurementCaseV1& item, candidate::CandidateKindV1 kind)
{
    std::vector<double> values;
    for (const auto& sample : item.samples)
        for (const auto& candidateSample : sample.candidates)
            if (candidateSample.kind == kind) values.push_back(candidateSample.gpuTotalNanoseconds);
    return values;
}

std::vector<double> AllGpuValues(const MeasurementEvidenceV1& evidence, candidate::CandidateKindV1 kind)
{
    std::vector<double> values;
    for (const auto& item : evidence.cases)
    {
        auto more = GpuValues(item, kind);
        values.insert(values.end(), more.begin(), more.end());
    }
    return values;
}

candidate::CandidateKindV1 Winner(const MeasurementCaseV1& item)
{
    const std::array kinds{
        candidate::CandidateKindV1::MatrixHierarchy,
        candidate::CandidateKindV1::DirectPgaHierarchy,
        candidate::CandidateKindV1::HybridHierarchy};
    auto winner = kinds[0];
    double best = Median(GpuValues(item, winner));
    Require(best > 0.0, "case has no positive GPU timing evidence");
    for (std::size_t index = 1; index < kinds.size(); ++index)
    {
        const double candidateMedian = Median(GpuValues(item, kinds[index]));
        Require(candidateMedian > 0.0, "case has incomplete GPU timing evidence");
        if (candidateMedian < best)
        {
            best = candidateMedian;
            winner = kinds[index];
        }
    }
    return winner;
}

std::vector<std::pair<std::uint32_t,std::uint32_t>> PairwiseIntervals(
    const MeasurementEvidenceV1& evidence,
    candidate::CandidateKindV1 left,
    candidate::CandidateKindV1 right)
{
    std::vector<std::pair<std::uint32_t,std::uint32_t>> intervals;
    for (std::size_t index = 1; index < evidence.cases.size(); ++index)
    {
        const auto& previous = evidence.cases[index - 1];
        const auto& current = evidence.cases[index];
        const double previousDelta = Median(GpuValues(previous,left)) - Median(GpuValues(previous,right));
        const double currentDelta = Median(GpuValues(current,left)) - Median(GpuValues(current,right));
        if ((previousDelta < 0.0 && currentDelta > 0.0) || (previousDelta > 0.0 && currentDelta < 0.0) || previousDelta == 0.0 || currentDelta == 0.0)
            intervals.emplace_back(previous.reuseCount,current.reuseCount);
    }
    return intervals;
}

void ValidateEvidenceForDecision(const MeasurementEvidenceV1& evidence)
{
    Require(evidence.schemaVersion == MeasurementEvidenceSchemaVersionV1, "Decision Report schema version mismatch");
    Require(evidence.cases.size() == CanonicalReuseCaseCountV1, "Decision Report requires all six R cases");
    Require(evidence.adapter.timestampFrequency > 0, "hardware timestamp frequency is missing");
    Require(evidence.targetProfileIdentity != base::Digest256{}, "Target profile identity is missing");
    Require(evidence.observationContractIdentity != base::Digest256{}, "Observation Contract identity is missing");
    std::uint32_t previousReuse = 0;
    for (const auto& item : evidence.cases)
    {
        Require(item.reuseCount > previousReuse, "R cases must be strictly increasing");
        previousReuse = item.reuseCount;
        Require(item.pointCount == item.boneCount * item.reuseCount, "case point count does not match bone/reuse shape");
        Require(item.preparation.size() == 3, "case must contain three candidate preparation records");
        Require(item.samples.size() == static_cast<std::size_t>(evidence.config.measurementCyclesPerOrder) * evidence.config.runCount * CandidateOrders.size(),
                "case sample count does not match measurement configuration");
        for (const auto& sample : item.samples)
        {
            Require(IsCanonicalOrder(sample.candidateOrder), "sample contains a non-canonical candidate order");
            Require(sample.candidates.size() == 3, "sample does not contain three candidate timings");
            std::set<std::uint32_t> kinds;
            for (const auto& timing : sample.candidates)
            {
                Require(IsCandidateKind(timing.kind), "sample contains an invalid candidate kind");
                Require(kinds.insert(static_cast<std::uint32_t>(timing.kind)).second, "sample contains duplicate candidate timing");
                Require(timing.gpuTotalNanoseconds > 0.0, "sample contains non-positive GPU timing");
            }
        }
    }
}

void WriteCsv(const MeasurementEvidenceV1& evidence, const std::filesystem::path& path)
{
    std::ofstream file(path, std::ios::binary);
    if (!file) throw std::runtime_error("failed to create measurement_samples.csv");
    file << "case,reuse,points,run,cycle,order,order_position,candidate,leaf,command_recording_ns,gpu_ns,candidate_e2e_ns,candidate_materialization_ns,dispatches,barriers,max_abs_error,max_rel_error,max_ulp\r\n"
         << std::setprecision(17);
    for (const auto& item : evidence.cases)
        for (const auto& sample : item.samples)
            for (const auto& leafSample : sample.leaves)
            {
                if (!IsCandidateKind(leafSample.kind)) continue;
                const auto timing = std::find_if(sample.candidates.begin(), sample.candidates.end(),
                    [&](const auto& value){return value.kind == leafSample.kind;});
                const auto preparation = std::find_if(item.preparation.begin(), item.preparation.end(),
                    [&](const auto& value){return value.kind == leafSample.kind;});
                file << item.id << ',' << item.reuseCount << ',' << item.pointCount << ','
                     << sample.run << ',' << sample.cycle << ',' << sample.candidateOrder << ','
                     << (timing == sample.candidates.end() ? 0 : timing->orderPosition) << ','
                     << KindName(leafSample.kind) << ',' << leafSample.role << ','
                     << leafSample.commandRecordingNanoseconds << ',' << leafSample.gpuNanoseconds << ','
                     << (timing == sample.candidates.end() ? 0.0 : timing->endToEndNanoseconds) << ','
                     << (preparation == item.preparation.end() ? 0u : preparation->materializationNanoseconds) << ','
                     << leafSample.dispatchCount << ',' << leafSample.barrierCount << ','
                     << (timing == sample.candidates.end() ? 0.0 : timing->numericMaxAbsoluteError) << ','
                     << (timing == sample.candidates.end() ? 0.0 : timing->numericMaxRelativeError) << ','
                     << (timing == sample.candidates.end() ? 0u : timing->numericMaxUlpDistance) << "\r\n";
            }
}

} // namespace

std::vector<std::byte> SerializeMeasurementEvidenceV1(const MeasurementEvidenceV1& evidence)
{
    base::BinaryWriter writer;
    WriteString(writer, EvidenceMagic);
    writer.WriteU32(evidence.schemaVersion);
    writer.WriteU64(evidence.captureUnixNanoseconds);
    writer.WriteU32(evidence.config.warmupCyclesPerOrder);
    writer.WriteU32(evidence.config.measurementCyclesPerOrder);
    writer.WriteU32(evidence.config.runCount);
    writer.WriteU32(evidence.config.adapterIndex);
    writer.WriteU32(evidence.config.frameIndex);
    WriteString(writer, evidence.config.clockPolicy);
    WriteString(writer, evidence.adapter.description);
    writer.WriteU32(evidence.adapter.vendorId);
    writer.WriteU32(evidence.adapter.deviceId);
    writer.WriteU32(evidence.adapter.subsystemId);
    writer.WriteU32(evidence.adapter.revision);
    writer.WriteU32(evidence.adapter.luidLow);
    writer.WriteI32(evidence.adapter.luidHigh);
    writer.WriteU64(evidence.adapter.driverVersion);
    writer.WriteU64(evidence.adapter.timestampFrequency);
    writer.WriteU32(evidence.adapter.uma ? 1u : 0u);
    writer.WriteU32(evidence.adapter.cacheCoherentUma ? 1u : 0u);
    WriteDigest(writer, evidence.adapter.fingerprint);
    WriteDigest(writer, evidence.targetProfileIdentity);
    WriteDigest(writer, evidence.observationContractIdentity);
    WriteDigest(writer, evidence.measurementProfileIdentity);
    writer.WriteU32(static_cast<std::uint32_t>(evidence.cases.size()));
    for (const auto& item : evidence.cases)
    {
        WriteString(writer, item.id);
        writer.WriteU32(item.boneCount);
        writer.WriteU32(item.hierarchyDepthCount);
        writer.WriteU32(item.reuseCount);
        writer.WriteU32(item.pointCount);
        WriteDigest(writer, item.hierarchyIdentity);
        WriteDigest(writer, item.reuseSemanticIdentity);
        WriteDigest(writer, item.localPointCorpusIdentity);
        WriteDigest(writer, item.invocationIdentity);
        WriteDigest(writer, item.scenarioIdentity);
        writer.WriteU32(static_cast<std::uint32_t>(item.preparation.size()));
        for (const auto& preparation : item.preparation) WritePreparation(writer, preparation);
        writer.WriteU32(static_cast<std::uint32_t>(item.samples.size()));
        for (const auto& sample : item.samples)
        {
            writer.WriteU32(sample.run);
            writer.WriteU32(sample.cycle);
            WriteString(writer, sample.candidateOrder);
            writer.WriteU32(static_cast<std::uint32_t>(sample.leaves.size()));
            for (const auto& leafTiming : sample.leaves) WriteLeaf(writer, leafTiming);
            writer.WriteU32(static_cast<std::uint32_t>(sample.candidates.size()));
            for (const auto& candidateTiming : sample.candidates) WriteCandidateSample(writer, candidateTiming);
        }
    }
    return std::move(writer).Take();
}

base::Result<MeasurementEvidenceV1,std::string> RunRealGpuMeasurementV1(const MeasurementConfigV1& config)
{
#if !defined(_WIN32)
    (void)config;
    return base::Result<MeasurementEvidenceV1,std::string>::Failure(
        "Spiral 3 real-GPU measurement is available only on Windows");
#else
    try
    {
        Require(config.warmupCyclesPerOrder > 0 && config.measurementCyclesPerOrder > 0 && config.runCount > 0,
                "measurement counts must be positive");
        Require(config.adapterIndex == 0,
                "Spiral 3 canonical backend measurement currently selects hardware adapter index 0");

        auto hierarchyResult = corpus::BuildCanonicalMeasurementHierarchyV1();
        if (!hierarchyResult) throw std::runtime_error(hierarchyResult.Error());
        auto hierarchy = std::move(hierarchyResult.Value());
        auto reuseCases = corpus::BuildReuseCorpusV1(hierarchy);
        if (!reuseCases) throw std::runtime_error(reuseCases.Error());
        auto frames = spiral2::corpus::BuildFrameCorpusV1(hierarchy);
        if (!frames) throw std::runtime_error(frames.Error());
        Require(reuseCases.Value().size() == CanonicalReuseCaseCountV1, "canonical R corpus does not contain six cases");
        Require(config.frameIndex < frames.Value().size(), "requested measurement frame is unavailable");
        const auto& palette = frames.Value()[config.frameIndex].palette;
        const auto paletteBytes = spiral2::contracts::SerializeDynamicLocalMotorPaletteV1(palette);

        MeasurementEvidenceV1 evidence;
        evidence.config = config;
        evidence.captureUnixNanoseconds = static_cast<std::uint64_t>(
            std::chrono::duration_cast<std::chrono::nanoseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count());
        evidence.adapter = CaptureAdapterProfile(config.adapterIndex);
        evidence.observationContractIdentity = reuseCases.Value()[0].semantic.observationContractIdentity;

        d3d12::D3D12Backend backend({false,false,true});
        base::BinaryWriter profileWriter;
        WriteString(profileWriter, MeasurementProfileDomain);
        WriteDigest(profileWriter, evidence.adapter.fingerprint);
        WriteDigest(profileWriter, evidence.observationContractIdentity);
        profileWriter.WriteU32(config.warmupCyclesPerOrder);
        profileWriter.WriteU32(config.measurementCyclesPerOrder);
        profileWriter.WriteU32(config.runCount);
        profileWriter.WriteU32(config.adapterIndex);
        profileWriter.WriteU32(config.frameIndex);
        WriteString(profileWriter, config.clockPolicy);

        const std::array kinds{
            candidate::CandidateKindV1::MatrixHierarchy,
            candidate::CandidateKindV1::DirectPgaHierarchy,
            candidate::CandidateKindV1::HybridHierarchy};

        for (const auto& reuseCase : reuseCases.Value())
        {
            Require(reuseCase.semantic.observationContractIdentity == evidence.observationContractIdentity,
                    "R cases disagree on Observation Contract identity");
            MeasurementCaseV1 measured;
            measured.id = reuseCase.id;
            measured.boneCount = hierarchy.boneCount;
            measured.hierarchyDepthCount = static_cast<std::uint32_t>(hierarchy.depthOffsets.size() - 1);
            measured.reuseCount = reuseCase.reuseCount;
            measured.pointCount = reuseCase.semantic.pointCount;
            measured.hierarchyIdentity = spiral2::hierarchy::RigidHierarchySemanticIdentityV1(hierarchy);
            measured.reuseSemanticIdentity = semantic::ReuseSweepSemanticIdentityV1(reuseCase.semantic);
            measured.localPointCorpusIdentity = reuseCase.semantic.localPointCorpusIdentity;
            measured.invocationIdentity = base::Sha256(paletteBytes);

            for (const auto kind : kinds)
                measured.preparation.push_back(PrepareCandidate(hierarchy,reuseCase,kind).evidence);

            auto comparison = scenarios::BuildFrozenReuseComparisonScenarioV1(hierarchy,reuseCase);
            if (!comparison) throw std::runtime_error(comparison.Error().stage + ": " + comparison.Error().message);
            measured.scenarioIdentity = comparison.Value().scenarioIdentity;
            auto reference = corpus::BuildIndependentCpuReferenceV1(
                hierarchy,palette,reuseCase.localPoints,contracts::ReuseCountV1{reuseCase.reuseCount});
            if (!reference) throw std::runtime_error(reference.Error());

            std::vector<CandidateRuntime> runtimeStorage;
            runtimeStorage.reserve(3);
            for (const auto kind : kinds)
            {
                auto scenario = BuildCandidateScenario(comparison.Value(),kind);
                auto preparation = std::find_if(measured.preparation.begin(), measured.preparation.end(),
                    [&](const auto& value){return value.kind == kind;});
                Require(preparation != measured.preparation.end(), "candidate preparation record is missing");
                preparation->compositionFreezeNanoseconds = scenario.compositionFreezeNanoseconds;
                preparation->sourcePackageBytes = scenario.leaves[0].packageBytes.size() + scenario.leaves[1].packageBytes.size();
                preparation->frozenCompositionBytes = scenario.frozenCompositionBytes.size();
                preparation->compositionLeafCount = static_cast<std::uint32_t>(scenario.leaves.size());
                preparation->candidateCompositionDigest = scenario.frozenCompositionDigest;
                std::uint64_t extractedCandidateBytes = 0;
                for (std::size_t index = 2; index < scenario.leaves.size(); ++index)
                {
                    extractedCandidateBytes += scenario.leaves[index].packageBytes.size();
                    Require(PackageTargetProfileIdentity(scenario.leaves[index]) == preparation->targetProfileIdentity,
                            "candidate-specific Composition target profile differs from verified candidate group");
                }
                Require(extractedCandidateBytes == preparation->candidatePackageBytes,
                        "candidate-specific Composition Package bytes differ from preparation evidence");
                for (std::size_t index = 0; index < 2; ++index)
                    Require(PackageTargetProfileIdentity(scenario.leaves[index]) == preparation->targetProfileIdentity,
                            "source Package target profile differs from candidate target profile");
                if (evidence.targetProfileIdentity == base::Digest256{})
                    evidence.targetProfileIdentity = preparation->targetProfileIdentity;
                else
                    Require(evidence.targetProfileIdentity == preparation->targetProfileIdentity,
                            "R/candidate cases disagree on Target profile identity");

                const auto materializationBegin = Clock::now();
                auto loaded = canrt::LoadStaticComposition(scenario.frozenCompositionBytes,backend);
                const auto materializationEnd = Clock::now();
                if (!loaded) throw std::runtime_error(loaded.Error().stage + ": " + loaded.Error().message);
                preparation->materializationNanoseconds = Nanoseconds(materializationBegin,materializationEnd);
                runtimeStorage.emplace_back(std::move(scenario),std::move(loaded.Value()));
            }
            std::array<CandidateRuntime*,3> runtimes{
                &runtimeStorage[0],&runtimeStorage[1],&runtimeStorage[2]};

            for (std::uint32_t cycle = 0; cycle < config.warmupCyclesPerOrder; ++cycle)
                for (const auto& order : CandidateOrders)
                    (void)ExecuteSample(0,cycle,order,hierarchy,reuseCase,palette,reference.Value(),runtimes,backend);

            for (std::uint32_t run = 0; run < config.runCount; ++run)
                for (std::uint32_t cycle = 0; cycle < config.measurementCyclesPerOrder; ++cycle)
                    for (std::size_t offset = 0; offset < CandidateOrders.size(); ++offset)
                    {
                        const auto& order = CandidateOrders[(offset + run) % CandidateOrders.size()];
                        measured.samples.push_back(ExecuteSample(
                            run,cycle,order,hierarchy,reuseCase,palette,reference.Value(),runtimes,backend));
                    }

            WriteString(profileWriter, measured.id);
            WriteDigest(profileWriter, measured.hierarchyIdentity);
            WriteDigest(profileWriter, measured.reuseSemanticIdentity);
            WriteDigest(profileWriter, measured.localPointCorpusIdentity);
            WriteDigest(profileWriter, measured.invocationIdentity);
            WriteDigest(profileWriter, measured.scenarioIdentity);
            for (const auto& preparation : measured.preparation)
            {
                WriteDigest(profileWriter, preparation.verifiedCertificate);
                WriteDigest(profileWriter, preparation.verifiedPlanIdentity);
                WriteDigest(profileWriter, preparation.candidateCompositionDigest);
            }
            std::cout << "[MEASURED] " << measured.id
                      << " reuse=" << measured.reuseCount
                      << " points=" << measured.pointCount
                      << " samples=" << measured.samples.size() << '\n';
            evidence.cases.push_back(std::move(measured));
        }

        WriteDigest(profileWriter,evidence.targetProfileIdentity);
        for (const auto& order : CandidateOrders) WriteString(profileWriter,OrderName(order));
        evidence.measurementProfileIdentity = base::Sha256(profileWriter.Bytes());
        return base::Result<MeasurementEvidenceV1,std::string>::Success(std::move(evidence));
    }
    catch (const std::exception& error)
    {
        return base::Result<MeasurementEvidenceV1,std::string>::Failure(error.what());
    }
#endif
}

base::Result<void,std::string> WriteMeasurementArtifactsV1(
    const MeasurementEvidenceV1& evidence,
    const std::filesystem::path& outputDirectory)
{
    try
    {
        std::filesystem::create_directories(outputDirectory);
        const auto binary = SerializeMeasurementEvidenceV1(evidence);
        auto written = base::WriteAllBytes(outputDirectory / "spiral3_measurement_evidence_v1.bin", binary);
        if (!written) return base::Result<void,std::string>::Failure(written.Error());
        WriteCsv(evidence,outputDirectory / "measurement_samples.csv");
        std::ofstream profile(outputDirectory / "measurement_profile.json",std::ios::binary);
        if (!profile) throw std::runtime_error("failed to create measurement_profile.json");
        profile << "{\n"
                << "  \"schema\": \"SGE4-5.Spiral3.MeasurementProfile.V1\",\n"
                << "  \"adapter\": \"" << JsonEscape(evidence.adapter.description) << "\",\n"
                << "  \"adapterFingerprint\": \"" << Hex(evidence.adapter.fingerprint) << "\",\n"
                << "  \"driverVersion\": " << evidence.adapter.driverVersion << ",\n"
                << "  \"targetProfileIdentity\": \"" << Hex(evidence.targetProfileIdentity) << "\",\n"
                << "  \"observationContractIdentity\": \"" << Hex(evidence.observationContractIdentity) << "\",\n"
                << "  \"queue\": \"D3D12 direct; one verifier-sealed candidate Composition completes before the next\",\n"
                << "  \"timestampFrequency\": " << evidence.adapter.timestampFrequency << ",\n"
                << "  \"clockPolicy\": \"" << JsonEscape(evidence.config.clockPolicy) << "\",\n"
                << "  \"frameIndex\": " << evidence.config.frameIndex << ",\n"
                << "  \"reuseCounts\": [1,4,16,64,256,512],\n"
                << "  \"orders\": [\"ABC\",\"ACB\",\"BAC\",\"BCA\",\"CAB\",\"CBA\"],\n"
                << "  \"warmupCyclesPerOrder\": " << evidence.config.warmupCyclesPerOrder << ",\n"
                << "  \"measurementCyclesPerOrder\": " << evidence.config.measurementCyclesPerOrder << ",\n"
                << "  \"runs\": " << evidence.config.runCount << ",\n"
                << "  \"profileIdentity\": \"" << Hex(evidence.measurementProfileIdentity) << "\"\n"
                << "}\n";
        return base::Result<void,std::string>::Success();
    }
    catch (const std::exception& error)
    {
        return base::Result<void,std::string>::Failure(error.what());
    }
}

base::Result<MeasurementEvidenceV1,std::string> ReadMeasurementEvidenceV1(
    const std::filesystem::path& evidencePath)
{
    auto bytes = base::ReadAllBytes(evidencePath);
    if (!bytes) return base::Result<MeasurementEvidenceV1,std::string>::Failure(bytes.Error());
    return DeserializeEvidence(bytes.Value());
}

base::Result<CrossoverAnalysisV1,std::string> AnalyzeCrossoverV1(const MeasurementEvidenceV1& evidence)
{
    try
    {
        ValidateEvidenceForDecision(evidence);
        CrossoverAnalysisV1 result;
        result.winners.reserve(evidence.cases.size());
        for (const auto& item : evidence.cases) result.winners.push_back(Winner(item));
        for (std::size_t index = 1; index < evidence.cases.size(); ++index)
        {
            if (result.winners[index] != result.winners[index - 1])
                result.transitions.push_back({
                    evidence.cases[index - 1].reuseCount,
                    evidence.cases[index].reuseCount,
                    result.winners[index - 1],
                    result.winners[index]});
        }
        if (result.transitions.empty())
            result.classification = CrossoverClassificationV1::NoObservedWinnerTransition;
        else if (result.transitions.size() == 1)
            result.classification = CrossoverClassificationV1::SingleStableWinnerTransition;
        else
            result.classification = CrossoverClassificationV1::MultipleWinnerTransitions;
        return base::Result<CrossoverAnalysisV1,std::string>::Success(std::move(result));
    }
    catch (const std::exception& error)
    {
        return base::Result<CrossoverAnalysisV1,std::string>::Failure(error.what());
    }
}

base::Result<void,std::string> WriteDecisionEvidenceReportV1(
    const MeasurementEvidenceV1& evidence,
    const std::filesystem::path& outputDirectory)
{
    try
    {
        ValidateEvidenceForDecision(evidence);
        auto analysisResult = AnalyzeCrossoverV1(evidence);
        if (!analysisResult) return base::Result<void,std::string>::Failure(analysisResult.Error());
        const auto& analysis = analysisResult.Value();
        std::filesystem::create_directories(outputDirectory);
        const std::array kinds{
            candidate::CandidateKindV1::MatrixHierarchy,
            candidate::CandidateKindV1::DirectPgaHierarchy,
            candidate::CandidateKindV1::HybridHierarchy};
        std::array<std::pair<candidate::CandidateKindV1,double>,3> ranking{};
        for (std::size_t index = 0; index < kinds.size(); ++index)
            ranking[index] = {kinds[index],Median(AllGpuValues(evidence,kinds[index]))};
        std::sort(ranking.begin(),ranking.end(),[](const auto& left,const auto& right){return left.second < right.second;});
        const auto ab = PairwiseIntervals(evidence,kinds[0],kinds[1]);
        const auto ac = PairwiseIntervals(evidence,kinds[0],kinds[2]);
        const auto bc = PairwiseIntervals(evidence,kinds[1],kinds[2]);
        std::size_t totalSamples = 0;
        for (const auto& item : evidence.cases) totalSamples += item.samples.size();

        std::ofstream report(outputDirectory / "SPIRAL3_DECISION_EVIDENCE_REPORT.md",std::ios::binary);
        if (!report) throw std::runtime_error("failed to create Spiral 3 Decision Evidence report");
        report << std::fixed << std::setprecision(3);
        report << "# SGE4-5 Spiral 3 Decision Evidence Report V1\n\n"
               << "HardwareEvidence = Qualified\n\n"
               << "MeasurementSamples = " << totalSamples << "\n\n"
               << "BalancedOrders = ABC/ACB/BAC/BCA/CAB/CBA\n\n"
               << "CrossoverClassification = " << ClassificationName(analysis.classification) << "\n\n";
        if (analysis.transitions.empty())
            report << "ObservedTransition = None\n\n";
        else
            for (const auto& transition : analysis.transitions)
                report << "ObservedTransition = R" << transition.lowerReuseCount << "-R" << transition.upperReuseCount
                       << ':' << KindLetter(transition.from) << "->" << KindLetter(transition.to) << "\n";
        report << "\n"
               << "- Adapter: " << evidence.adapter.description << "\n"
               << "- Adapter fingerprint: `" << Hex(evidence.adapter.fingerprint) << "`\n"
               << "- Driver version: `" << evidence.adapter.driverVersion << "`\n"
               << "- Timestamp frequency: `" << evidence.adapter.timestampFrequency << "`\n"
               << "- Target profile identity: `" << Hex(evidence.targetProfileIdentity) << "`\n"
               << "- Observation Contract identity: `" << Hex(evidence.observationContractIdentity) << "`\n"
               << "- Measurement profile identity: `" << Hex(evidence.measurementProfileIdentity) << "`\n"
               << "- Clock policy: `" << evidence.config.clockPolicy << "`\n"
               << "- Fixed frame: `F" << std::setw(2) << std::setfill('0') << evidence.config.frameIndex << std::setfill(' ') << "`\n"
               << "- Queue contract: each verifier-sealed candidate Composition completes before the next order position.\n"
               << "- Samples per R case: " << evidence.cases.front().samples.size() << "\n\n";

        report << "## Result\n\n";
        if (analysis.transitions.empty())
            report << "No winner transition was observed across the sampled R set. This is a valid negative crossover result; it is not a universal representation rule.\n\n";
        else
        {
            for (const auto& transition : analysis.transitions)
                report << "Observed winner transition interval: `R" << transition.lowerReuseCount
                       << " -> R" << transition.upperReuseCount << "`, `"
                       << KindLetter(transition.from) << " -> " << KindLetter(transition.to) << "`.\n\n";
        }
        report << "A transition interval is bounded only by adjacent sampled reuse counts. No interpolation, Runtime policy, or hardware-independent theorem is authorized.\n\n";

        report << "## Per-R materialization comparison\n\n"
               << "| R | Points | A GPU median ns | B GPU median ns | C GPU median ns | Lowest | A/B/C candidate package B | A/B/C intermediate B | A/B/C dispatches | Max abs error |\n"
               << "|---:|---:|---:|---:|---:|---|---|---|---|---:|\n";
        for (std::size_t caseIndex = 0; caseIndex < evidence.cases.size(); ++caseIndex)
        {
            const auto& item = evidence.cases[caseIndex];
            auto prep = [&](candidate::CandidateKindV1 kind)->const CandidatePreparationV1&
            {
                auto found = std::find_if(item.preparation.begin(),item.preparation.end(),[&](const auto& value){return value.kind == kind;});
                if (found == item.preparation.end()) throw std::runtime_error("preparation record missing while writing report");
                return *found;
            };
            double maxError = 0.0;
            for (const auto& sample : item.samples)
                for (const auto& timing : sample.candidates)
                    maxError = std::max(maxError,timing.numericMaxAbsoluteError);
            report << '|' << item.reuseCount << '|' << item.pointCount << '|'
                   << Median(GpuValues(item,kinds[0])) << '|'
                   << Median(GpuValues(item,kinds[1])) << '|'
                   << Median(GpuValues(item,kinds[2])) << '|'
                   << KindLetter(analysis.winners[caseIndex]) << '|'
                   << prep(kinds[0]).candidatePackageBytes << '/'
                   << prep(kinds[1]).candidatePackageBytes << '/'
                   << prep(kinds[2]).candidatePackageBytes << '|'
                   << prep(kinds[0]).logicalIntermediateBytes << '/'
                   << prep(kinds[1]).logicalIntermediateBytes << '/'
                   << prep(kinds[2]).logicalIntermediateBytes << '|'
                   << prep(kinds[0]).plannedDispatchCount << '/'
                   << prep(kinds[1]).plannedDispatchCount << '/'
                   << prep(kinds[2]).plannedDispatchCount << '|'
                   << maxError << "|\n";
        }

        auto writeIntervals = [&](std::string_view name,const auto& intervals)
        {
            report << "- " << name << ": ";
            if (intervals.empty()) report << "none observed";
            else for (std::size_t index = 0; index < intervals.size(); ++index)
            {
                if (index) report << ", ";
                report << "R" << intervals[index].first << "-R" << intervals[index].second;
            }
            report << "\n";
        };
        report << "\n## Pairwise sign-change intervals\n\n";
        writeIntervals("A versus B",ab);
        writeIntervals("A versus C",ac);
        writeIntervals("B versus C",bc);

        report << "\n## Full metric ledger\n\n"
               << "| R | Candidate | Generation ns | Verifier ns | Leaf freeze ns | Composition freeze ns | Materialization ns | Candidate package B | Source package B | Frozen Composition B | Dynamic B | Point source B | Intermediate B | Command median ns | GPU median ns | End-to-end median ns | Dispatches | Barriers | Max abs | Max rel | Max ULP |\n"
               << "|---:|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|\n";
        for (const auto& item : evidence.cases)
            for (const auto kind : kinds)
            {
                const auto preparation = std::find_if(item.preparation.begin(),item.preparation.end(),[&](const auto& value){return value.kind == kind;});
                if (preparation == item.preparation.end()) throw std::runtime_error("preparation record missing");
                std::vector<double> command,gpu,endToEnd;
                double maxAbsolute = 0.0,maxRelative = 0.0;
                std::uint64_t maxUlp = 0;
                std::uint32_t barriers = 0;
                for (const auto& sample : item.samples)
                    for (const auto& timing : sample.candidates)
                        if (timing.kind == kind)
                        {
                            command.push_back(timing.commandRecordingNanoseconds);
                            gpu.push_back(timing.gpuTotalNanoseconds);
                            endToEnd.push_back(timing.endToEndNanoseconds);
                            maxAbsolute = std::max(maxAbsolute,timing.numericMaxAbsoluteError);
                            maxRelative = std::max(maxRelative,timing.numericMaxRelativeError);
                            maxUlp = std::max(maxUlp,timing.numericMaxUlpDistance);
                            barriers = timing.barrierCount;
                        }
                report << '|' << item.reuseCount << '|' << KindLetter(kind) << '|'
                       << preparation->generationNanoseconds << '|'
                       << preparation->verifierNanoseconds << '|'
                       << preparation->leafFreezeNanoseconds << '|'
                       << preparation->compositionFreezeNanoseconds << '|'
                       << preparation->materializationNanoseconds << '|'
                       << preparation->candidatePackageBytes << '|'
                       << preparation->sourcePackageBytes << '|'
                       << preparation->frozenCompositionBytes << '|'
                       << preparation->dynamicInvocationBytes << '|'
                       << preparation->pointSourceBytes << '|'
                       << preparation->logicalIntermediateBytes << '|'
                       << Median(command) << '|' << Median(gpu) << '|' << Median(endToEnd) << '|'
                       << preparation->plannedDispatchCount << '/' << preparation->packageDispatchCount << '|'
                       << barriers << '|' << maxAbsolute << '|' << maxRelative << '|' << maxUlp << "|\n";
            }

        report << "\n## Aggregate ranking\n\n";
        for (std::size_t index = 0; index < ranking.size(); ++index)
            report << (index + 1) << ". " << KindName(ranking[index].first)
                   << ": aggregate sample median " << ranking[index].second << " ns\n";

        report << "\n`measurement_samples.csv` preserves every candidate order position, per-Leaf timestamp, command-recording time, GPU time, dispatch/barrier count, end-to-end time, and numeric diagnostic.\n\n"
               << "RecommendationAuthority = NonAuthoritative\n\n"
               << "RuntimePolicyAuthorization = None\n\n"
               << "The observed winner and transition intervals are adapter/configuration evidence only. They do not alter Frozen Packages, authorize Runtime selection, or establish a global PGA-versus-Matrix rule.\n\n"
               << "## Owner decision boundary\n\n"
               << "NextCapabilitySelection = DeferredByOwner\n\n"
               << "SelectionStatus = OWNER_DECISION_REQUIRED\n";

        std::cout << "[RANKING] 1=" << KindName(ranking[0].first) << " (" << ranking[0].second
                  << " ns), 2=" << KindName(ranking[1].first) << " (" << ranking[1].second
                  << " ns), 3=" << KindName(ranking[2].first) << " (" << ranking[2].second << " ns)\n";
        std::cout << "[CROSSOVER] " << ClassificationName(analysis.classification);
        for (const auto& transition : analysis.transitions)
            std::cout << " R" << transition.lowerReuseCount << "-R" << transition.upperReuseCount
                      << ':' << KindLetter(transition.from) << "->" << KindLetter(transition.to);
        std::cout << '\n';
        return base::Result<void,std::string>::Success();
    }
    catch (const std::exception& error)
    {
        return base::Result<void,std::string>::Failure(error.what());
    }
}

base::Result<void,std::string> RunFormatSelfTestV1(const std::filesystem::path& outputDirectory)
{
    try
    {
        MeasurementEvidenceV1 evidence;
        evidence.captureUnixNanoseconds = 1;
        evidence.config = {1,1,1,0,CanonicalFrameIndexV1,"self-test"};
        evidence.adapter.description = "Synthetic hardware adapter";
        evidence.adapter.vendorId = 1;
        evidence.adapter.timestampFrequency = 1'000'000;
        constexpr std::string_view synthetic = "SGE4-5.Spiral3.SyntheticHardwareEvidence.V1";
        evidence.adapter.fingerprint = base::Sha256(std::as_bytes(std::span(synthetic.data(),synthetic.size())));
        evidence.targetProfileIdentity = evidence.adapter.fingerprint;
        evidence.observationContractIdentity = evidence.adapter.fingerprint;
        evidence.measurementProfileIdentity = evidence.adapter.fingerprint;
        const std::array reuseCounts{1u,4u,16u,64u,256u,512u};
        const std::array kinds{
            candidate::CandidateKindV1::MatrixHierarchy,
            candidate::CandidateKindV1::DirectPgaHierarchy,
            candidate::CandidateKindV1::HybridHierarchy};
        for (std::size_t caseIndex = 0; caseIndex < reuseCounts.size(); ++caseIndex)
        {
            MeasurementCaseV1 item;
            item.id = "R" + std::to_string(reuseCounts[caseIndex]);
            item.boneCount = 8;
            item.hierarchyDepthCount = 8;
            item.reuseCount = reuseCounts[caseIndex];
            item.pointCount = item.boneCount * item.reuseCount;
            item.hierarchyIdentity = evidence.adapter.fingerprint;
            item.reuseSemanticIdentity = evidence.adapter.fingerprint;
            item.localPointCorpusIdentity = evidence.adapter.fingerprint;
            item.invocationIdentity = evidence.adapter.fingerprint;
            item.scenarioIdentity = evidence.adapter.fingerprint;
            for (const auto kind : kinds)
            {
                CandidatePreparationV1 preparation;
                preparation.kind = kind;
                preparation.generationNanoseconds = 100 + static_cast<std::uint32_t>(kind);
                preparation.verifierNanoseconds = 200 + static_cast<std::uint32_t>(kind);
                preparation.leafFreezeNanoseconds = 300 + static_cast<std::uint32_t>(kind);
                preparation.compositionFreezeNanoseconds = 400 + static_cast<std::uint32_t>(kind);
                preparation.materializationNanoseconds = 500 + static_cast<std::uint32_t>(kind);
                preparation.candidatePackageBytes = 1000 + static_cast<std::uint32_t>(kind);
                preparation.sourcePackageBytes = 700;
                preparation.frozenCompositionBytes = 2000 + static_cast<std::uint32_t>(kind);
                preparation.dynamicInvocationBytes = 8u * 32u + item.pointCount * 16u;
                preparation.pointSourceBytes = item.pointCount * 16u;
                preparation.logicalIntermediateBytes = static_cast<std::uint64_t>(kind == candidate::CandidateKindV1::DirectPgaHierarchy ? 256u : kind == candidate::CandidateKindV1::MatrixHierarchy ? 768u : 640u);
                preparation.plannedDispatchCount = kind == candidate::CandidateKindV1::DirectPgaHierarchy ? 2u : 3u;
                preparation.packageDispatchCount = preparation.plannedDispatchCount;
                preparation.candidateLeafCount = preparation.plannedDispatchCount;
                preparation.compositionLeafCount = preparation.candidateLeafCount + 2;
                preparation.verifiedCertificate = evidence.adapter.fingerprint;
                preparation.verifiedPlanIdentity = evidence.adapter.fingerprint;
                preparation.targetProfileIdentity = evidence.targetProfileIdentity;
                preparation.candidateCompositionDigest = evidence.adapter.fingerprint;
                item.preparation.push_back(preparation);
            }
            for (const auto& order : CandidateOrders)
            {
                MeasurementSampleV1 sample;
                sample.candidateOrder = OrderName(order);
                for (std::uint32_t position = 0; position < 3; ++position)
                {
                    CandidateTimingSampleV1 timing;
                    timing.kind = order[position];
                    timing.orderPosition = position;
                    const bool lowReuse = caseIndex < 2;
                    if (timing.kind == candidate::CandidateKindV1::MatrixHierarchy) timing.gpuTotalNanoseconds = 130.0 + caseIndex;
                    if (timing.kind == candidate::CandidateKindV1::DirectPgaHierarchy) timing.gpuTotalNanoseconds = lowReuse ? 80.0 + caseIndex : 150.0 + caseIndex;
                    if (timing.kind == candidate::CandidateKindV1::HybridHierarchy) timing.gpuTotalNanoseconds = lowReuse ? 120.0 + caseIndex : 70.0 + caseIndex;
                    timing.commandRecordingNanoseconds = timing.gpuTotalNanoseconds * 0.25;
                    timing.endToEndNanoseconds = timing.gpuTotalNanoseconds * 2.0;
                    timing.dispatchCount = timing.kind == candidate::CandidateKindV1::DirectPgaHierarchy ? 2u : 3u;
                    sample.candidates.push_back(timing);
                    LeafTimingSampleV1 leafTiming;
                    leafTiming.role = std::string(1,KindLetter(timing.kind)) + "0.Synthetic";
                    leafTiming.kind = timing.kind;
                    leafTiming.commandRecordingNanoseconds = timing.commandRecordingNanoseconds;
                    leafTiming.gpuNanoseconds = timing.gpuTotalNanoseconds;
                    leafTiming.dispatchCount = timing.dispatchCount;
                    sample.leaves.push_back(leafTiming);
                }
                item.samples.push_back(std::move(sample));
            }
            evidence.cases.push_back(std::move(item));
        }

        auto written = WriteMeasurementArtifactsV1(evidence,outputDirectory);
        if (!written) return written;
        auto read = ReadMeasurementEvidenceV1(outputDirectory / "spiral3_measurement_evidence_v1.bin");
        if (!read) return base::Result<void,std::string>::Failure(read.Error());
        if (SerializeMeasurementEvidenceV1(read.Value()) != SerializeMeasurementEvidenceV1(evidence))
            return base::Result<void,std::string>::Failure("measurement evidence roundtrip changed bytes");
        auto analysis = AnalyzeCrossoverV1(read.Value());
        if (!analysis) return base::Result<void,std::string>::Failure(analysis.Error());
        if (analysis.Value().classification != CrossoverClassificationV1::SingleStableWinnerTransition ||
            analysis.Value().transitions.size() != 1 ||
            analysis.Value().transitions[0].lowerReuseCount != 4 ||
            analysis.Value().transitions[0].upperReuseCount != 16 ||
            analysis.Value().transitions[0].from != candidate::CandidateKindV1::DirectPgaHierarchy ||
            analysis.Value().transitions[0].to != candidate::CandidateKindV1::HybridHierarchy)
            return base::Result<void,std::string>::Failure("synthetic crossover analysis did not preserve the R4-R16 B-to-C transition");
        auto report = WriteDecisionEvidenceReportV1(read.Value(),outputDirectory);
        if (!report) return report;

        auto corrupt = SerializeMeasurementEvidenceV1(evidence);
        corrupt.push_back(std::byte{0});
        auto rejected = DeserializeEvidence(corrupt);
        if (rejected) return base::Result<void,std::string>::Failure("trailing-byte corruption was accepted");
        return base::Result<void,std::string>::Success();
    }
    catch (const std::exception& error)
    {
        return base::Result<void,std::string>::Failure(error.what());
    }
}
}
