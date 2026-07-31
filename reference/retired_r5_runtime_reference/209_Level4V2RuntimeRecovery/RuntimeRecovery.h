#pragma once

#include "../208_Level4V2RuntimeSubmission/RuntimeSubmission.h"

namespace sge4_5::v2::runtime_recovery
{
struct RecoveryReportV1 final
{
    RecoveryOperationIdentity identity;
    RecoveryModeV1 mode;
    RuntimeStateV1 finalState;
    std::uint64_t previousEpoch = 0;
    std::uint64_t newEpoch = 0;
    AdapterIdentity previousAdapter;
    std::optional<AdapterIdentity> newAdapter;
    bool allRuntimeObjectsReleased = false;
    bool allRuntimeObjectsRematerialized = false;
    bool externalRebindRequired = false;
    bool recoverySeedRequired = false;
};

struct RecoveryResultV1 final
{
    RuntimeErrorV1 error = RuntimeErrorV1::None;
    std::optional<RecoveryReportV1> report;
    [[nodiscard]] bool Accepted() const noexcept { return error == RuntimeErrorV1::None && report.has_value(); }
};

class RuntimeRecoveryCoordinatorV1 final
{
public:
    [[nodiscard]] static RecoveryResultV1 ControlledRebuild(LoadedRuntimeV1& loaded);
    [[nodiscard]] static RecoveryResultV1 ForceRemovalForQualification(LoadedRuntimeV1& loaded);
    [[nodiscard]] static RecoveryResultV1 RetryAdapterReacquisition(
        LoadedRuntimeV1& loaded,
        std::span<const AdapterIdentity> explicitlyOrderedCandidates);
private:
    [[nodiscard]] static RuntimeErrorV1 RematerializeOnAdapter(LoadedRuntimeV1&, const AdapterIdentity&);
};
}
