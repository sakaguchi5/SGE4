#pragma once

#include "../00_Foundation/Result.h"
#include "../00_Foundation/Sha256.h"
#include "../172_SparseTemporalDeltaSemantic/SparseTemporalDeltaSemantic.h"
#include "../180_SparseTemporalDeltaCandidateFamilyVerifier/DeltaCandidateFamilyVerifier.h"
#include "../181_Spiral7DeltaFamilyExecution/Spiral7DeltaFamilyExecution.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <vector>

namespace sge4_5::spiral7::architecture_runtime
{
enum class DeltaFamilyRuntimeStateV1 : std::uint8_t
{
    Active = 0,
    AwaitingAdapter = 1
};

enum class DeltaFamilyRecoveryModeV1 : std::uint8_t
{
    ControlledRebuild = 0,
    ForceRemovalForTest = 1,
    RetryAdapterReacquisition = 2
};

struct DeltaFamilyRuntimeErrorV1 final
{
    std::string stage;
    std::string message;
    long nativeCode = 0;
};

struct DeltaRuntimeEpochHandleV1 final
{
    std::uint64_t deviceEpoch = 0;
    base::Digest256 domainIdentity{};
    base::Digest256 authorityIdentity{};
};

struct DeltaTimelineBindingV1 final
{
    std::uint64_t deviceEpoch = 0;
    std::uint32_t invocationCount = 0;
    base::Digest256 firstInvocationIdentity{};
    base::Digest256 lastInvocationIdentity{};
    base::Digest256 bindingIdentity{};
};

struct DeltaRepresentationSetHandleV1 final
{
    std::uint64_t deviceEpoch = 0;
    std::uint32_t invocationCount = 0;
    base::Digest256 representationSetIdentity{};
};

struct DeltaHistoryHandleV1 final
{
    std::uint64_t deviceEpoch = 0;
    std::uint32_t invocationIndex = 0;
    base::Digest256 activeSetIdentity{};
    base::Digest256 outputDigest{};
    base::Digest256 historyIdentity{};
};

struct DeltaRuntimeSubmissionTokenV1 final
{
    std::uint64_t deviceEpoch = 0;
    std::uint64_t submissionOrdinal = 0;
    std::uint32_t invocationCount = 0;
    base::Digest256 tokenIdentity{};
};

struct DeltaRuntimeSubmissionV1 final
{
    DeltaRuntimeSubmissionTokenV1 completion;
    DeltaHistoryHandleV1 history;
    DeltaRepresentationSetHandleV1 representations;
    bool representationResourcesMaterialized = false;
    bool indirectArgumentsMaterialized = false;
    bool historyStateMaterialized = false;
    bool completionStateMaterialized = false;
    bool readbackStateMaterialized = false;
    family_execution::DeltaFamilyArchitectureReportV1 architecture;
};

struct DeltaRecoverySeedBindingV1 final
{
    std::uint64_t deviceEpoch = 0;
    std::uint32_t semanticInvocationIndex = 0;
    base::Digest256 semanticRecoveryInvocationIdentity{};
    base::Digest256 initializationSeedInvocationIdentity{};
    base::Digest256 executionRecoveryInvocationIdentity{};
    base::Digest256 activeSetIdentity{};
    base::Digest256 modifiedSetIdentity{};
    base::Digest256 bindingIdentity{};
};

struct DeltaRecoveryRepresentationSetHandleV1 final
{
    std::uint64_t deviceEpoch = 0;
    base::Digest256 initializationSeedInvocationIdentity{};
    base::Digest256 executionRecoveryInvocationIdentity{};
    base::Digest256 representationSetIdentity{};
};

struct DeltaFamilyRecoveryReportV1 final
{
    DeltaFamilyRecoveryModeV1 mode = DeltaFamilyRecoveryModeV1::ControlledRebuild;
    DeltaFamilyRuntimeStateV1 stateBefore = DeltaFamilyRuntimeStateV1::Active;
    DeltaFamilyRuntimeStateV1 stateAfter = DeltaFamilyRuntimeStateV1::Active;
    std::uint64_t previousDeviceEpoch = 0;
    std::uint64_t newDeviceEpoch = 0;
    long removalReason = 0;
    std::uint32_t removedAdapterLuidLow = 0;
    std::int32_t removedAdapterLuidHigh = 0;
    bool forcedRemoval = false;
    bool adapterReacquired = false;
    bool nativeDeviceRematerialized = false;
    bool allRuntimeObjectsReleased = false;
    bool executionContextRematerialized = false;
    bool representationResourcesInvalidated = false;
    bool indirectArgumentsInvalidated = false;
    bool historyStateInvalidated = false;
    bool completionStateInvalidated = false;
    bool readbackStateInvalidated = false;
    bool externalSetRebindRequired = false;
    bool explicitRepresentationRebuildRequired = false;
    bool forcedFullActiveSeedRequired = false;
    bool frozenAuthorityPreserved = false;
};

class LoadedDeltaFamilyRuntimeV1 final
{
public:
    LoadedDeltaFamilyRuntimeV1(LoadedDeltaFamilyRuntimeV1&&) noexcept;
    LoadedDeltaFamilyRuntimeV1& operator=(LoadedDeltaFamilyRuntimeV1&&) noexcept;
    ~LoadedDeltaFamilyRuntimeV1();

