#ifndef NOMINMAX
#define NOMINMAX
#endif

#include "Spiral7DeltaFamilyRuntime.h"

#include "../00_Foundation/BinaryIO.h"

#if defined(_WIN32)
#include <Windows.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl/client.h>
#endif

#include <algorithm>
#include <array>
#include <cstring>
#include <optional>
#include <span>
#include <stdexcept>
#include <string_view>
#include <utility>

#if defined(_WIN32)
#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "dxguid.lib")
#endif

namespace sge4_5::spiral7::architecture_runtime
{
namespace fc = family_candidate;
namespace fv = family_verification;
namespace fe = family_execution;
namespace
{
DeltaFamilyRuntimeErrorV1 Error(std::string stage, std::string message, long nativeCode = 0)
{
    return {std::move(stage), std::move(message), nativeCode};
}

base::Digest256 Literal(std::string_view value)
{
    return base::Sha256(std::as_bytes(std::span(value.data(), value.size())));
}

void Digest(base::BinaryWriter& writer, const base::Digest256& value)
{
    writer.WriteBytes(value);
}

void WriteString(base::BinaryWriter& writer, std::string_view value)
{
    writer.WriteU32(static_cast<std::uint32_t>(value.size()));
    writer.WriteBytes(std::as_bytes(std::span(value.data(), value.size())));
}

constexpr std::array<fc::DeltaFamilyCandidateKindV1, 3> CandidateOrder{
    fc::DeltaFamilyCandidateKindV1::FullActiveDenseRecompute,
    fc::DeltaFamilyCandidateKindV1::CompactDeltaIndexHistoryReuse,
    fc::DeltaFamilyCandidateKindV1::AffectedBlockDeltaHistoryReuse};

base::Digest256 ArtifactSetIdentity(const fe::VerifiedDeltaFamilyCaseV1& value)
{
    base::BinaryWriter writer;
    Digest(writer, Literal("SGE4-5.Spiral7.CU5.ArtifactSet.V1"));
    Digest(writer, value.invocation.Identity());
    writer.WriteU32(static_cast<std::uint32_t>(value.artifacts.size()));
    for (std::size_t index = 0; index < value.artifacts.size(); ++index)
    {
        Digest(writer, value.artifacts[index].ArtifactIdentity());
        Digest(writer, value.verifiedCandidates[index].CertificateIdentity());
    }
    return base::Sha256(writer.Bytes());
}

base::Result<void, DeltaFamilyRuntimeErrorV1> ValidateAuthorityCase(
    const semantic::SparseTemporalDeltaSemanticV1& semanticValue,
    const fv::DeltaFamilyVerificationContextV1& context,
    const fe::VerifiedDeltaFamilyCaseV1& value,
    std::uint32_t expectedIndex)
{
    if (value.invocation.InvocationIndex() != expectedIndex)
        return base::Result<void, DeltaFamilyRuntimeErrorV1>::Failure(
            Error("delta-runtime/authority-index", "Authority invocation indices are not contiguous."));
    if (value.artifacts.size() != CandidateOrder.size() ||
        value.verifiedCandidates.size() != CandidateOrder.size())
        return base::Result<void, DeltaFamilyRuntimeErrorV1>::Failure(
            Error("delta-runtime/authority-family", "Authority invocation does not contain exactly A/B/C."));

    for (std::size_t index = 0; index < CandidateOrder.size(); ++index)
    {
        if (value.artifacts[index].Kind() != CandidateOrder[index] ||
            value.verifiedCandidates[index].Plan().operation.kind != CandidateOrder[index])
            return base::Result<void, DeltaFamilyRuntimeErrorV1>::Failure(
                Error("delta-runtime/authority-order", "Authority Candidate order is not canonical A/B/C."));
        auto artifactValid = fc::ValidateDeltaFamilyArtifactV1(
            value.artifacts[index], semanticValue, value.invocation);
        if (!artifactValid)
            return base::Result<void, DeltaFamilyRuntimeErrorV1>::Failure(
                Error("delta-runtime/authority-artifact", artifactValid.Error()));
        auto verifiedValid = fv::ValidateVerifiedDeltaFamilyCandidateV1(
            value.verifiedCandidates[index], semanticValue, value.invocation, context);
        if (!verifiedValid)
            return base::Result<void, DeltaFamilyRuntimeErrorV1>::Failure(
                Error("delta-runtime/authority-verified", verifiedValid.Error()));
        if (value.verifiedCandidates[index].Plan().operation.artifactIdentity !=
            value.artifacts[index].ArtifactIdentity())
            return base::Result<void, DeltaFamilyRuntimeErrorV1>::Failure(
                Error("delta-runtime/authority-binding", "Verified operation and Frozen artifact differ."));
    }
    return base::Result<void, DeltaFamilyRuntimeErrorV1>::Success();
}

std::vector<std::byte> SerializeAuthority(
    const semantic::SparseTemporalDeltaSemanticV1& semanticValue,
    const fv::DeltaFamilyVerificationContextV1& context,
    std::span<const fe::VerifiedDeltaFamilyCaseV1> cases)
{
    base::BinaryWriter writer;
    Digest(writer, Literal("SGE4-5.Spiral7.CU5.FrozenTimelineAuthority.V1"));
    Digest(writer, semantic::SparseTemporalDeltaSemanticIdentityV1(semanticValue));
    Digest(writer, context.targetProfileIdentity);
    Digest(writer, context.observationContractIdentity);
    Digest(writer, context.completionContractIdentity);
    Digest(writer, context.deviceEpochPolicyIdentity);
    writer.WriteU32(static_cast<std::uint32_t>(cases.size()));
    for (const auto& testCase : cases)
    {
        WriteString(writer, testCase.id);
        Digest(writer, testCase.invocation.Identity());
        Digest(writer, testCase.invocation.PreviousActiveSet().Identity());
        Digest(writer, testCase.invocation.ActiveSet().Identity());
        Digest(writer, testCase.invocation.ModifiedSurvivorSet().Identity());
        Digest(writer, testCase.invocation.TransitionSet().Identity());
        writer.WriteU32(testCase.invocation.ForceFullActiveRebuild() ? 1u : 0u);
        Digest(writer, ArtifactSetIdentity(testCase));
    }
    Digest(writer, base::Sha256(writer.Bytes()));
    return std::move(writer).Take();
}

base::Digest256 BuildTimelineBindingIdentity(
    std::uint64_t epoch,
    std::uint32_t count,
    const base::Digest256& authorityIdentity,
    const fe::VerifiedDeltaFamilyCaseV1& first,
    const fe::VerifiedDeltaFamilyCaseV1& last)
{
    base::BinaryWriter writer;
    Digest(writer, Literal("SGE4-5.Spiral7.CU5.TimelineBinding.V1"));
    writer.WriteU64(epoch);
    writer.WriteU32(count);
    Digest(writer, authorityIdentity);
    Digest(writer, first.invocation.Identity());
    Digest(writer, last.invocation.Identity());
    return base::Sha256(writer.Bytes());
}

base::Digest256 BuildRepresentationSetIdentity(
    std::uint64_t epoch,
    std::span<const fe::VerifiedDeltaFamilyCaseV1> cases)
{
    base::BinaryWriter writer;
    Digest(writer, Literal("SGE4-5.Spiral7.CU5.RepresentationSet.V1"));
    writer.WriteU64(epoch);
    writer.WriteU32(static_cast<std::uint32_t>(cases.size()));
    for (const auto& value : cases) Digest(writer, ArtifactSetIdentity(value));
    return base::Sha256(writer.Bytes());
}

base::Digest256 BuildHistoryIdentity(const DeltaHistoryHandleV1& value)
{
    base::BinaryWriter writer;
    Digest(writer, Literal("SGE4-5.Spiral7.CU5.HistoryHandle.V1"));
    writer.WriteU64(value.deviceEpoch);
    writer.WriteU32(value.invocationIndex);
    Digest(writer, value.activeSetIdentity);
    Digest(writer, value.outputDigest);
    return base::Sha256(writer.Bytes());
}

base::Digest256 BuildTokenIdentity(
    std::uint64_t epoch,
    std::uint64_t ordinal,
    std::uint32_t invocationCount,
    const base::Digest256& authorityIdentity)
{
    base::BinaryWriter writer;
    Digest(writer, Literal("SGE4-5.Spiral7.CU5.SubmissionToken.V1"));
    writer.WriteU64(epoch);
    writer.WriteU64(ordinal);
    writer.WriteU32(invocationCount);
    Digest(writer, authorityIdentity);
    return base::Sha256(writer.Bytes());
}

base::Digest256 BuildRecoveryBindingIdentity(
    std::uint64_t epoch,
    const semantic::SparseDeltaInvocationV1& semanticRecovery,
    const fe::VerifiedDeltaFamilyCaseV1& initializationSeed,
    const fe::VerifiedDeltaFamilyCaseV1& executionRecovery)
{
    base::BinaryWriter writer;
    Digest(writer, Literal("SGE4-5.Spiral7.CU5.RecoverySeedBinding.V1"));
    writer.WriteU64(epoch);
    Digest(writer, semanticRecovery.Identity());
    Digest(writer, initializationSeed.invocation.Identity());
    Digest(writer, executionRecovery.invocation.Identity());
    Digest(writer, semanticRecovery.ActiveSet().Identity());
    Digest(writer, semanticRecovery.ModifiedSurvivorSet().Identity());
    return base::Sha256(writer.Bytes());
}

#if defined(_WIN32)
using Microsoft::WRL::ComPtr;

std::string WideToUtf8(const wchar_t* text)
{
    if (!text) return {};
    const int count = WideCharToMultiByte(CP_UTF8, 0, text, -1, nullptr, 0, nullptr, nullptr);
    if (count <= 1) return {};
    std::string result(static_cast<std::size_t>(count), '\0');
    WideCharToMultiByte(CP_UTF8, 0, text, -1, result.data(), count, nullptr, nullptr);
    result.pop_back();
    return result;
}
#endif

struct NativeDevice final
{
#if defined(_WIN32)
    ComPtr<IDXGIFactory6> factory;
    ComPtr<IDXGIAdapter1> adapter;
    ComPtr<ID3D12Device> device;
    LUID luid{};
#endif
    std::string description;
};

base::Result<std::unique_ptr<NativeDevice>, DeltaFamilyRuntimeErrorV1> CreateNativeDevice(
    const std::optional<std::pair<std::uint32_t, std::int32_t>>& excluded)
{
#if !defined(_WIN32)
    (void)excluded;
    auto native = std::make_unique<NativeDevice>();
    native->description = "Portable self-test (no D3D12)";
    return base::Result<std::unique_ptr<NativeDevice>, DeltaFamilyRuntimeErrorV1>::Success(
        std::move(native));
#else
    auto native = std::make_unique<NativeDevice>();
    HRESULT hr = CreateDXGIFactory2(0, IID_PPV_ARGS(&native->factory));
    if (FAILED(hr))
        return base::Result<std::unique_ptr<NativeDevice>, DeltaFamilyRuntimeErrorV1>::Failure(
            Error("delta-runtime/factory", "CreateDXGIFactory2 failed.", hr));
    hr = native->factory->EnumWarpAdapter(IID_PPV_ARGS(&native->adapter));
    if (FAILED(hr))
        return base::Result<std::unique_ptr<NativeDevice>, DeltaFamilyRuntimeErrorV1>::Failure(
            Error("delta-runtime/adapter", "EnumWarpAdapter failed.", hr));
    DXGI_ADAPTER_DESC1 desc{};
    hr = native->adapter->GetDesc1(&desc);
    if (FAILED(hr))
        return base::Result<std::unique_ptr<NativeDevice>, DeltaFamilyRuntimeErrorV1>::Failure(
            Error("delta-runtime/adapter-desc", "GetDesc1 failed.", hr));
    native->luid = desc.AdapterLuid;
    native->description = WideToUtf8(desc.Description);
    if (excluded && excluded->first == native->luid.LowPart &&
        excluded->second == native->luid.HighPart)
    {
        return base::Result<std::unique_ptr<NativeDevice>, DeltaFamilyRuntimeErrorV1>::Failure(
            Error("delta-runtime/excluded-adapter",
                  "The only WARP adapter is the removed adapter and remains excluded."));
    }
    hr = D3D12CreateDevice(native->adapter.Get(), D3D_FEATURE_LEVEL_11_0,
                           IID_PPV_ARGS(&native->device));
    if (FAILED(hr))
        return base::Result<std::unique_ptr<NativeDevice>, DeltaFamilyRuntimeErrorV1>::Failure(
            Error("delta-runtime/device", "D3D12CreateDevice(WARP) failed.", hr));
    return base::Result<std::unique_ptr<NativeDevice>, DeltaFamilyRuntimeErrorV1>::Success(
        std::move(native));
#endif
}
} // namespace

struct LoadedDeltaFamilyRuntimeV1::Impl final
{
    semantic::SparseTemporalDeltaSemanticV1 semanticValue;
    fv::DeltaFamilyVerificationContextV1 context;
    std::vector<fe::VerifiedDeltaFamilyCaseV1> authorityCases;
    std::vector<std::byte> authorityBytes;
    base::Digest256 authorityIdentity{};
    base::Digest256 domainIdentity{};
    DeltaFamilyRuntimeStateV1 state = DeltaFamilyRuntimeStateV1::Active;
    std::uint64_t deviceEpoch = 1;
    std::uint64_t submissionOrdinal = 0;
    std::unique_ptr<NativeDevice> native;
    std::string adapterDescription;
    std::optional<std::pair<std::uint32_t, std::int32_t>> removedLuid;
    bool timelineBound = false;
    bool representationsBuilt = false;
    bool recoverySeedRequired = false;
    base::Digest256 currentBindingIdentity{};
    base::Digest256 currentRepresentationIdentity{};
};

LoadedDeltaFamilyRuntimeV1::LoadedDeltaFamilyRuntimeV1(std::unique_ptr<Impl> impl)
    : impl_(std::move(impl)) {}
LoadedDeltaFamilyRuntimeV1::LoadedDeltaFamilyRuntimeV1(LoadedDeltaFamilyRuntimeV1&&) noexcept = default;
LoadedDeltaFamilyRuntimeV1& LoadedDeltaFamilyRuntimeV1::operator=(LoadedDeltaFamilyRuntimeV1&&) noexcept = default;
LoadedDeltaFamilyRuntimeV1::~LoadedDeltaFamilyRuntimeV1() = default;

DeltaFamilyRuntimeStateV1 LoadedDeltaFamilyRuntimeV1::State() const noexcept { return impl_->state; }
std::uint64_t LoadedDeltaFamilyRuntimeV1::DeviceEpoch() const noexcept { return impl_->deviceEpoch; }
const base::Digest256& LoadedDeltaFamilyRuntimeV1::DomainIdentity() const noexcept { return impl_->domainIdentity; }
const base::Digest256& LoadedDeltaFamilyRuntimeV1::AuthorityIdentity() const noexcept { return impl_->authorityIdentity; }
const std::string& LoadedDeltaFamilyRuntimeV1::AdapterDescription() const noexcept { return impl_->adapterDescription; }
std::size_t LoadedDeltaFamilyRuntimeV1::AuthorityInvocationCount() const noexcept { return impl_->authorityCases.size(); }
bool LoadedDeltaFamilyRuntimeV1::TimelineBound() const noexcept { return impl_->timelineBound; }
bool LoadedDeltaFamilyRuntimeV1::RepresentationsBuilt() const noexcept { return impl_->representationsBuilt; }
bool LoadedDeltaFamilyRuntimeV1::RecoverySeedRequired() const noexcept { return impl_->recoverySeedRequired; }

base::Result<LoadedDeltaFamilyRuntimeV1, DeltaFamilyRuntimeErrorV1>
LoadDeltaFamilyRuntimeOnWarpV1(
    const semantic::SparseTemporalDeltaSemanticV1& semanticValue,
    const fv::DeltaFamilyVerificationContextV1& context,
    std::span<const fe::VerifiedDeltaFamilyCaseV1> authorityCases)
{
    try
    {
        if (authorityCases.size() != semantic::TimelineLengthV1)
            return base::Result<LoadedDeltaFamilyRuntimeV1, DeltaFamilyRuntimeErrorV1>::Failure(
                Error("delta-runtime/authority-count", "CU5 requires exactly 128 authority invocations."));
        if (!(context == fv::BuildCanonicalDeltaFamilyVerificationContextV1()))
            return base::Result<LoadedDeltaFamilyRuntimeV1, DeltaFamilyRuntimeErrorV1>::Failure(
                Error("delta-runtime/context", "Delta Family verification context is not canonical."));

        for (std::uint32_t index = 0; index < authorityCases.size(); ++index)
        {
            auto valid = ValidateAuthorityCase(semanticValue, context, authorityCases[index], index);
            if (!valid)
                return base::Result<LoadedDeltaFamilyRuntimeV1, DeltaFamilyRuntimeErrorV1>::Failure(
                    valid.Error());
            if (index == 0)
            {
                if (authorityCases[index].invocation.PreviousActiveSet().Cardinality().Value() != 0)
                    return base::Result<LoadedDeltaFamilyRuntimeV1, DeltaFamilyRuntimeErrorV1>::Failure(
                        Error("delta-runtime/initial", "The first authority invocation must begin from empty membership."));
            }
            else if (authorityCases[index].invocation.PreviousActiveSet().Identity() !=
                     authorityCases[index - 1].invocation.ActiveSet().Identity())
            {
                return base::Result<LoadedDeltaFamilyRuntimeV1, DeltaFamilyRuntimeErrorV1>::Failure(
                    Error("delta-runtime/timeline-chain", "Authority Active membership is not chained."));
            }
        }

        auto nativeResult = CreateNativeDevice(std::nullopt);
        if (!nativeResult)
            return base::Result<LoadedDeltaFamilyRuntimeV1, DeltaFamilyRuntimeErrorV1>::Failure(
                nativeResult.Error());

        auto impl = std::make_unique<LoadedDeltaFamilyRuntimeV1::Impl>();
        impl->semanticValue = semanticValue;
        impl->context = context;
        impl->authorityCases.assign(authorityCases.begin(), authorityCases.end());
        impl->authorityBytes = SerializeAuthority(semanticValue, context, authorityCases);
        impl->authorityIdentity = base::Sha256(impl->authorityBytes);
        impl->native = std::move(nativeResult).Value();
        impl->adapterDescription = impl->native->description;
        base::BinaryWriter domain;
        Digest(domain, Literal("SGE4-5.Spiral7.CU5.DeltaFamilyRuntimeDomain.V1"));
        Digest(domain, semantic::SparseTemporalDeltaSemanticIdentityV1(semanticValue));
        Digest(domain, impl->authorityIdentity);
        impl->domainIdentity = base::Sha256(domain.Bytes());
        return base::Result<LoadedDeltaFamilyRuntimeV1, DeltaFamilyRuntimeErrorV1>::Success(
            LoadedDeltaFamilyRuntimeV1(std::move(impl)));
    }
    catch (const std::exception& error)
    {
        return base::Result<LoadedDeltaFamilyRuntimeV1, DeltaFamilyRuntimeErrorV1>::Failure(
            Error("delta-runtime/load", error.what()));
    }
}

DeltaRuntimeEpochHandleV1 CaptureDeltaRuntimeEpochHandleV1(
    const LoadedDeltaFamilyRuntimeV1& runtime)
{
    return {runtime.impl_->deviceEpoch, runtime.impl_->domainIdentity,
            runtime.impl_->authorityIdentity};
}

base::Result<void, DeltaFamilyRuntimeErrorV1>
ValidateDeltaRuntimeEpochHandleV1(
    const LoadedDeltaFamilyRuntimeV1& runtime,
    const DeltaRuntimeEpochHandleV1& handle)
{
    if (runtime.impl_->state != DeltaFamilyRuntimeStateV1::Active)
        return base::Result<void, DeltaFamilyRuntimeErrorV1>::Failure(
            Error("delta-runtime/device-state", "Delta Family Runtime is awaiting an adapter."));
    if (handle.deviceEpoch != runtime.impl_->deviceEpoch)
        return base::Result<void, DeltaFamilyRuntimeErrorV1>::Failure(
            Error("delta-runtime/stale-epoch", "Delta Runtime epoch handle is stale."));
    if (handle.domainIdentity != runtime.impl_->domainIdentity ||
        handle.authorityIdentity != runtime.impl_->authorityIdentity)
        return base::Result<void, DeltaFamilyRuntimeErrorV1>::Failure(
            Error("delta-runtime/epoch-identity", "Delta Runtime epoch handle identity differs."));
    return base::Result<void, DeltaFamilyRuntimeErrorV1>::Success();
}

base::Result<DeltaTimelineBindingV1, DeltaFamilyRuntimeErrorV1>
BindDeltaTimelinePrefixV1(
    LoadedDeltaFamilyRuntimeV1& runtime,
    const DeltaRuntimeEpochHandleV1& handle,
    std::uint32_t invocationCount)
{
    auto epoch = ValidateDeltaRuntimeEpochHandleV1(runtime, handle);
    if (!epoch) return base::Result<DeltaTimelineBindingV1, DeltaFamilyRuntimeErrorV1>::Failure(epoch.Error());
    if (runtime.impl_->recoverySeedRequired)
        return base::Result<DeltaTimelineBindingV1, DeltaFamilyRuntimeErrorV1>::Failure(
            Error("delta-runtime/recovery-seed-required",
                  "A forced full-active Recovery seed is required before normal timeline submission."));
    if (invocationCount == 0 || invocationCount > runtime.impl_->authorityCases.size())
        return base::Result<DeltaTimelineBindingV1, DeltaFamilyRuntimeErrorV1>::Failure(
            Error("delta-runtime/binding-range", "Timeline prefix count is outside the Frozen authority."));

    DeltaTimelineBindingV1 result;
    result.deviceEpoch = runtime.impl_->deviceEpoch;
    result.invocationCount = invocationCount;
    result.firstInvocationIdentity = runtime.impl_->authorityCases.front().invocation.Identity();
    result.lastInvocationIdentity = runtime.impl_->authorityCases[invocationCount - 1].invocation.Identity();
    result.bindingIdentity = BuildTimelineBindingIdentity(
        result.deviceEpoch, result.invocationCount, runtime.impl_->authorityIdentity,
        runtime.impl_->authorityCases.front(), runtime.impl_->authorityCases[invocationCount - 1]);
    runtime.impl_->timelineBound = true;
    runtime.impl_->representationsBuilt = false;
    runtime.impl_->currentBindingIdentity = result.bindingIdentity;
    runtime.impl_->currentRepresentationIdentity = {};
    return base::Result<DeltaTimelineBindingV1, DeltaFamilyRuntimeErrorV1>::Success(result);
}

base::Result<DeltaRepresentationSetHandleV1, DeltaFamilyRuntimeErrorV1>
RebuildDeltaTimelineRepresentationsV1(
    LoadedDeltaFamilyRuntimeV1& runtime,
    const DeltaRuntimeEpochHandleV1& handle,
    const DeltaTimelineBindingV1& binding)
{
    auto epoch = ValidateDeltaRuntimeEpochHandleV1(runtime, handle);
    if (!epoch) return base::Result<DeltaRepresentationSetHandleV1, DeltaFamilyRuntimeErrorV1>::Failure(epoch.Error());
    if (!runtime.impl_->timelineBound || binding.deviceEpoch != runtime.impl_->deviceEpoch ||
        binding.bindingIdentity != runtime.impl_->currentBindingIdentity)
        return base::Result<DeltaRepresentationSetHandleV1, DeltaFamilyRuntimeErrorV1>::Failure(
            Error("delta-runtime/rebind-required", "Timeline must be explicitly rebound in the current epoch."));
    if (binding.invocationCount == 0 || binding.invocationCount > runtime.impl_->authorityCases.size())
        return base::Result<DeltaRepresentationSetHandleV1, DeltaFamilyRuntimeErrorV1>::Failure(
            Error("delta-runtime/binding-range", "Timeline binding count is invalid."));

    DeltaRepresentationSetHandleV1 result;
    result.deviceEpoch = runtime.impl_->deviceEpoch;
    result.invocationCount = binding.invocationCount;
    result.representationSetIdentity = BuildRepresentationSetIdentity(
        result.deviceEpoch,
        std::span<const fe::VerifiedDeltaFamilyCaseV1>(
            runtime.impl_->authorityCases.data(), result.invocationCount));
    runtime.impl_->representationsBuilt = true;
    runtime.impl_->currentRepresentationIdentity = result.representationSetIdentity;
    return base::Result<DeltaRepresentationSetHandleV1, DeltaFamilyRuntimeErrorV1>::Success(result);
}

base::Result<DeltaRuntimeSubmissionV1, DeltaFamilyRuntimeErrorV1>
SubmitDeltaTimelinePrefixV1(
    LoadedDeltaFamilyRuntimeV1& runtime,
    const DeltaRuntimeEpochHandleV1& handle,
    const DeltaTimelineBindingV1& binding,
    const DeltaRepresentationSetHandleV1& representations)
{
    auto epoch = ValidateDeltaRuntimeEpochHandleV1(runtime, handle);
    if (!epoch) return base::Result<DeltaRuntimeSubmissionV1, DeltaFamilyRuntimeErrorV1>::Failure(epoch.Error());
    if (!runtime.impl_->timelineBound || binding.bindingIdentity != runtime.impl_->currentBindingIdentity)
        return base::Result<DeltaRuntimeSubmissionV1, DeltaFamilyRuntimeErrorV1>::Failure(
            Error("delta-runtime/rebind-required", "Current-epoch timeline binding is missing."));
    if (!runtime.impl_->representationsBuilt ||
        representations.deviceEpoch != runtime.impl_->deviceEpoch ||
        representations.invocationCount != binding.invocationCount ||
        representations.representationSetIdentity != runtime.impl_->currentRepresentationIdentity)
        return base::Result<DeltaRuntimeSubmissionV1, DeltaFamilyRuntimeErrorV1>::Failure(
            Error("delta-runtime/rebuild-required", "Current-epoch Candidate representations were not rebuilt."));

    std::vector<fe::VerifiedDeltaFamilyCaseV1> cases(
        runtime.impl_->authorityCases.begin(),
        runtime.impl_->authorityCases.begin() + binding.invocationCount);
    auto executed = fe::ExecuteVerifiedDeltaCandidateFamilyV1(runtime.impl_->semanticValue, cases);
    if (!executed)
        return base::Result<DeltaRuntimeSubmissionV1, DeltaFamilyRuntimeErrorV1>::Failure(
            Error(executed.Error().stage, executed.Error().message, executed.Error().nativeCode));
    if (executed.Value().invocations.size() != binding.invocationCount)
        return base::Result<DeltaRuntimeSubmissionV1, DeltaFamilyRuntimeErrorV1>::Failure(
            Error("delta-runtime/report", "Timeline report invocation count differs."));

    DeltaRuntimeSubmissionV1 result;
    result.representations = representations;
    result.representationResourcesMaterialized = true;
    result.indirectArgumentsMaterialized = true;
    result.historyStateMaterialized = true;
    result.completionStateMaterialized = true;
    result.readbackStateMaterialized = true;
    result.architecture = std::move(executed).Value();

    const auto& lastAuthority = runtime.impl_->authorityCases[binding.invocationCount - 1];
    const auto& lastObservation = result.architecture.invocations.back();
    if (lastObservation.candidates.size() != CandidateOrder.size())
        return base::Result<DeltaRuntimeSubmissionV1, DeltaFamilyRuntimeErrorV1>::Failure(
            Error("delta-runtime/report", "Last invocation does not contain A/B/C observations."));
    result.history.deviceEpoch = runtime.impl_->deviceEpoch;
    result.history.invocationIndex = lastAuthority.invocation.InvocationIndex();
    result.history.activeSetIdentity = lastAuthority.invocation.ActiveSet().Identity();
    result.history.outputDigest = lastObservation.candidates.front().outputDigest;
    result.history.historyIdentity = BuildHistoryIdentity(result.history);

    ++runtime.impl_->submissionOrdinal;
    result.completion.deviceEpoch = runtime.impl_->deviceEpoch;
    result.completion.submissionOrdinal = runtime.impl_->submissionOrdinal;
    result.completion.invocationCount = binding.invocationCount;
    result.completion.tokenIdentity = BuildTokenIdentity(
        result.completion.deviceEpoch, result.completion.submissionOrdinal,
        result.completion.invocationCount, runtime.impl_->authorityIdentity);
    return base::Result<DeltaRuntimeSubmissionV1, DeltaFamilyRuntimeErrorV1>::Success(
        std::move(result));
}

base::Result<void, DeltaFamilyRuntimeErrorV1>
ValidateDeltaRepresentationSetHandleV1(
    const LoadedDeltaFamilyRuntimeV1& runtime,
    const DeltaRepresentationSetHandleV1& handle)
{
    if (runtime.impl_->state != DeltaFamilyRuntimeStateV1::Active)
        return base::Result<void, DeltaFamilyRuntimeErrorV1>::Failure(
            Error("delta-runtime/device-state", "Delta Family Runtime is awaiting an adapter."));
    if (handle.deviceEpoch != runtime.impl_->deviceEpoch)
        return base::Result<void, DeltaFamilyRuntimeErrorV1>::Failure(
            Error("delta-runtime/stale-epoch", "Delta representation handle is stale."));
    if (handle.invocationCount == 0 || handle.invocationCount > runtime.impl_->authorityCases.size())
        return base::Result<void, DeltaFamilyRuntimeErrorV1>::Failure(
            Error("delta-runtime/representation-range", "Delta representation handle range is invalid."));
    const auto expected = BuildRepresentationSetIdentity(
        handle.deviceEpoch,
        std::span<const fe::VerifiedDeltaFamilyCaseV1>(
            runtime.impl_->authorityCases.data(), handle.invocationCount));
    if (handle.representationSetIdentity != expected)
        return base::Result<void, DeltaFamilyRuntimeErrorV1>::Failure(
            Error("delta-runtime/representation-identity", "Delta representation handle identity differs."));
    return base::Result<void, DeltaFamilyRuntimeErrorV1>::Success();
}

base::Result<void, DeltaFamilyRuntimeErrorV1>
ValidateDeltaHistoryHandleV1(
    const LoadedDeltaFamilyRuntimeV1& runtime,
    const DeltaHistoryHandleV1& handle)
{
    if (runtime.impl_->state != DeltaFamilyRuntimeStateV1::Active)
        return base::Result<void, DeltaFamilyRuntimeErrorV1>::Failure(
            Error("delta-runtime/device-state", "Delta Family Runtime is awaiting an adapter."));
    if (handle.deviceEpoch != runtime.impl_->deviceEpoch)
        return base::Result<void, DeltaFamilyRuntimeErrorV1>::Failure(
            Error("delta-runtime/stale-epoch", "Delta History handle is stale."));
    if (handle.historyIdentity != BuildHistoryIdentity(handle))
        return base::Result<void, DeltaFamilyRuntimeErrorV1>::Failure(
            Error("delta-runtime/history-identity", "Delta History handle identity differs."));
    return base::Result<void, DeltaFamilyRuntimeErrorV1>::Success();
}

base::Result<void, DeltaFamilyRuntimeErrorV1>
ValidateDeltaRuntimeSubmissionTokenV1(
    const LoadedDeltaFamilyRuntimeV1& runtime,
    const DeltaRuntimeSubmissionTokenV1& token)
{
    if (runtime.impl_->state != DeltaFamilyRuntimeStateV1::Active)
        return base::Result<void, DeltaFamilyRuntimeErrorV1>::Failure(
            Error("delta-runtime/device-state", "Delta Family Runtime is awaiting an adapter."));
    if (token.deviceEpoch != runtime.impl_->deviceEpoch)
        return base::Result<void, DeltaFamilyRuntimeErrorV1>::Failure(
            Error("delta-runtime/stale-epoch", "Delta completion token is stale."));
    if (token.tokenIdentity != BuildTokenIdentity(
            token.deviceEpoch, token.submissionOrdinal,
            token.invocationCount, runtime.impl_->authorityIdentity))
        return base::Result<void, DeltaFamilyRuntimeErrorV1>::Failure(
            Error("delta-runtime/token-identity", "Delta completion token identity differs."));
    return base::Result<void, DeltaFamilyRuntimeErrorV1>::Success();
}

base::Result<DeltaFamilyRecoveryReportV1, DeltaFamilyRuntimeErrorV1>
RecoverDeltaFamilyRuntimeV1(
    LoadedDeltaFamilyRuntimeV1& runtime,
    DeltaFamilyRecoveryModeV1 mode)
{
    DeltaFamilyRecoveryReportV1 report;
    report.mode = mode;
    report.stateBefore = runtime.impl_->state;
    report.previousDeviceEpoch = runtime.impl_->deviceEpoch;
    const auto authorityBefore = runtime.impl_->authorityBytes;

    auto invalidate = [&]()
    {
        runtime.impl_->timelineBound = false;
        runtime.impl_->representationsBuilt = false;
        runtime.impl_->currentBindingIdentity = {};
        runtime.impl_->currentRepresentationIdentity = {};
        runtime.impl_->recoverySeedRequired = true;
        report.allRuntimeObjectsReleased = true;
        report.representationResourcesInvalidated = true;
        report.indirectArgumentsInvalidated = true;
        report.historyStateInvalidated = true;
        report.completionStateInvalidated = true;
        report.readbackStateInvalidated = true;
        report.externalSetRebindRequired = true;
        report.explicitRepresentationRebuildRequired = true;
        report.forcedFullActiveSeedRequired = true;
    };

    if (mode == DeltaFamilyRecoveryModeV1::ControlledRebuild)
    {
        if (runtime.impl_->state != DeltaFamilyRuntimeStateV1::Active)
            return base::Result<DeltaFamilyRecoveryReportV1, DeltaFamilyRuntimeErrorV1>::Failure(
                Error("delta-runtime/recovery-state", "Controlled Recovery requires an Active Runtime."));
        runtime.impl_->native.reset();
        invalidate();
        auto rematerialized = CreateNativeDevice(std::nullopt);
        if (!rematerialized)
            return base::Result<DeltaFamilyRecoveryReportV1, DeltaFamilyRuntimeErrorV1>::Failure(
                rematerialized.Error());
        runtime.impl_->native = std::move(rematerialized).Value();
        runtime.impl_->adapterDescription = runtime.impl_->native->description;
        ++runtime.impl_->deviceEpoch;
        runtime.impl_->state = DeltaFamilyRuntimeStateV1::Active;
        report.adapterReacquired = true;
        report.nativeDeviceRematerialized = true;
        report.executionContextRematerialized = true;
    }
    else if (mode == DeltaFamilyRecoveryModeV1::ForceRemovalForTest)
    {
        if (runtime.impl_->state != DeltaFamilyRuntimeStateV1::Active)
            return base::Result<DeltaFamilyRecoveryReportV1, DeltaFamilyRuntimeErrorV1>::Failure(
                Error("delta-runtime/recovery-state", "Forced removal requires an Active Runtime."));
#if !defined(_WIN32)
        return base::Result<DeltaFamilyRecoveryReportV1, DeltaFamilyRuntimeErrorV1>::Failure(
            Error("delta-runtime/platform", "Forced D3D12 removal requires Windows."));
#else
        ComPtr<ID3D12Device5> device5;
        HRESULT hr = runtime.impl_->native->device.As(&device5);
        if (FAILED(hr))
            return base::Result<DeltaFamilyRecoveryReportV1, DeltaFamilyRuntimeErrorV1>::Failure(
                Error("delta-runtime/device5", "ID3D12Device5 is unavailable.", hr));
        device5->RemoveDevice();
        const HRESULT reason = runtime.impl_->native->device->GetDeviceRemovedReason();
        report.removalReason = reason;
        report.forcedRemoval = true;
        report.removedAdapterLuidLow = runtime.impl_->native->luid.LowPart;
        report.removedAdapterLuidHigh = runtime.impl_->native->luid.HighPart;
        runtime.impl_->removedLuid = std::pair{
            report.removedAdapterLuidLow, report.removedAdapterLuidHigh};
        runtime.impl_->native.reset();
        invalidate();
        runtime.impl_->state = DeltaFamilyRuntimeStateV1::AwaitingAdapter;
#endif
    }
    else
    {
        if (runtime.impl_->state != DeltaFamilyRuntimeStateV1::AwaitingAdapter)
            return base::Result<DeltaFamilyRecoveryReportV1, DeltaFamilyRuntimeErrorV1>::Failure(
                Error("delta-runtime/recovery-state", "Retry requires AwaitingAdapter state."));
        auto retry = CreateNativeDevice(runtime.impl_->removedLuid);
        if (retry)
        {
            runtime.impl_->native = std::move(retry).Value();
            runtime.impl_->adapterDescription = runtime.impl_->native->description;
            ++runtime.impl_->deviceEpoch;
            runtime.impl_->state = DeltaFamilyRuntimeStateV1::Active;
            report.adapterReacquired = true;
            report.nativeDeviceRematerialized = true;
            report.executionContextRematerialized = true;
        }
        else if (retry.Error().stage != "delta-runtime/excluded-adapter")
        {
            return base::Result<DeltaFamilyRecoveryReportV1, DeltaFamilyRuntimeErrorV1>::Failure(
                retry.Error());
        }
    }

    report.stateAfter = runtime.impl_->state;
    report.newDeviceEpoch = runtime.impl_->deviceEpoch;
    report.frozenAuthorityPreserved = authorityBefore == runtime.impl_->authorityBytes;
    return base::Result<DeltaFamilyRecoveryReportV1, DeltaFamilyRuntimeErrorV1>::Success(report);
}

base::Result<DeltaRecoverySeedBindingV1, DeltaFamilyRuntimeErrorV1>
BindDeltaRecoverySeedV1(
    LoadedDeltaFamilyRuntimeV1& runtime,
    const DeltaRuntimeEpochHandleV1& handle,
    const semantic::SparseDeltaInvocationV1& semanticRecoveryInvocation,
    const fe::VerifiedDeltaFamilyCaseV1& initializationSeedCase,
    const fe::VerifiedDeltaFamilyCaseV1& executionRecoveryCase)
{
    auto epoch = ValidateDeltaRuntimeEpochHandleV1(runtime, handle);
    if (!epoch) return base::Result<DeltaRecoverySeedBindingV1, DeltaFamilyRuntimeErrorV1>::Failure(epoch.Error());
    if (!runtime.impl_->recoverySeedRequired)
        return base::Result<DeltaRecoverySeedBindingV1, DeltaFamilyRuntimeErrorV1>::Failure(
            Error("delta-runtime/recovery-state", "Runtime does not require a Recovery seed."));
    if (!semanticRecoveryInvocation.ForceFullActiveRebuild() ||
        semanticRecoveryInvocation.PreviousActiveSet().Identity() !=
            semanticRecoveryInvocation.ActiveSet().Identity() ||
        semanticRecoveryInvocation.UpdateSet().Identity() !=
            semanticRecoveryInvocation.ActiveSet().Identity() ||
        semanticRecoveryInvocation.RetainSet().Cardinality().Value() != 0 ||
        semanticRecoveryInvocation.DeactivationSet().Cardinality().Value() != 0)
        return base::Result<DeltaRecoverySeedBindingV1, DeltaFamilyRuntimeErrorV1>::Failure(
            Error("delta-runtime/recovery-meaning",
                  "Semantic Recovery invocation must explicitly rebind current A_t and force W_t=A_t, H_t=empty."));

    if (!initializationSeedCase.invocation.ForceFullActiveRebuild() ||
        initializationSeedCase.invocation.InvocationIndex() != 0 ||
        initializationSeedCase.invocation.PreviousActiveSet().Cardinality().Value() != 0 ||
        initializationSeedCase.invocation.ActiveSet().Identity() !=
            semanticRecoveryInvocation.ActiveSet().Identity())
        return base::Result<DeltaRecoverySeedBindingV1, DeltaFamilyRuntimeErrorV1>::Failure(
            Error("delta-runtime/recovery-initialization",
                  "Recovery initialization seed must be invocation zero from empty membership over the rebound Active Set."));

    if (!executionRecoveryCase.invocation.ForceFullActiveRebuild() ||
        executionRecoveryCase.invocation.InvocationIndex() != 1 ||
        executionRecoveryCase.invocation.PreviousActiveSet().Identity() !=
            semanticRecoveryInvocation.ActiveSet().Identity() ||
        executionRecoveryCase.invocation.ActiveSet().Identity() !=
            semanticRecoveryInvocation.ActiveSet().Identity() ||
        executionRecoveryCase.invocation.ModifiedSurvivorSet().Identity() !=
            semanticRecoveryInvocation.ModifiedSurvivorSet().Identity() ||
        executionRecoveryCase.invocation.UpdateSet().Identity() !=
            semanticRecoveryInvocation.ActiveSet().Identity() ||
        executionRecoveryCase.invocation.RetainSet().Cardinality().Value() != 0 ||
        executionRecoveryCase.invocation.DeactivationSet().Cardinality().Value() != 0 ||
        executionRecoveryCase.invocation.ItemGenerations() !=
            semanticRecoveryInvocation.ItemGenerations())
        return base::Result<DeltaRecoverySeedBindingV1, DeltaFamilyRuntimeErrorV1>::Failure(
            Error("delta-runtime/recovery-materialization",
                  "Recovery execution invocation must reproduce the rebound Active/Modified sets and exact generations while forcing W=A and H=empty."));

    auto initializationValid = ValidateAuthorityCase(
        runtime.impl_->semanticValue, runtime.impl_->context, initializationSeedCase, 0);
    if (!initializationValid)
        return base::Result<DeltaRecoverySeedBindingV1, DeltaFamilyRuntimeErrorV1>::Failure(
            initializationValid.Error());
    auto recoveryValid = ValidateAuthorityCase(
        runtime.impl_->semanticValue, runtime.impl_->context, executionRecoveryCase, 1);
    if (!recoveryValid)
        return base::Result<DeltaRecoverySeedBindingV1, DeltaFamilyRuntimeErrorV1>::Failure(
            recoveryValid.Error());

    DeltaRecoverySeedBindingV1 result;
    result.deviceEpoch = runtime.impl_->deviceEpoch;
    result.semanticInvocationIndex = semanticRecoveryInvocation.InvocationIndex();
    result.semanticRecoveryInvocationIdentity = semanticRecoveryInvocation.Identity();
    result.initializationSeedInvocationIdentity = initializationSeedCase.invocation.Identity();
    result.executionRecoveryInvocationIdentity = executionRecoveryCase.invocation.Identity();
    result.activeSetIdentity = semanticRecoveryInvocation.ActiveSet().Identity();
    result.modifiedSetIdentity = semanticRecoveryInvocation.ModifiedSurvivorSet().Identity();
    result.bindingIdentity = BuildRecoveryBindingIdentity(
        result.deviceEpoch, semanticRecoveryInvocation,
        initializationSeedCase, executionRecoveryCase);
    runtime.impl_->timelineBound = true;
    runtime.impl_->representationsBuilt = false;
    runtime.impl_->currentBindingIdentity = result.bindingIdentity;
    runtime.impl_->currentRepresentationIdentity = {};
    return base::Result<DeltaRecoverySeedBindingV1, DeltaFamilyRuntimeErrorV1>::Success(result);
}

base::Result<DeltaRecoveryRepresentationSetHandleV1, DeltaFamilyRuntimeErrorV1>
RebuildDeltaRecoverySeedRepresentationsV1(
    LoadedDeltaFamilyRuntimeV1& runtime,
    const DeltaRuntimeEpochHandleV1& handle,
    const DeltaRecoverySeedBindingV1& binding,
    const fe::VerifiedDeltaFamilyCaseV1& initializationSeedCase,
    const fe::VerifiedDeltaFamilyCaseV1& executionRecoveryCase)
{
    auto epoch = ValidateDeltaRuntimeEpochHandleV1(runtime, handle);
    if (!epoch) return base::Result<DeltaRecoveryRepresentationSetHandleV1, DeltaFamilyRuntimeErrorV1>::Failure(epoch.Error());
    if (!runtime.impl_->timelineBound || binding.deviceEpoch != runtime.impl_->deviceEpoch ||
        binding.bindingIdentity != runtime.impl_->currentBindingIdentity ||
        binding.initializationSeedInvocationIdentity != initializationSeedCase.invocation.Identity() ||
        binding.executionRecoveryInvocationIdentity != executionRecoveryCase.invocation.Identity())
        return base::Result<DeltaRecoveryRepresentationSetHandleV1, DeltaFamilyRuntimeErrorV1>::Failure(
            Error("delta-runtime/rebind-required", "Recovery seed must be explicitly rebound in the current epoch."));

    DeltaRecoveryRepresentationSetHandleV1 result;
    result.deviceEpoch = runtime.impl_->deviceEpoch;
    result.initializationSeedInvocationIdentity = initializationSeedCase.invocation.Identity();
    result.executionRecoveryInvocationIdentity = executionRecoveryCase.invocation.Identity();
    base::BinaryWriter writer;
    Digest(writer, Literal("SGE4-5.Spiral7.CU5.RecoveryRepresentationSet.V1"));
    writer.WriteU64(result.deviceEpoch);
    Digest(writer, binding.bindingIdentity);
    Digest(writer, ArtifactSetIdentity(initializationSeedCase));
    Digest(writer, ArtifactSetIdentity(executionRecoveryCase));
    result.representationSetIdentity = base::Sha256(writer.Bytes());
    runtime.impl_->representationsBuilt = true;
    runtime.impl_->currentRepresentationIdentity = result.representationSetIdentity;
    return base::Result<DeltaRecoveryRepresentationSetHandleV1, DeltaFamilyRuntimeErrorV1>::Success(result);
}

base::Result<DeltaRuntimeSubmissionV1, DeltaFamilyRuntimeErrorV1>
SubmitDeltaRecoverySeedV1(
    LoadedDeltaFamilyRuntimeV1& runtime,
    const DeltaRuntimeEpochHandleV1& handle,
    const DeltaRecoverySeedBindingV1& binding,
    const DeltaRecoveryRepresentationSetHandleV1& representations,
    const fe::VerifiedDeltaFamilyCaseV1& initializationSeedCase,
    const fe::VerifiedDeltaFamilyCaseV1& executionRecoveryCase)
{
    auto epoch = ValidateDeltaRuntimeEpochHandleV1(runtime, handle);
    if (!epoch) return base::Result<DeltaRuntimeSubmissionV1, DeltaFamilyRuntimeErrorV1>::Failure(epoch.Error());
    if (!runtime.impl_->timelineBound || binding.bindingIdentity != runtime.impl_->currentBindingIdentity)
        return base::Result<DeltaRuntimeSubmissionV1, DeltaFamilyRuntimeErrorV1>::Failure(
            Error("delta-runtime/rebind-required", "Recovery Active/Modified sets were not rebound."));
    if (!runtime.impl_->representationsBuilt ||
        representations.deviceEpoch != runtime.impl_->deviceEpoch ||
        representations.initializationSeedInvocationIdentity != initializationSeedCase.invocation.Identity() ||
        representations.executionRecoveryInvocationIdentity != executionRecoveryCase.invocation.Identity() ||
        representations.representationSetIdentity != runtime.impl_->currentRepresentationIdentity)
        return base::Result<DeltaRuntimeSubmissionV1, DeltaFamilyRuntimeErrorV1>::Failure(
            Error("delta-runtime/rebuild-required", "Recovery Candidate representations were not rebuilt."));

    std::vector<fe::VerifiedDeltaFamilyCaseV1> recoverySequence{
        initializationSeedCase, executionRecoveryCase};
    auto executed = fe::ExecuteVerifiedDeltaCandidateFamilyV1(
        runtime.impl_->semanticValue, recoverySequence);
    if (!executed)
        return base::Result<DeltaRuntimeSubmissionV1, DeltaFamilyRuntimeErrorV1>::Failure(
            Error(executed.Error().stage, executed.Error().message, executed.Error().nativeCode));
    if (executed.Value().invocations.size() != 2 ||
        executed.Value().invocations.back().candidates.size() != CandidateOrder.size())
        return base::Result<DeltaRuntimeSubmissionV1, DeltaFamilyRuntimeErrorV1>::Failure(
            Error("delta-runtime/recovery-report", "Recovery materialization report shape differs."));

    DeltaRuntimeSubmissionV1 result;
    result.representationResourcesMaterialized = true;
    result.indirectArgumentsMaterialized = true;
    result.historyStateMaterialized = true;
    result.completionStateMaterialized = true;
    result.readbackStateMaterialized = true;
    result.architecture = std::move(executed).Value();
    result.history.deviceEpoch = runtime.impl_->deviceEpoch;
    result.history.invocationIndex = binding.semanticInvocationIndex;
    result.history.activeSetIdentity = binding.activeSetIdentity;
    result.history.outputDigest = result.architecture.invocations.back().candidates.front().outputDigest;
    result.history.historyIdentity = BuildHistoryIdentity(result.history);
    ++runtime.impl_->submissionOrdinal;
    result.completion.deviceEpoch = runtime.impl_->deviceEpoch;
    result.completion.submissionOrdinal = runtime.impl_->submissionOrdinal;
    result.completion.invocationCount = 1;
    result.completion.tokenIdentity = BuildTokenIdentity(
        result.completion.deviceEpoch, result.completion.submissionOrdinal,
        result.completion.invocationCount, runtime.impl_->authorityIdentity);
    result.representations.deviceEpoch = representations.deviceEpoch;
    result.representations.invocationCount = 2;
    result.representations.representationSetIdentity = representations.representationSetIdentity;
    runtime.impl_->recoverySeedRequired = false;
    return base::Result<DeltaRuntimeSubmissionV1, DeltaFamilyRuntimeErrorV1>::Success(
        std::move(result));
}

std::vector<std::byte> SerializeDeltaFamilyRuntimeAuthorityV1(
    const LoadedDeltaFamilyRuntimeV1& runtime)
{
    return runtime.impl_->authorityBytes;
}

std::vector<std::byte> SerializeDeltaRuntimeSubmissionV1(
    const DeltaRuntimeSubmissionV1& submission)
{
    base::BinaryWriter writer;
    Digest(writer, Literal("SGE4-5.Spiral7.CU5.RuntimeSubmission.V1"));
    writer.WriteU64(submission.completion.deviceEpoch);
    writer.WriteU64(submission.completion.submissionOrdinal);
    writer.WriteU32(submission.completion.invocationCount);
    Digest(writer, submission.completion.tokenIdentity);
    writer.WriteU64(submission.history.deviceEpoch);
    writer.WriteU32(submission.history.invocationIndex);
    Digest(writer, submission.history.activeSetIdentity);
    Digest(writer, submission.history.outputDigest);
    Digest(writer, submission.history.historyIdentity);
    writer.WriteU64(submission.representations.deviceEpoch);
    writer.WriteU32(submission.representations.invocationCount);
    Digest(writer, submission.representations.representationSetIdentity);
    writer.WriteU32(submission.representationResourcesMaterialized ? 1u : 0u);
    writer.WriteU32(submission.indirectArgumentsMaterialized ? 1u : 0u);
    writer.WriteU32(submission.historyStateMaterialized ? 1u : 0u);
    writer.WriteU32(submission.completionStateMaterialized ? 1u : 0u);
    writer.WriteU32(submission.readbackStateMaterialized ? 1u : 0u);
    const auto report = fe::SerializeDeltaFamilyArchitectureReportV1(submission.architecture);
    writer.WriteU32(static_cast<std::uint32_t>(report.size()));
    writer.WriteBytes(report);
    Digest(writer, base::Sha256(writer.Bytes()));
    return std::move(writer).Take();
}

std::vector<std::byte> SerializeDeltaFamilyRecoveryReportV1(
    const DeltaFamilyRecoveryReportV1& report)
{
    base::BinaryWriter writer;
    Digest(writer, Literal("SGE4-5.Spiral7.CU5.RecoveryReport.V1"));
    writer.WriteU32(static_cast<std::uint32_t>(report.mode));
    writer.WriteU32(static_cast<std::uint32_t>(report.stateBefore));
    writer.WriteU32(static_cast<std::uint32_t>(report.stateAfter));
    writer.WriteU64(report.previousDeviceEpoch);
    writer.WriteU64(report.newDeviceEpoch);
    writer.WriteU32(static_cast<std::uint32_t>(report.removalReason));
    writer.WriteU32(report.removedAdapterLuidLow);
    writer.WriteU32(static_cast<std::uint32_t>(report.removedAdapterLuidHigh));
    for (const bool value : {
        report.forcedRemoval, report.adapterReacquired,
        report.nativeDeviceRematerialized, report.allRuntimeObjectsReleased,
        report.executionContextRematerialized,
        report.representationResourcesInvalidated,
        report.indirectArgumentsInvalidated, report.historyStateInvalidated,
        report.completionStateInvalidated, report.readbackStateInvalidated,
        report.externalSetRebindRequired,
        report.explicitRepresentationRebuildRequired,
        report.forcedFullActiveSeedRequired,
        report.frozenAuthorityPreserved})
        writer.WriteU32(value ? 1u : 0u);
    Digest(writer, base::Sha256(writer.Bytes()));
    return std::move(writer).Take();
}
} // namespace sge4_5::spiral7::architecture_runtime
