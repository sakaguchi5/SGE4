#pragma once

#include "../00_Foundation/Result.h"
#include "../00_Foundation/Sha256.h"
#include "../120_ActiveWorkSemantic/ActiveWorkSemantic.h"
#include "../130_ActiveWorkCandidateFamilyVerifier/ActiveWorkCandidateFamilyVerifier.h"
#include "../131_Spiral4CandidateFamilyExecution/CandidateFamilyExecution.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <vector>

namespace sge4_5::spiral4::family_runtime
{
enum class CandidateFamilyRuntimeStateV1 : std::uint8_t
{
    Active = 0,
    AwaitingAdapter = 1
};

enum class CandidateFamilyRecoveryModeV1 : std::uint8_t
{
    ControlledRebuild = 0,
    ForceRemovalForTest = 1,
    RetryAdapterReacquisition = 2
};

struct CandidateFamilyRuntimeErrorV1 final
{
    std::string stage;
    std::string message;
    long hresult = 0;
};

struct CandidateFamilyEpochHandleV1 final
{
    std::uint64_t deviceEpoch = 0;
    base::Digest256 domainIdentity{};
    base::Digest256 candidateBindingSetIdentity{};
};

struct CandidateFamilyDynamicBindingV1 final
{
    std::uint64_t deviceEpoch = 0;
    base::Digest256 bindingIdentity{};
};

struct CandidateFamilySubmissionTokenV1 final
{
    std::uint64_t deviceEpoch = 0;
    std::uint64_t submissionOrdinal = 0;
    base::Digest256 tokenIdentity{};
};

struct CandidateFamilySubmissionV1 final
{
    CandidateFamilySubmissionTokenV1 completion;
    bool commandSignaturesMaterialized = false;
    bool argumentBuffersRegenerated = false;
    bool endpointStateMaterialized = false;
    bool readbackStateMaterialized = false;
    std::vector<family_execution::CandidateFamilyRunResultV1> candidates;
};

struct CandidateFamilyRecoveryReportV1 final
{
    CandidateFamilyRecoveryModeV1 mode =
        CandidateFamilyRecoveryModeV1::ControlledRebuild;
    CandidateFamilyRuntimeStateV1 stateBefore =
        CandidateFamilyRuntimeStateV1::Active;
    CandidateFamilyRuntimeStateV1 stateAfter =
        CandidateFamilyRuntimeStateV1::Active;
    std::uint64_t previousDeviceEpoch = 0;
    std::uint64_t newDeviceEpoch = 0;
    long removalReason = 0;
    std::uint32_t dredBreadcrumbNodeCount = 0;
    std::uint32_t dredPageFaultAllocationCount = 0;
    std::uint32_t removedAdapterLuidLow = 0;
    std::int32_t removedAdapterLuidHigh = 0;
    bool forcedRemoval = false;
    bool adapterReacquired = false;
    bool nativeDeviceRematerialized = false;
    bool allRuntimeObjectsReleased = false;
    bool allRuntimeObjectsRematerialized = false;
    bool commandSignaturesRematerialized = false;
    bool argumentBuffersRematerialized = false;
    bool endpointStateRematerialized = false;
    bool dynamicRebindRequired = false;
    bool frozenIdentityPreserved = false;
};

class LoadedCandidateFamilyRuntimeV1 final
{
public:
    LoadedCandidateFamilyRuntimeV1(LoadedCandidateFamilyRuntimeV1&&) noexcept;
    LoadedCandidateFamilyRuntimeV1& operator=(LoadedCandidateFamilyRuntimeV1&&) noexcept;
    ~LoadedCandidateFamilyRuntimeV1();

    LoadedCandidateFamilyRuntimeV1(
        const LoadedCandidateFamilyRuntimeV1&) = delete;
    LoadedCandidateFamilyRuntimeV1& operator=(
        const LoadedCandidateFamilyRuntimeV1&) = delete;

    [[nodiscard]] CandidateFamilyRuntimeStateV1 State() const noexcept;
    [[nodiscard]] std::uint64_t DeviceEpoch() const noexcept;
    [[nodiscard]] const base::Digest256& DomainIdentity() const noexcept;
    [[nodiscard]] const base::Digest256& CandidateBindingSetIdentity() const noexcept;
    [[nodiscard]] const std::string& AdapterDescription() const noexcept;
    [[nodiscard]] std::size_t CandidateCount() const noexcept;
    [[nodiscard]] bool DynamicInputsBound() const noexcept;

private:
    struct Impl;
    explicit LoadedCandidateFamilyRuntimeV1(std::unique_ptr<Impl> impl);
    std::unique_ptr<Impl> impl_;

