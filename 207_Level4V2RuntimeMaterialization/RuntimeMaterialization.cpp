#include "RuntimeMaterialization.h"

namespace sge4_5::v2::runtime_recovery
{
namespace
{
canonical::ResourceInstanceIdentity MakeResourceInstanceIdentity(
    RuntimeInstanceIdentity runtimeIdentity,
    std::uint8_t kind)
{
    std::vector<std::byte> payload(runtimeIdentity.Digest().begin(), runtimeIdentity.Digest().end());
    payload.push_back(static_cast<std::byte>(kind));
    return canonical::MakeCanonicalIdentityV1<canonical::ResourceInstanceIdentity>(
        {"SGE.V2.Runtime.ResourceInstance.V1", RuntimeRecoverySchemaVersionV1, payload});
}

template<class Handle>
RuntimeErrorV1 ValidateHandle(const LoadedRuntimeV1& loaded, const Handle& handle) noexcept
{
    if (loaded.State() != RuntimeStateV1::Active && loaded.State() != RuntimeStateV1::ActiveNeedsExternalRebind)
        return RuntimeErrorV1::RuntimeNotActive;
    return handle.Epoch() == loaded.Epoch() ? RuntimeErrorV1::None : RuntimeErrorV1::StaleHandle;
}
}

RuntimeMaterializationResultV1 MaterializeRuntimeV1(
    const frozen_composition::OpaqueFrozenCompositionV1& composition,
    const AdapterIdentity& explicitAdapter,
    IRuntimeBackendV1& backend)
{
    const auto adapters = backend.EnumerateAdapters();
    if (!ContainsAdapterIdentityV1(adapters, explicitAdapter))
        return {RuntimeErrorV1::AdapterNotEnumerated, std::nullopt};
    if (!backend.AcquireAdapter(explicitAdapter))
        return {RuntimeErrorV1::AdapterAcquireFailed, std::nullopt};
    const auto epoch = canonical::DeviceEpoch::TryCreate(1u);
    if (!epoch) return {RuntimeErrorV1::MaterializationFailed, std::nullopt};
    NativeMaterializationRequestV1 request{
        composition.Identity(), explicitAdapter, *epoch, composition.LeafCount(), composition.FlowCount()};
    if (!backend.Materialize(request))
    {
        backend.ReleaseAllRuntimeObjects();
        return {RuntimeErrorV1::MaterializationFailed, std::nullopt};
    }
    const auto runtimeIdentity = ComputeRuntimeInstanceIdentityV1(composition.Identity(), explicitAdapter, *epoch);
    auto representation = canonical::RepresentationHandleV1(MakeResourceInstanceIdentity(runtimeIdentity, 1u), *epoch);
    auto history = canonical::HistoryHandleV1(MakeResourceInstanceIdentity(runtimeIdentity, 2u), *epoch);
    return {RuntimeErrorV1::None, LoadedRuntimeV1(composition, explicitAdapter, *epoch, runtimeIdentity,
        std::move(representation), std::move(history), backend)};
}

RuntimeErrorV1 RebindExternalStateV1(LoadedRuntimeV1& loaded, ExternalBindingSetV1 bindings)
{
    if (loaded.state_ != RuntimeStateV1::ActiveNeedsExternalRebind && loaded.state_ != RuntimeStateV1::Active)
        return RuntimeErrorV1::RuntimeNotActive;
    if (bindings.compositionIdentity != loaded.composition_.Identity())
        return RuntimeErrorV1::ExternalBindingCompositionMismatch;
    if (bindings.deviceEpoch != loaded.deviceEpoch_)
        return RuntimeErrorV1::ExternalBindingEpochMismatch;
    for (std::size_t index = 1; index < bindings.bindings.size(); ++index)
        if (bindings.bindings[index - 1].slot == bindings.bindings[index].slot)
            return RuntimeErrorV1::ExternalBindingDuplicateSlot;
    if (ComputeExternalBindingSetIdentityV1(bindings) != bindings.identity)
        return RuntimeErrorV1::ExternalRebindFailed;
    if (!loaded.backend_->RebindExternalState(bindings))
        return RuntimeErrorV1::ExternalRebindFailed;
    loaded.externalBindings_ = std::move(bindings);
    loaded.state_ = RuntimeStateV1::Active;
    return RuntimeErrorV1::None;
}

RuntimeErrorV1 ValidateRuntimeHandleEpochV1(const LoadedRuntimeV1& loaded, const canonical::RepresentationHandleV1& handle) noexcept { return ValidateHandle(loaded, handle); }
RuntimeErrorV1 ValidateRuntimeHandleEpochV1(const LoadedRuntimeV1& loaded, const canonical::HistoryHandleV1& handle) noexcept { return ValidateHandle(loaded, handle); }
RuntimeErrorV1 ValidateRuntimeHandleEpochV1(const LoadedRuntimeV1& loaded, const canonical::CompletionHandleV1& handle) noexcept { return ValidateHandle(loaded, handle); }
RuntimeErrorV1 ValidateRuntimeHandleEpochV1(const LoadedRuntimeV1& loaded, const canonical::SubmissionHandleV1& handle) noexcept { return ValidateHandle(loaded, handle); }
}