    LoadedDeltaFamilyRuntimeV1(const LoadedDeltaFamilyRuntimeV1&) = delete;
    LoadedDeltaFamilyRuntimeV1& operator=(const LoadedDeltaFamilyRuntimeV1&) = delete;

    [[nodiscard]] DeltaFamilyRuntimeStateV1 State() const noexcept;
    [[nodiscard]] std::uint64_t DeviceEpoch() const noexcept;
    [[nodiscard]] const base::Digest256& DomainIdentity() const noexcept;
    [[nodiscard]] const base::Digest256& AuthorityIdentity() const noexcept;
    [[nodiscard]] const std::string& AdapterDescription() const noexcept;
    [[nodiscard]] std::size_t AuthorityInvocationCount() const noexcept;
    [[nodiscard]] bool TimelineBound() const noexcept;
    [[nodiscard]] bool RepresentationsBuilt() const noexcept;
    [[nodiscard]] bool RecoverySeedRequired() const noexcept;

private:
    struct Impl;
    explicit LoadedDeltaFamilyRuntimeV1(std::unique_ptr<Impl> impl);
    std::unique_ptr<Impl> impl_;

    friend base::Result<LoadedDeltaFamilyRuntimeV1, DeltaFamilyRuntimeErrorV1>
    LoadDeltaFamilyRuntimeOnWarpV1(
        const semantic::SparseTemporalDeltaSemanticV1&,
        const family_verification::DeltaFamilyVerificationContextV1&,
        std::span<const family_execution::VerifiedDeltaFamilyCaseV1>);
    friend DeltaRuntimeEpochHandleV1 CaptureDeltaRuntimeEpochHandleV1(
        const LoadedDeltaFamilyRuntimeV1&);
    friend base::Result<void, DeltaFamilyRuntimeErrorV1>
    ValidateDeltaRuntimeEpochHandleV1(
        const LoadedDeltaFamilyRuntimeV1&,
        const DeltaRuntimeEpochHandleV1&);
    friend base::Result<DeltaTimelineBindingV1, DeltaFamilyRuntimeErrorV1>
    BindDeltaTimelinePrefixV1(
        LoadedDeltaFamilyRuntimeV1&,
        const DeltaRuntimeEpochHandleV1&,
        std::uint32_t);
    friend base::Result<DeltaRepresentationSetHandleV1, DeltaFamilyRuntimeErrorV1>
    RebuildDeltaTimelineRepresentationsV1(
        LoadedDeltaFamilyRuntimeV1&,
        const DeltaRuntimeEpochHandleV1&,
        const DeltaTimelineBindingV1&);
    friend base::Result<DeltaRuntimeSubmissionV1, DeltaFamilyRuntimeErrorV1>
    SubmitDeltaTimelinePrefixV1(
        LoadedDeltaFamilyRuntimeV1&,
        const DeltaRuntimeEpochHandleV1&,
        const DeltaTimelineBindingV1&,
        const DeltaRepresentationSetHandleV1&);
    friend base::Result<void, DeltaFamilyRuntimeErrorV1>
    ValidateDeltaRepresentationSetHandleV1(
        const LoadedDeltaFamilyRuntimeV1&,
        const DeltaRepresentationSetHandleV1&);
    friend base::Result<void, DeltaFamilyRuntimeErrorV1>
    ValidateDeltaHistoryHandleV1(
        const LoadedDeltaFamilyRuntimeV1&,
        const DeltaHistoryHandleV1&);
    friend base::Result<void, DeltaFamilyRuntimeErrorV1>
    ValidateDeltaRuntimeSubmissionTokenV1(
        const LoadedDeltaFamilyRuntimeV1&,
        const DeltaRuntimeSubmissionTokenV1&);
    friend base::Result<DeltaFamilyRecoveryReportV1, DeltaFamilyRuntimeErrorV1>
    RecoverDeltaFamilyRuntimeV1(
        LoadedDeltaFamilyRuntimeV1&,
        DeltaFamilyRecoveryModeV1);
    friend base::Result<DeltaRecoverySeedBindingV1, DeltaFamilyRuntimeErrorV1>
    BindDeltaRecoverySeedV1(
        LoadedDeltaFamilyRuntimeV1&,
        const DeltaRuntimeEpochHandleV1&,
        const semantic::SparseDeltaInvocationV1&,
        const family_execution::VerifiedDeltaFamilyCaseV1&,
        const family_execution::VerifiedDeltaFamilyCaseV1&);
    friend base::Result<DeltaRecoveryRepresentationSetHandleV1, DeltaFamilyRuntimeErrorV1>
    RebuildDeltaRecoverySeedRepresentationsV1(
        LoadedDeltaFamilyRuntimeV1&,
        const DeltaRuntimeEpochHandleV1&,
        const DeltaRecoverySeedBindingV1&,
        const family_execution::VerifiedDeltaFamilyCaseV1&,
        const family_execution::VerifiedDeltaFamilyCaseV1&);
    friend base::Result<DeltaRuntimeSubmissionV1, DeltaFamilyRuntimeErrorV1>
    SubmitDeltaRecoverySeedV1(
        LoadedDeltaFamilyRuntimeV1&,
        const DeltaRuntimeEpochHandleV1&,
        const DeltaRecoverySeedBindingV1&,
        const DeltaRecoveryRepresentationSetHandleV1&,
        const family_execution::VerifiedDeltaFamilyCaseV1&,
        const family_execution::VerifiedDeltaFamilyCaseV1&);
    friend std::vector<std::byte> SerializeDeltaFamilyRuntimeAuthorityV1(
        const LoadedDeltaFamilyRuntimeV1&);
};

[[nodiscard]] base::Result<LoadedDeltaFamilyRuntimeV1, DeltaFamilyRuntimeErrorV1>
LoadDeltaFamilyRuntimeOnWarpV1(
    const semantic::SparseTemporalDeltaSemanticV1& semantic,
    const family_verification::DeltaFamilyVerificationContextV1& context,
    std::span<const family_execution::VerifiedDeltaFamilyCaseV1> authorityCases);

[[nodiscard]] DeltaRuntimeEpochHandleV1 CaptureDeltaRuntimeEpochHandleV1(
    const LoadedDeltaFamilyRuntimeV1& runtime);
[[nodiscard]] base::Result<void, DeltaFamilyRuntimeErrorV1>
ValidateDeltaRuntimeEpochHandleV1(
    const LoadedDeltaFamilyRuntimeV1& runtime,
    const DeltaRuntimeEpochHandleV1& handle);
[[nodiscard]] base::Result<DeltaTimelineBindingV1, DeltaFamilyRuntimeErrorV1>
BindDeltaTimelinePrefixV1(
    LoadedDeltaFamilyRuntimeV1& runtime,
    const DeltaRuntimeEpochHandleV1& handle,
    std::uint32_t invocationCount);
[[nodiscard]] base::Result<DeltaRepresentationSetHandleV1, DeltaFamilyRuntimeErrorV1>
RebuildDeltaTimelineRepresentationsV1(
    LoadedDeltaFamilyRuntimeV1& runtime,
    const DeltaRuntimeEpochHandleV1& handle,
    const DeltaTimelineBindingV1& binding);
[[nodiscard]] base::Result<DeltaRuntimeSubmissionV1, DeltaFamilyRuntimeErrorV1>
SubmitDeltaTimelinePrefixV1(
    LoadedDeltaFamilyRuntimeV1& runtime,
    const DeltaRuntimeEpochHandleV1& handle,
    const DeltaTimelineBindingV1& binding,
    const DeltaRepresentationSetHandleV1& representations);

[[nodiscard]] base::Result<void, DeltaFamilyRuntimeErrorV1>
ValidateDeltaRepresentationSetHandleV1(
    const LoadedDeltaFamilyRuntimeV1& runtime,
    const DeltaRepresentationSetHandleV1& handle);
[[nodiscard]] base::Result<void, DeltaFamilyRuntimeErrorV1>
ValidateDeltaHistoryHandleV1(
    const LoadedDeltaFamilyRuntimeV1& runtime,
    const DeltaHistoryHandleV1& handle);
[[nodiscard]] base::Result<void, DeltaFamilyRuntimeErrorV1>
ValidateDeltaRuntimeSubmissionTokenV1(
    const LoadedDeltaFamilyRuntimeV1& runtime,
    const DeltaRuntimeSubmissionTokenV1& token);

[[nodiscard]] base::Result<DeltaFamilyRecoveryReportV1, DeltaFamilyRuntimeErrorV1>
RecoverDeltaFamilyRuntimeV1(
    LoadedDeltaFamilyRuntimeV1& runtime,
    DeltaFamilyRecoveryModeV1 mode);

[[nodiscard]] base::Result<DeltaRecoverySeedBindingV1, DeltaFamilyRuntimeErrorV1>
BindDeltaRecoverySeedV1(
    LoadedDeltaFamilyRuntimeV1& runtime,
    const DeltaRuntimeEpochHandleV1& handle,
    const semantic::SparseDeltaInvocationV1& semanticRecoveryInvocation,
    const family_execution::VerifiedDeltaFamilyCaseV1& initializationSeedCase,
    const family_execution::VerifiedDeltaFamilyCaseV1& executionRecoveryCase);
[[nodiscard]] base::Result<DeltaRecoveryRepresentationSetHandleV1, DeltaFamilyRuntimeErrorV1>
RebuildDeltaRecoverySeedRepresentationsV1(
    LoadedDeltaFamilyRuntimeV1& runtime,
    const DeltaRuntimeEpochHandleV1& handle,
    const DeltaRecoverySeedBindingV1& binding,
    const family_execution::VerifiedDeltaFamilyCaseV1& initializationSeedCase,
    const family_execution::VerifiedDeltaFamilyCaseV1& executionRecoveryCase);
[[nodiscard]] base::Result<DeltaRuntimeSubmissionV1, DeltaFamilyRuntimeErrorV1>
SubmitDeltaRecoverySeedV1(
    LoadedDeltaFamilyRuntimeV1& runtime,
    const DeltaRuntimeEpochHandleV1& handle,
    const DeltaRecoverySeedBindingV1& binding,
    const DeltaRecoveryRepresentationSetHandleV1& representations,
    const family_execution::VerifiedDeltaFamilyCaseV1& initializationSeedCase,
    const family_execution::VerifiedDeltaFamilyCaseV1& executionRecoveryCase);

[[nodiscard]] std::vector<std::byte> SerializeDeltaFamilyRuntimeAuthorityV1(
    const LoadedDeltaFamilyRuntimeV1& runtime);
[[nodiscard]] std::vector<std::byte> SerializeDeltaRuntimeSubmissionV1(
    const DeltaRuntimeSubmissionV1& submission);
[[nodiscard]] std::vector<std::byte> SerializeDeltaFamilyRecoveryReportV1(
    const DeltaFamilyRecoveryReportV1& report);
} // namespace sge4_5::spiral7::architecture_runtime
