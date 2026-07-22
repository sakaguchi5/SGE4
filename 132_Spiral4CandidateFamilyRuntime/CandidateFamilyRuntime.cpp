#ifndef NOMINMAX
#define NOMINMAX
#endif

#include "CandidateFamilyRuntime.h"

#include "../00_Foundation/BinaryIO.h"

#include <Windows.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl/client.h>

#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "dxguid.lib")

#include <algorithm>
#include <span>
#include <sstream>
#include <stdexcept>
#include <string_view>
#include <utility>

namespace sge4_5::spiral4::family_runtime
{
namespace
{
using Microsoft::WRL::ComPtr;

base::Digest256 LiteralIdentity(std::string_view value)
{
    return base::Sha256(std::as_bytes(std::span(value.data(), value.size())));
}

CandidateFamilyRuntimeErrorV1 Error(
    std::string stage,
    std::string message,
    HRESULT hr = S_OK)
{
    return CandidateFamilyRuntimeErrorV1{
        std::move(stage),
        std::move(message),
        static_cast<long>(hr)
    };
}

std::string HResultText(HRESULT hr)
{
    std::ostringstream stream;
    stream << "HRESULT 0x" << std::hex
           << static_cast<unsigned long>(hr);
    return stream.str();
}

std::string Utf8FromWide(const wchar_t* value)
{
    if (!value || *value == L'\0') return {};
    const int required = WideCharToMultiByte(
        CP_UTF8, 0, value, -1, nullptr, 0, nullptr, nullptr);
    if (required <= 1) return {};
    std::string result(static_cast<std::size_t>(required), '\0');
    if (WideCharToMultiByte(
            CP_UTF8, 0, value, -1, result.data(), required,
            nullptr, nullptr) != required)
    {
        throw std::runtime_error("WideCharToMultiByte failed.");
    }
    result.resize(static_cast<std::size_t>(required - 1));
    return result;
}

bool SameLuid(const LUID& a, const LUID& b) noexcept
{
    return a.LowPart == b.LowPart && a.HighPart == b.HighPart;
}

base::Digest256 CandidateBindingSetIdentity(
    std::span<const family_execution::FrozenVerifiedCandidateFamilyV1> candidates)
{
    base::BinaryWriter writer;
    writer.WriteBytes(LiteralIdentity(
        "SGE4-5.Spiral4.CandidateBindingSet.V1"));
    writer.WriteU32(static_cast<std::uint32_t>(candidates.size()));
    for (const auto& candidate : candidates)
    {
        writer.WriteBytes(candidate.BindingIdentity());
        writer.WriteBytes(candidate.Verified().Plan().planIdentity);
        writer.WriteBytes(candidate.Verified().CertificateIdentity());
        writer.WriteBytes(candidate.Artifact().ArtifactIdentity());
    }
    return base::Sha256(std::move(writer).Take());
}

base::Digest256 DomainIdentity(
    const semantic::ActiveWorkSemanticV1& semantic,
    const family_verification::CandidateFamilyVerificationContextV1& context,
    const base::Digest256& bindingSet)
{
    base::BinaryWriter writer;
    writer.WriteBytes(LiteralIdentity(
        "SGE4-5.Spiral4.CandidateFamilyDeviceDomain.V1"));
    writer.WriteBytes(semantic::ActiveWorkSemanticIdentityV1(semantic));
    writer.WriteBytes(context.targetProfileIdentity);
    writer.WriteBytes(context.activeCountContractIdentity);
    writer.WriteBytes(context.observationContractIdentity);
    writer.WriteBytes(bindingSet);
    return base::Sha256(std::move(writer).Take());
}

bool CanonicalCandidateSet(
    std::span<const family_execution::FrozenVerifiedCandidateFamilyV1> candidates)
{
    if (candidates.size() != 7) return false;
    using K = family_contract::CandidateFamilyKindV1;
    if (candidates[0].Verified().Plan().kind != K::FixedMaximumGuarded ||
        candidates[0].Verified().Plan().batchSize != 0) return false;
    if (candidates[1].Verified().Plan().kind != K::SingleExecuteIndirectDispatch ||
        candidates[1].Verified().Plan().batchSize != 0) return false;
    constexpr std::uint32_t batchSizes[] = {64, 128, 256, 512, 1024};
    for (std::size_t i = 0; i < 5; ++i)
    {
        if (candidates[i + 2].Verified().Plan().kind !=
                K::BatchedExecuteIndirectDispatch ||
            candidates[i + 2].Verified().Plan().batchSize != batchSizes[i])
        {
            return false;
        }
    }
    return true;
}
}

struct LoadedCandidateFamilyRuntimeV1::Impl final
{
    semantic::ActiveWorkSemanticV1 semantic;
    family_verification::CandidateFamilyVerificationContextV1 context;
    std::vector<family_execution::FrozenVerifiedCandidateFamilyV1> candidates;
    base::Digest256 bindingSetIdentity{};
    base::Digest256 domainIdentity{};