    friend base::Result<LoadedCandidateFamilyRuntimeV1, CandidateFamilyRuntimeErrorV1>
    LoadCandidateFamilyRuntimeOnWarpV1(
        const semantic::ActiveWorkSemanticV1&,
        const family_verification::CandidateFamilyVerificationContextV1&,
        std::span<const family_execution::FrozenVerifiedCandidateFamilyV1>);
    friend base::Result<CandidateFamilyDynamicBindingV1, CandidateFamilyRuntimeErrorV1>
    BindCanonicalCandidateFamilyInputsV1(LoadedCandidateFamilyRuntimeV1&);
    friend CandidateFamilyEpochHandleV1 CaptureCandidateFamilyEpochHandleV1(
        const LoadedCandidateFamilyRuntimeV1&);
    friend base::Result<CandidateFamilySubmissionV1, CandidateFamilyRuntimeErrorV1>
    SubmitCandidateFamilyV1(
        LoadedCandidateFamilyRuntimeV1&,
        const CandidateFamilyEpochHandleV1&,
        const CandidateFamilyDynamicBindingV1&,
        std::span<const std::uint32_t>);
    friend base::Result<void, CandidateFamilyRuntimeErrorV1>
    ValidateCandidateFamilyEpochHandleV1(
        const LoadedCandidateFamilyRuntimeV1&,
        const CandidateFamilyEpochHandleV1&);
    friend base::Result<void, CandidateFamilyRuntimeErrorV1>
    ValidateCandidateFamilySubmissionTokenV1(
        const LoadedCandidateFamilyRuntimeV1&,
        const CandidateFamilySubmissionTokenV1&);
    friend base::Result<CandidateFamilyRecoveryReportV1, CandidateFamilyRuntimeErrorV1>
    RecoverCandidateFamilyRuntimeV1(
        LoadedCandidateFamilyRuntimeV1&,
        CandidateFamilyRecoveryModeV1);
    friend std::vector<std::byte>
    SerializeCandidateFamilyRuntimeAuthorityV1(
        const LoadedCandidateFamilyRuntimeV1&);
};

[[nodiscard]] base::Result<
    LoadedCandidateFamilyRuntimeV1,
    CandidateFamilyRuntimeErrorV1>
LoadCandidateFamilyRuntimeOnWarpV1(
    const semantic::ActiveWorkSemanticV1& semantic,
    const family_verification::CandidateFamilyVerificationContextV1& context,
    std::span<const family_execution::FrozenVerifiedCandidateFamilyV1> candidates);

[[nodiscard]] base::Result<
    CandidateFamilyDynamicBindingV1,
    CandidateFamilyRuntimeErrorV1>
BindCanonicalCandidateFamilyInputsV1(
    LoadedCandidateFamilyRuntimeV1& runtime);

[[nodiscard]] CandidateFamilyEpochHandleV1
CaptureCandidateFamilyEpochHandleV1(
    const LoadedCandidateFamilyRuntimeV1& runtime);

[[nodiscard]] base::Result<
    CandidateFamilySubmissionV1,
    CandidateFamilyRuntimeErrorV1>
SubmitCandidateFamilyV1(
    LoadedCandidateFamilyRuntimeV1& runtime,
    const CandidateFamilyEpochHandleV1& handle,
    const CandidateFamilyDynamicBindingV1& binding,
    std::span<const std::uint32_t> activeCounts);

[[nodiscard]] base::Result<void, CandidateFamilyRuntimeErrorV1>
ValidateCandidateFamilyEpochHandleV1(
    const LoadedCandidateFamilyRuntimeV1& runtime,
    const CandidateFamilyEpochHandleV1& handle);

[[nodiscard]] base::Result<void, CandidateFamilyRuntimeErrorV1>
ValidateCandidateFamilySubmissionTokenV1(
    const LoadedCandidateFamilyRuntimeV1& runtime,
    const CandidateFamilySubmissionTokenV1& token);

[[nodiscard]] base::Result<
    CandidateFamilyRecoveryReportV1,
    CandidateFamilyRuntimeErrorV1>
RecoverCandidateFamilyRuntimeV1(
    LoadedCandidateFamilyRuntimeV1& runtime,
    CandidateFamilyRecoveryModeV1 mode);

[[nodiscard]] std::vector<std::byte>
SerializeCandidateFamilyRuntimeAuthorityV1(
    const LoadedCandidateFamilyRuntimeV1& runtime);
}
