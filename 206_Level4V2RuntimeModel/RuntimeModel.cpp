#include "RuntimeModel.h"

#include <limits>
#include <stdexcept>

namespace sge4_5::v2::runtime_recovery
{
namespace
{
void AppendU8(std::vector<std::byte>& bytes, std::uint8_t value) { bytes.push_back(static_cast<std::byte>(value)); }
void AppendU32(std::vector<std::byte>& bytes, std::uint32_t value)
{
    for (std::uint32_t index = 0; index < sizeof(value); ++index)
        bytes.push_back(static_cast<std::byte>((value >> (index * 8u)) & 0xffu));
}
void AppendU64(std::vector<std::byte>& bytes, std::uint64_t value)
{
    for (std::uint32_t index = 0; index < sizeof(value); ++index)
        bytes.push_back(static_cast<std::byte>((value >> (index * 8u)) & 0xffu));
}
void AppendDigest(std::vector<std::byte>& bytes, const canonical::Digest256V1& digest)
{
    bytes.insert(bytes.end(), digest.begin(), digest.end());
}
template<class Identity>
Identity MakeIdentity(std::string_view domain, std::span<const std::byte> payload)
{
    return canonical::MakeCanonicalIdentityV1<Identity>({domain, RuntimeRecoverySchemaVersionV1, payload});
}
}

AdapterIdentity ComputeAdapterIdentityV1(std::span<const std::byte> canonicalDescriptor)
{
    return MakeIdentity<AdapterIdentity>("SGE.V2.Runtime.Adapter.V1", canonicalDescriptor);
}

RuntimeInstanceIdentity ComputeRuntimeInstanceIdentityV1(
    composition::FrozenCompositionIdentity compositionIdentity,
    AdapterIdentity adapterIdentity,
    canonical::DeviceEpoch deviceEpoch)
{
    std::vector<std::byte> payload;
    AppendDigest(payload, compositionIdentity.Digest());
    AppendDigest(payload, adapterIdentity.Digest());
    AppendU64(payload, deviceEpoch.Value());
    return MakeIdentity<RuntimeInstanceIdentity>("SGE.V2.Runtime.Instance.V1", payload);
}

ExternalBindingSetIdentity ComputeExternalBindingSetIdentityV1(const ExternalBindingSetV1& bindings)
{
    std::vector<ExternalBindingV1> sorted = bindings.bindings;
    std::sort(sorted.begin(), sorted.end(), [](const auto& left, const auto& right) {
        return left.slot.Value() < right.slot.Value();
    });
    std::vector<std::byte> payload;
    AppendDigest(payload, bindings.compositionIdentity.Digest());
    AppendU64(payload, bindings.deviceEpoch.Value());
    AppendU64(payload, static_cast<std::uint64_t>(sorted.size()));
    for (const auto& binding : sorted)
    {
        AppendU32(payload, binding.slot.Value());
        AppendDigest(payload, binding.resourceIdentity.Digest());
        AppendDigest(payload, binding.stateDigest);
    }
    return MakeIdentity<ExternalBindingSetIdentity>("SGE.V2.Runtime.ExternalBindingSet.V1", payload);
}

NativeMaterializationIdentity ComputeNativeMaterializationIdentityV1(
    RuntimeInstanceIdentity runtimeIdentity,
    composition::FrozenCompositionIdentity compositionIdentity,
    AdapterIdentity adapterIdentity,
    canonical::DeviceEpoch deviceEpoch,
    ExternalBindingSetIdentity externalBindingsIdentity)
{
    std::vector<std::byte> payload;
    AppendDigest(payload, runtimeIdentity.Digest());
    AppendDigest(payload, compositionIdentity.Digest());
    AppendDigest(payload, adapterIdentity.Digest());
    AppendU64(payload, deviceEpoch.Value());
    AppendDigest(payload, externalBindingsIdentity.Digest());
    return MakeIdentity<NativeMaterializationIdentity>("SGE.V2.Runtime.Materialization.V1", payload);
}

RuntimeSubmissionIdentity ComputeRuntimeSubmissionIdentityV1(
    RuntimeInstanceIdentity runtimeIdentity,
    dynamic::FrozenDynamicInvocationIdentity invocationIdentity,
    dynamic::DynamicWriteSetIdentity writeSetIdentity,
    canonical::TransitionCount indirectWorkCount,
    canonical::DeviceEpoch deviceEpoch)
{
    std::vector<std::byte> payload;
    AppendDigest(payload, runtimeIdentity.Digest());
    AppendDigest(payload, invocationIdentity.Digest());
    AppendDigest(payload, writeSetIdentity.Digest());
    AppendU32(payload, indirectWorkCount.Value());
    AppendU64(payload, deviceEpoch.Value());
    return MakeIdentity<RuntimeSubmissionIdentity>("SGE.V2.Runtime.Submission.V1", payload);
}

RecoveryOperationIdentity ComputeRecoveryOperationIdentityV1(
    RuntimeInstanceIdentity previousRuntimeIdentity,
    RecoveryModeV1 mode,
    AdapterIdentity previousAdapter,
    std::optional<AdapterIdentity> newAdapter,
    std::uint64_t previousEpoch,
    std::uint64_t newEpoch,
    RuntimeStateV1 finalState)
{
    std::vector<std::byte> payload;
    AppendDigest(payload, previousRuntimeIdentity.Digest());
    AppendU8(payload, static_cast<std::uint8_t>(mode));
    AppendDigest(payload, previousAdapter.Digest());
    AppendU8(payload, newAdapter.has_value() ? 1u : 0u);
    if (newAdapter.has_value()) AppendDigest(payload, newAdapter->Digest());
    AppendU64(payload, previousEpoch);
    AppendU64(payload, newEpoch);
    AppendU8(payload, static_cast<std::uint8_t>(finalState));
    return MakeIdentity<RecoveryOperationIdentity>("SGE.V2.Runtime.RecoveryOperation.V1", payload);
}

ExternalBindingSetV1 MakeExternalBindingSetV1(
    composition::FrozenCompositionIdentity compositionIdentity,
    canonical::DeviceEpoch deviceEpoch,
    std::vector<ExternalBindingV1> bindings)
{
    std::sort(bindings.begin(), bindings.end(), [](const auto& left, const auto& right) {
        return left.slot.Value() < right.slot.Value();
    });
    ExternalBindingSetV1 result{ExternalBindingSetIdentity::FromDigest({}), compositionIdentity, deviceEpoch, std::move(bindings)};
    result.identity = ComputeExternalBindingSetIdentityV1(result);
    return result;
}

bool ContainsAdapterIdentityV1(std::span<const AdapterDescriptorV1> adapters, const AdapterIdentity& identity) noexcept
{
    return std::any_of(adapters.begin(), adapters.end(), [&](const auto& adapter) { return adapter.identity == identity; });
}

bool ContainsExcludedAdapterV1(std::span<const AdapterIdentity> excluded, const AdapterIdentity& identity) noexcept
{
    return std::find(excluded.begin(), excluded.end(), identity) != excluded.end();
}
}