    ComPtr<IDXGIFactory6> factory;
    ComPtr<IDXGIAdapter> adapter;
    ComPtr<ID3D12Device> device;
    std::string adapterDescription;
    LUID activeAdapterLuid{};
    bool hasActiveAdapterLuid = false;
    LUID excludedAdapterLuid{};
    bool hasExcludedAdapterLuid = false;

    CandidateFamilyRuntimeStateV1 state =
        CandidateFamilyRuntimeStateV1::AwaitingAdapter;
    std::uint64_t deviceEpoch = 1;
    std::uint64_t submissionOrdinal = 0;
    bool dynamicInputsBound = false;
    base::Digest256 dynamicBindingIdentity{};

    base::Result<void, CandidateFamilyRuntimeErrorV1> InitializeDevice()
    {
        factory.Reset();
        adapter.Reset();
        device.Reset();
        adapterDescription.clear();
        hasActiveAdapterLuid = false;

        HRESULT hr = CreateDXGIFactory2(0, IID_PPV_ARGS(&factory));
        if (FAILED(hr))
        {
            return base::Result<void, CandidateFamilyRuntimeErrorV1>::Failure(
                Error("candidate-runtime/create-factory", HResultText(hr), hr));
        }

        ComPtr<IDXGIAdapter> candidate;
        hr = factory->EnumWarpAdapter(IID_PPV_ARGS(&candidate));
        if (FAILED(hr))
        {
            return base::Result<void, CandidateFamilyRuntimeErrorV1>::Failure(
                Error("candidate-runtime/enum-warp", HResultText(hr), hr));
        }

        DXGI_ADAPTER_DESC desc{};
        hr = candidate->GetDesc(&desc);
        if (FAILED(hr))
        {
            return base::Result<void, CandidateFamilyRuntimeErrorV1>::Failure(
                Error("candidate-runtime/adapter-desc", HResultText(hr), hr));
        }

        if (hasExcludedAdapterLuid &&
            SameLuid(desc.AdapterLuid, excludedAdapterLuid))
        {
            return base::Result<void, CandidateFamilyRuntimeErrorV1>::Failure(
                Error(
                    "candidate-runtime/no-eligible-adapter",
                    "The only WARP adapter is excluded after actual removal."));
        }

        ComPtr<ID3D12Device> created;
        hr = D3D12CreateDevice(
            candidate.Get(),
            D3D_FEATURE_LEVEL_11_0,
            IID_PPV_ARGS(&created));
        if (FAILED(hr))
        {
            return base::Result<void, CandidateFamilyRuntimeErrorV1>::Failure(
                Error("candidate-runtime/create-device", HResultText(hr), hr));
        }

        adapter = std::move(candidate);
        device = std::move(created);
        adapterDescription = Utf8FromWide(desc.Description);
        activeAdapterLuid = desc.AdapterLuid;
        hasActiveAdapterLuid = true;
        state = CandidateFamilyRuntimeStateV1::Active;
        return base::Result<void, CandidateFamilyRuntimeErrorV1>::Success();
    }

    void ReleaseDeviceObjects()
    {
        device.Reset();
        adapter.Reset();
        factory.Reset();
        adapterDescription.clear();
        hasActiveAdapterLuid = false;
        dynamicInputsBound = false;
        dynamicBindingIdentity = {};
    }
};

LoadedCandidateFamilyRuntimeV1::LoadedCandidateFamilyRuntimeV1(
    std::unique_ptr<Impl> impl)
    : impl_(std::move(impl))
{
}

LoadedCandidateFamilyRuntimeV1::LoadedCandidateFamilyRuntimeV1(
    LoadedCandidateFamilyRuntimeV1&&) noexcept = default;
LoadedCandidateFamilyRuntimeV1&
LoadedCandidateFamilyRuntimeV1::operator=(
    LoadedCandidateFamilyRuntimeV1&&) noexcept = default;
LoadedCandidateFamilyRuntimeV1::~LoadedCandidateFamilyRuntimeV1() = default;

CandidateFamilyRuntimeStateV1
LoadedCandidateFamilyRuntimeV1::State() const noexcept
{
    return impl_->state;
}

std::uint64_t LoadedCandidateFamilyRuntimeV1::DeviceEpoch() const noexcept
{
    return impl_->deviceEpoch;
}

const base::Digest256&
LoadedCandidateFamilyRuntimeV1::DomainIdentity() const noexcept
{
    return impl_->domainIdentity;
}

const base::Digest256&
LoadedCandidateFamilyRuntimeV1::CandidateBindingSetIdentity() const noexcept
{
    return impl_->bindingSetIdentity;
}

const std::string&
LoadedCandidateFamilyRuntimeV1::AdapterDescription() const noexcept
{
    return impl_->adapterDescription;
}

std::size_t LoadedCandidateFamilyRuntimeV1::CandidateCount() const noexcept
{
    return impl_->candidates.size();
}

bool LoadedCandidateFamilyRuntimeV1::DynamicInputsBound() const noexcept
{
    return impl_->dynamicInputsBound;
}

base::Result<LoadedCandidateFamilyRuntimeV1, CandidateFamilyRuntimeErrorV1>
LoadCandidateFamilyRuntimeOnWarpV1(
    const semantic::ActiveWorkSemanticV1& semantic,
    const family_verification::CandidateFamilyVerificationContextV1& context,
    std::span<const family_execution::FrozenVerifiedCandidateFamilyV1> candidates)
{
    if (!CanonicalCandidateSet(candidates))
    {
        return base::Result<LoadedCandidateFamilyRuntimeV1, CandidateFamilyRuntimeErrorV1>::Failure(
            Error(
                "candidate-runtime/corpus",
                "The canonical seven-Candidate family is required."));
    }

    for (const auto& candidate : candidates)
    {
        auto valid = family_verification::ValidateVerifiedCandidateFamilyContextV1(
            candidate.Verified(), semantic, context);
        if (!valid)
        {
            return base::Result<LoadedCandidateFamilyRuntimeV1, CandidateFamilyRuntimeErrorV1>::Failure(
                Error("candidate-runtime/verified", valid.Error()));
        }
    }

    auto impl = std::make_unique<LoadedCandidateFamilyRuntimeV1::Impl>();
    impl->semantic = semantic;
    impl->context = context;
    impl->candidates.assign(candidates.begin(), candidates.end());
    impl->bindingSetIdentity = CandidateBindingSetIdentity(impl->candidates);
    impl->domainIdentity = DomainIdentity(
        impl->semantic, impl->context, impl->bindingSetIdentity);

    auto initialized = impl->InitializeDevice();
    if (!initialized)
    {
        return base::Result<LoadedCandidateFamilyRuntimeV1, CandidateFamilyRuntimeErrorV1>::Failure(
            initialized.Error());
    }

    return base::Result<LoadedCandidateFamilyRuntimeV1, CandidateFamilyRuntimeErrorV1>::Success(
        LoadedCandidateFamilyRuntimeV1(std::move(impl)));
}

base::Result<CandidateFamilyDynamicBindingV1, CandidateFamilyRuntimeErrorV1>
BindCanonicalCandidateFamilyInputsV1(
    LoadedCandidateFamilyRuntimeV1& runtime)
{
    auto& impl = *runtime.impl_;
    if (impl.state != CandidateFamilyRuntimeStateV1::Active || !impl.device)
    {
        return base::Result<CandidateFamilyDynamicBindingV1, CandidateFamilyRuntimeErrorV1>::Failure(
            Error(
                "candidate-runtime/device-state",
                "Dynamic inputs can only be rebound to an Active device epoch."));
    }

    base::BinaryWriter writer;
    writer.WriteBytes(LiteralIdentity(
        "SGE4-5.Spiral4.CanonicalDynamicInputBinding.V1"));
    writer.WriteBytes(impl.domainIdentity);
    writer.WriteBytes(impl.bindingSetIdentity);
    writer.WriteU64(impl.deviceEpoch);
    impl.dynamicBindingIdentity = base::Sha256(std::move(writer).Take());
    impl.dynamicInputsBound = true;

    return base::Result<CandidateFamilyDynamicBindingV1, CandidateFamilyRuntimeErrorV1>::Success(
        CandidateFamilyDynamicBindingV1{
            impl.deviceEpoch,
            impl.dynamicBindingIdentity});
}

CandidateFamilyEpochHandleV1 CaptureCandidateFamilyEpochHandleV1(
    const LoadedCandidateFamilyRuntimeV1& runtime)
{
    return CandidateFamilyEpochHandleV1{
        runtime.impl_->deviceEpoch,
        runtime.impl_->domainIdentity,
        runtime.impl_->bindingSetIdentity};
}

base::Result<void, CandidateFamilyRuntimeErrorV1>
ValidateCandidateFamilyEpochHandleV1(
    const LoadedCandidateFamilyRuntimeV1& runtime,
    const CandidateFamilyEpochHandleV1& handle)
{
    const auto& impl = *runtime.impl_;
    if (impl.state != CandidateFamilyRuntimeStateV1::Active || !impl.device)
    {
        return base::Result<void, CandidateFamilyRuntimeErrorV1>::Failure(
            Error(
                "candidate-runtime/device-state",
                "The Candidate family device domain is not Active."));
    }
    if (handle.deviceEpoch != impl.deviceEpoch)
    {
        return base::Result<void, CandidateFamilyRuntimeErrorV1>::Failure(
            Error(
                "candidate-runtime/stale-epoch",
                "The Candidate family handle belongs to an older device epoch."));
    }
    if (handle.domainIdentity != impl.domainIdentity ||
        handle.candidateBindingSetIdentity != impl.bindingSetIdentity)
    {
        return base::Result<void, CandidateFamilyRuntimeErrorV1>::Failure(
            Error(
                "candidate-runtime/identity",
                "The Candidate family handle belongs to another Frozen domain."));
    }
    return base::Result<void, CandidateFamilyRuntimeErrorV1>::Success();
}

base::Result<CandidateFamilySubmissionV1, CandidateFamilyRuntimeErrorV1>
SubmitCandidateFamilyV1(
    LoadedCandidateFamilyRuntimeV1& runtime,
    const CandidateFamilyEpochHandleV1& handle,
    const CandidateFamilyDynamicBindingV1& binding,
    std::span<const std::uint32_t> activeCounts)
{
    auto validHandle = ValidateCandidateFamilyEpochHandleV1(runtime, handle);
    if (!validHandle)
    {
        return base::Result<CandidateFamilySubmissionV1, CandidateFamilyRuntimeErrorV1>::Failure(
            validHandle.Error());
    }

    auto& impl = *runtime.impl_;
    if (!impl.dynamicInputsBound ||
        binding.deviceEpoch != impl.deviceEpoch ||
        binding.bindingIdentity != impl.dynamicBindingIdentity)
    {
        return base::Result<CandidateFamilySubmissionV1, CandidateFamilyRuntimeErrorV1>::Failure(
            Error(
                "candidate-runtime/rebind-required",
                "Dynamic Motor, Work, and Active Count inputs require explicit current-epoch rebind."));
    }

    auto executed = family_execution::RunVerifiedCandidateFamilyCorpusOnDeviceV1(
        impl.device.Get(),
        impl.adapterDescription,
        impl.semantic,
        impl.context,
        impl.candidates,
        activeCounts);
    if (!executed)
    {
        return base::Result<CandidateFamilySubmissionV1, CandidateFamilyRuntimeErrorV1>::Failure(
            Error(
                executed.Error().stage,
                executed.Error().message,
                static_cast<HRESULT>(executed.Error().hresult)));
    }

    ++impl.submissionOrdinal;
    base::BinaryWriter writer;
    writer.WriteBytes(LiteralIdentity(
        "SGE4-5.Spiral4.CandidateFamilySubmissionToken.V1"));
    writer.WriteBytes(impl.domainIdentity);
    writer.WriteBytes(impl.bindingSetIdentity);
    writer.WriteU64(impl.deviceEpoch);
    writer.WriteU64(impl.submissionOrdinal);
    writer.WriteU32(static_cast<std::uint32_t>(executed.Value().size()));
    for (const auto& candidate : executed.Value())
    {
        writer.WriteBytes(candidate.verifiedBindingIdentity);
        writer.WriteU32(static_cast<std::uint32_t>(candidate.kind));
        writer.WriteU32(candidate.batchSize);
        writer.WriteU32(static_cast<std::uint32_t>(candidate.cases.size()));
        for (const auto& value : candidate.cases)
        {
            writer.WriteU32(value.activeWorkCount);
            writer.WriteBytes(value.outputDigest);
        }
    }

    CandidateFamilySubmissionV1 result;
    result.completion.deviceEpoch = impl.deviceEpoch;
    result.completion.submissionOrdinal = impl.submissionOrdinal;
    result.completion.tokenIdentity = base::Sha256(std::move(writer).Take());
    result.commandSignaturesMaterialized = true;
    result.argumentBuffersRegenerated = true;
    result.endpointStateMaterialized = true;
    result.readbackStateMaterialized = true;
    result.candidates = std::move(executed.Value());
    return base::Result<CandidateFamilySubmissionV1, CandidateFamilyRuntimeErrorV1>::Success(
        std::move(result));
}

base::Result<void, CandidateFamilyRuntimeErrorV1>
ValidateCandidateFamilySubmissionTokenV1(
    const LoadedCandidateFamilyRuntimeV1& runtime,
    const CandidateFamilySubmissionTokenV1& token)
{
    const auto& impl = *runtime.impl_;
    if (impl.state != CandidateFamilyRuntimeStateV1::Active || !impl.device)
    {
        return base::Result<void, CandidateFamilyRuntimeErrorV1>::Failure(
            Error(
                "candidate-runtime/device-state",
                "The submission token belongs to a removed or unavailable device domain."));
    }
    if (token.deviceEpoch != impl.deviceEpoch)
    {
        return base::Result<void, CandidateFamilyRuntimeErrorV1>::Failure(
            Error(
                "candidate-runtime/stale-epoch",
                "The submission token belongs to an older device epoch."));
    }
    if (token.submissionOrdinal == 0 ||
        token.submissionOrdinal > impl.submissionOrdinal ||
        token.tokenIdentity == base::Digest256{})
    {
        return base::Result<void, CandidateFamilyRuntimeErrorV1>::Failure(
            Error(
                "candidate-runtime/submission-token",
                "The submission token is not valid for this domain."));
    }
    return base::Result<void, CandidateFamilyRuntimeErrorV1>::Success();
}

base::Result<CandidateFamilyRecoveryReportV1, CandidateFamilyRuntimeErrorV1>
RecoverCandidateFamilyRuntimeV1(
    LoadedCandidateFamilyRuntimeV1& runtime,
    CandidateFamilyRecoveryModeV1 mode)
{
    auto& impl = *runtime.impl_;
    CandidateFamilyRecoveryReportV1 report;
    report.mode = mode;
    report.stateBefore = impl.state;
    report.previousDeviceEpoch = impl.deviceEpoch;
    report.newDeviceEpoch = impl.deviceEpoch;
    const auto frozenBefore = impl.bindingSetIdentity;

    if (mode == CandidateFamilyRecoveryModeV1::RetryAdapterReacquisition)
    {
        if (impl.state != CandidateFamilyRuntimeStateV1::AwaitingAdapter)
        {
            return base::Result<CandidateFamilyRecoveryReportV1, CandidateFamilyRuntimeErrorV1>::Failure(
                Error(
                    "candidate-runtime/retry-state",
                    "Retry requires AwaitingAdapter state."));
        }
        auto initialized = impl.InitializeDevice();
        if (!initialized)
        {
            if (initialized.Error().stage ==
                "candidate-runtime/no-eligible-adapter")
            {
                impl.state = CandidateFamilyRuntimeStateV1::AwaitingAdapter;
                report.stateAfter = impl.state;
                report.dynamicRebindRequired = true;
                report.frozenIdentityPreserved =
                    frozenBefore == impl.bindingSetIdentity;
                return base::Result<CandidateFamilyRecoveryReportV1, CandidateFamilyRuntimeErrorV1>::Success(report);
            }
            return base::Result<CandidateFamilyRecoveryReportV1, CandidateFamilyRuntimeErrorV1>::Failure(
                initialized.Error());
        }

        ++impl.deviceEpoch;
        impl.dynamicInputsBound = false;
        report.newDeviceEpoch = impl.deviceEpoch;
        report.stateAfter = impl.state;
        report.adapterReacquired = true;
        report.nativeDeviceRematerialized = true;
        report.allRuntimeObjectsRematerialized = false;
        report.commandSignaturesRematerialized = false;
        report.argumentBuffersRematerialized = false;
        report.endpointStateRematerialized = false;
        report.dynamicRebindRequired = true;
        report.frozenIdentityPreserved =
            frozenBefore == impl.bindingSetIdentity;
        return base::Result<CandidateFamilyRecoveryReportV1, CandidateFamilyRuntimeErrorV1>::Success(report);
    }

    if (impl.state != CandidateFamilyRuntimeStateV1::Active || !impl.device)
    {
        return base::Result<CandidateFamilyRecoveryReportV1, CandidateFamilyRuntimeErrorV1>::Failure(
            Error(
                "candidate-runtime/recovery-state",
                "Recovery requires an Active materialized Candidate family domain."));
    }

    if (mode == CandidateFamilyRecoveryModeV1::ControlledRebuild)
    {
        const HRESULT sourceReason = impl.device->GetDeviceRemovedReason();
        if (FAILED(sourceReason))
        {
            return base::Result<CandidateFamilyRecoveryReportV1, CandidateFamilyRuntimeErrorV1>::Failure(
                Error(
                    "candidate-runtime/controlled-source",
                    "Controlled rebuild cannot begin from a removed device.",
                    sourceReason));
        }

        impl.ReleaseDeviceObjects();
        report.allRuntimeObjectsReleased = true;
        auto initialized = impl.InitializeDevice();
        if (!initialized)
        {
            return base::Result<CandidateFamilyRecoveryReportV1, CandidateFamilyRuntimeErrorV1>::Failure(
                initialized.Error());
        }

        ++impl.deviceEpoch;
        impl.dynamicInputsBound = false;
        report.newDeviceEpoch = impl.deviceEpoch;
        report.stateAfter = impl.state;
        report.adapterReacquired = true;
        report.nativeDeviceRematerialized = true;
        report.allRuntimeObjectsRematerialized = false;
        report.commandSignaturesRematerialized = false;
        report.argumentBuffersRematerialized = false;
        report.endpointStateRematerialized = false;
        report.dynamicRebindRequired = true;
        report.frozenIdentityPreserved =
            frozenBefore == impl.bindingSetIdentity;
        return base::Result<CandidateFamilyRecoveryReportV1, CandidateFamilyRuntimeErrorV1>::Success(report);
    }

    if (mode != CandidateFamilyRecoveryModeV1::ForceRemovalForTest)
    {
        return base::Result<CandidateFamilyRecoveryReportV1, CandidateFamilyRuntimeErrorV1>::Failure(
            Error("candidate-runtime/recovery-mode", "Unknown Recovery mode."));
    }

    ComPtr<ID3D12Device5> removable;
    HRESULT hr = impl.device.As(&removable);
    if (FAILED(hr))
    {
        return base::Result<CandidateFamilyRecoveryReportV1, CandidateFamilyRuntimeErrorV1>::Failure(
            Error(
                "candidate-runtime/query-device5",
                HResultText(hr),
                hr));
    }

    removable->RemoveDevice();
    const HRESULT reason = impl.device->GetDeviceRemovedReason();
    report.removalReason = static_cast<long>(reason);
    if (SUCCEEDED(reason))
    {
        return base::Result<CandidateFamilyRecoveryReportV1, CandidateFamilyRuntimeErrorV1>::Failure(
            Error(
                "candidate-runtime/remove-device",
                "ID3D12Device5::RemoveDevice did not remove the device."));
    }

    report.forcedRemoval = true;
    if (impl.hasActiveAdapterLuid)
    {
        impl.excludedAdapterLuid = impl.activeAdapterLuid;
        impl.hasExcludedAdapterLuid = true;
        report.removedAdapterLuidLow = impl.activeAdapterLuid.LowPart;
        report.removedAdapterLuidHigh = impl.activeAdapterLuid.HighPart;
    }

    ComPtr<ID3D12DeviceRemovedExtendedData> dred;
    if (SUCCEEDED(impl.device.As(&dred)))
    {
        D3D12_DRED_AUTO_BREADCRUMBS_OUTPUT breadcrumbs{};
        if (SUCCEEDED(dred->GetAutoBreadcrumbsOutput(&breadcrumbs)))
        {
            for (auto* node = breadcrumbs.pHeadAutoBreadcrumbNode;
                 node;
                 node = node->pNext)
            {
                ++report.dredBreadcrumbNodeCount;
            }
        }

        D3D12_DRED_PAGE_FAULT_OUTPUT pageFault{};
        if (SUCCEEDED(dred->GetPageFaultAllocationOutput(&pageFault)))
        {
            for (auto* node = pageFault.pHeadExistingAllocationNode;
                 node;
                 node = node->pNext)
            {
                ++report.dredPageFaultAllocationCount;
            }
            for (auto* node = pageFault.pHeadRecentFreedAllocationNode;
                 node;
                 node = node->pNext)
            {
                ++report.dredPageFaultAllocationCount;
            }
        }
    }

    impl.ReleaseDeviceObjects();
    impl.state = CandidateFamilyRuntimeStateV1::AwaitingAdapter;
    report.allRuntimeObjectsReleased = true;
    report.stateAfter = impl.state;
    report.dynamicRebindRequired = true;
    report.frozenIdentityPreserved =
        frozenBefore == impl.bindingSetIdentity;
    return base::Result<CandidateFamilyRecoveryReportV1, CandidateFamilyRuntimeErrorV1>::Success(report);
}

std::vector<std::byte> SerializeCandidateFamilyRuntimeAuthorityV1(
    const LoadedCandidateFamilyRuntimeV1& runtime)
{
    const auto& impl = *runtime.impl_;
    base::BinaryWriter writer;
    writer.WriteBytes(LiteralIdentity(
        "SGE4-5.Spiral4.CandidateFamilyRuntimeAuthority.V1"));
    writer.WriteBytes(impl.domainIdentity);
    writer.WriteBytes(impl.bindingSetIdentity);
    writer.WriteBytes(semantic::ActiveWorkSemanticIdentityV1(impl.semantic));
    writer.WriteBytes(impl.context.targetProfileIdentity);
    writer.WriteBytes(impl.context.activeCountContractIdentity);
    writer.WriteBytes(impl.context.observationContractIdentity);
    writer.WriteU32(static_cast<std::uint32_t>(impl.candidates.size()));
    for (const auto& candidate : impl.candidates)
    {
        writer.WriteBytes(candidate.BindingIdentity());
        writer.WriteBytes(candidate.Verified().Plan().planIdentity);
        writer.WriteBytes(candidate.Verified().CertificateIdentity());
        writer.WriteBytes(candidate.Artifact().ArtifactIdentity());
    }
    return std::move(writer).Take();
}
}
