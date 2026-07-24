#include "CompositionModel.h"

#include <algorithm>
#include <limits>
#include <stdexcept>

namespace sge4_5::v2::composition
{
namespace
{
void AppendU8(std::vector<std::byte>& bytes, std::uint8_t value) { bytes.push_back(static_cast<std::byte>(value)); }
void AppendU32(std::vector<std::byte>& bytes, std::uint32_t value)
{
    for (std::uint32_t i=0;i<sizeof(value);++i) bytes.push_back(static_cast<std::byte>((value>>(i*8u))&0xffu));
}
void AppendU64(std::vector<std::byte>& bytes, std::uint64_t value)
{
    for (std::uint32_t i=0;i<sizeof(value);++i) bytes.push_back(static_cast<std::byte>((value>>(i*8u))&0xffu));
}
void AppendBool(std::vector<std::byte>& bytes, bool value) { AppendU8(bytes, value ? 1u : 0u); }
void AppendDigest(std::vector<std::byte>& bytes, const canonical::Digest256V1& digest)
{
    bytes.insert(bytes.end(), digest.begin(), digest.end());
}
void AppendPayload(std::vector<std::byte>& bytes, std::span<const std::byte> payload)
{
    if constexpr(sizeof(std::size_t)>sizeof(std::uint64_t))
        if(payload.size()>static_cast<std::size_t>(std::numeric_limits<std::uint64_t>::max())) throw std::invalid_argument("Payload too large.");
    AppendU64(bytes, static_cast<std::uint64_t>(payload.size()));
    bytes.insert(bytes.end(), payload.begin(), payload.end());
}
template<class Identity>
Identity MakeIdentity(std::string_view domain, std::span<const std::byte> payload)
{
    return canonical::MakeCanonicalIdentityV1<Identity>({domain, CompositionModelSchemaVersionV1, payload});
}
template<class Identity>
bool IdentityLess(const Identity& a, const Identity& b) { return a.Digest() < b.Digest(); }
}

LeafAuthorityV1 MakeLeafAuthorityV1(std::uint32_t keySchemaVersion, std::span<const std::byte> canonicalKey, const frozen_authority::OpaqueFrozenArtifactV1& artifact)
{
    std::vector<std::byte> key(canonicalKey.begin(), canonicalKey.end());
    const auto& descriptor=artifact.Descriptor();
    std::vector<std::byte> payload;
    AppendU32(payload,keySchemaVersion); AppendPayload(payload,key);
    AppendDigest(payload,descriptor.identity.Digest()); AppendDigest(payload,descriptor.verifiedPlanIdentity.Digest());
    AppendDigest(payload,descriptor.targetProfileIdentity.Digest()); AppendDigest(payload,descriptor.resourceContractIdentity.Digest());
    auto identity=MakeIdentity<LeafIdentity>("SGE.V2.Composition.Leaf.V1",payload);
    return LeafAuthorityV1(identity,descriptor.identity,descriptor.verifiedPlanIdentity,descriptor.targetProfileIdentity,descriptor.resourceContractIdentity,keySchemaVersion,std::move(key));
}

EndpointAuthorityV1 MakeEndpointAuthorityV1(const LeafAuthorityV1& leaf,std::uint32_t keySchemaVersion,std::span<const std::byte> canonicalKey,ResourceKindV1 resourceKind,EndpointAccessV1 access,std::uint64_t minimumBytes,ResourceStateV1 incomingState,ResourceStateV1 outgoingState,bool requiresSynchronization,bool required,bool presenter)
{
    std::vector<std::byte> key(canonicalKey.begin(),canonicalKey.end());
    std::vector<std::byte> payload;
    AppendU32(payload,keySchemaVersion); AppendPayload(payload,key); AppendDigest(payload,leaf.Identity().Digest());
    AppendDigest(payload,leaf.ResourceContractIdentityValue().Digest()); AppendU8(payload,static_cast<std::uint8_t>(resourceKind));
    AppendU8(payload,static_cast<std::uint8_t>(access)); AppendU64(payload,minimumBytes); AppendU8(payload,static_cast<std::uint8_t>(incomingState));
    AppendU8(payload,static_cast<std::uint8_t>(outgoingState)); AppendBool(payload,requiresSynchronization); AppendBool(payload,required); AppendBool(payload,presenter);
    auto identity=MakeIdentity<EndpointIdentity>("SGE.V2.Composition.Endpoint.V1",payload);
    return EndpointAuthorityV1(identity,leaf.Identity(),leaf.ResourceContractIdentityValue(),resourceKind,access,minimumBytes,incomingState,outgoingState,requiresSynchronization,required,presenter,keySchemaVersion,std::move(key));
}

FlowDeclarationV1 MakeFlowDeclarationV1(std::uint32_t keySchemaVersion,std::span<const std::byte> canonicalKey,FlowBoundaryV1 boundary,std::span<const EndpointIdentity> endpoints)
{
    std::vector<std::byte> key(canonicalKey.begin(),canonicalKey.end());
    std::vector<EndpointIdentity> sorted(endpoints.begin(),endpoints.end());
    std::sort(sorted.begin(),sorted.end(),IdentityLess<EndpointIdentity>);
    std::vector<std::byte> payload; AppendU32(payload,keySchemaVersion); AppendPayload(payload,key); AppendU8(payload,static_cast<std::uint8_t>(boundary));
    AppendU64(payload,static_cast<std::uint64_t>(sorted.size())); for(const auto& e:sorted) AppendDigest(payload,e.Digest());
    auto identity=MakeIdentity<FlowIdentity>("SGE.V2.Composition.Flow.V1",payload);
    return {identity,keySchemaVersion,std::move(key),boundary,std::vector<EndpointIdentity>(endpoints.begin(),endpoints.end())};
}

RawCompositionContractV1 MakeRawCompositionContractV1(std::vector<LeafAuthorityV1> leaves,std::vector<EndpointAuthorityV1> endpoints,std::vector<FlowDeclarationV1> flows)
{
    RawCompositionContractV1 result{CompositionContractIdentity::FromDigest({}),std::move(leaves),std::move(endpoints),std::move(flows),RecoveryScopeV1::WholeComposition};
    result.identity=ComputeCompositionContractIdentityV1(result); return result;
}

LeafIdentity ComputeLeafIdentityV1(const LeafAuthorityV1& leaf)
{
    std::vector<std::byte> payload; AppendU32(payload,leaf.keySchemaVersion_); AppendPayload(payload,leaf.canonicalKey_);
    AppendDigest(payload,leaf.frozenArtifactIdentity_.Digest()); AppendDigest(payload,leaf.verifiedPlanIdentity_.Digest());
    AppendDigest(payload,leaf.targetProfileIdentity_.Digest()); AppendDigest(payload,leaf.resourceContractIdentity_.Digest());
    return MakeIdentity<LeafIdentity>("SGE.V2.Composition.Leaf.V1",payload);
}
EndpointIdentity ComputeEndpointIdentityV1(const EndpointAuthorityV1& e)
{
    std::vector<std::byte> payload; AppendU32(payload,e.keySchemaVersion_); AppendPayload(payload,e.canonicalKey_); AppendDigest(payload,e.leafIdentity_.Digest());
    AppendDigest(payload,e.resourceContractIdentity_.Digest()); AppendU8(payload,static_cast<std::uint8_t>(e.resourceKind_)); AppendU8(payload,static_cast<std::uint8_t>(e.access_));
    AppendU64(payload,e.minimumBytes_); AppendU8(payload,static_cast<std::uint8_t>(e.incomingState_)); AppendU8(payload,static_cast<std::uint8_t>(e.outgoingState_));
    AppendBool(payload,e.requiresSynchronization_); AppendBool(payload,e.required_); AppendBool(payload,e.presenter_);
    return MakeIdentity<EndpointIdentity>("SGE.V2.Composition.Endpoint.V1",payload);
}
FlowIdentity ComputeFlowIdentityV1(const FlowDeclarationV1& flow)
{
    auto sorted=flow.endpoints; std::sort(sorted.begin(),sorted.end(),IdentityLess<EndpointIdentity>);
    std::vector<std::byte> payload; AppendU32(payload,flow.keySchemaVersion); AppendPayload(payload,flow.canonicalKey); AppendU8(payload,static_cast<std::uint8_t>(flow.boundary));
    AppendU64(payload,static_cast<std::uint64_t>(sorted.size())); for(const auto& e:sorted) AppendDigest(payload,e.Digest());
    return MakeIdentity<FlowIdentity>("SGE.V2.Composition.Flow.V1",payload);
}
CompositionContractIdentity ComputeCompositionContractIdentityV1(const RawCompositionContractV1& contract)
{
    std::vector<LeafIdentity> leaves; for(const auto& x:contract.leaves) leaves.push_back(x.Identity());
    std::vector<EndpointIdentity> endpoints; for(const auto& x:contract.endpoints) endpoints.push_back(x.Identity());
    std::vector<FlowIdentity> flows; for(const auto& x:contract.flows) flows.push_back(x.identity);
    std::sort(leaves.begin(),leaves.end(),IdentityLess<LeafIdentity>); std::sort(endpoints.begin(),endpoints.end(),IdentityLess<EndpointIdentity>); std::sort(flows.begin(),flows.end(),IdentityLess<FlowIdentity>);
    std::vector<std::byte> payload; AppendU8(payload,static_cast<std::uint8_t>(contract.recoveryScope));
    AppendU64(payload,leaves.size()); for(const auto& x:leaves) AppendDigest(payload,x.Digest());
    AppendU64(payload,endpoints.size()); for(const auto& x:endpoints) AppendDigest(payload,x.Digest());
    AppendU64(payload,flows.size()); for(const auto& x:flows) AppendDigest(payload,x.Digest());
    return MakeIdentity<CompositionContractIdentity>("SGE.V2.Composition.Contract.V1",payload);
}
AllocationIdentity ComputeAllocationIdentityV1(FlowIdentity flowIdentity,std::uint64_t sizeBytes,AllocationOwnershipV1 ownership)
{
    std::vector<std::byte> payload; AppendDigest(payload,flowIdentity.Digest()); AppendU64(payload,sizeBytes); AppendU8(payload,static_cast<std::uint8_t>(ownership));
    return MakeIdentity<AllocationIdentity>("SGE.V2.Composition.Allocation.V1",payload);
}
ScheduleIdentity ComputeScheduleIdentityV1(std::span<const CompositionScheduleEntryV1> schedule)
{
    std::vector<std::byte> payload; AppendU64(payload,schedule.size()); for(const auto& x:schedule){AppendU32(payload,x.ordinal.Value());AppendDigest(payload,x.leafIdentity.Digest());}
    return MakeIdentity<ScheduleIdentity>("SGE.V2.Composition.Schedule.V1",payload);
}
BindingSetIdentity ComputeBindingSetIdentityV1(std::span<const EndpointBindingV1> bindings)
{
    std::vector<std::byte> payload; AppendU64(payload,bindings.size()); for(const auto& x:bindings){AppendDigest(payload,x.endpointIdentity.Digest());AppendDigest(payload,x.flowIdentity.Digest());}
    return MakeIdentity<BindingSetIdentity>("SGE.V2.Composition.BindingSet.V1",payload);
}
HandoffSetIdentity ComputeHandoffSetIdentityV1(std::span<const StateHandoffV1> handoffs)
{
    std::vector<std::byte> payload; AppendU64(payload,handoffs.size()); for(const auto& x:handoffs){AppendDigest(payload,x.flowIdentity.Digest());AppendDigest(payload,x.producerEndpointIdentity.Digest());AppendDigest(payload,x.consumerEndpointIdentity.Digest());AppendU8(payload,static_cast<std::uint8_t>(x.producerOutgoingState));AppendU8(payload,static_cast<std::uint8_t>(x.consumerIncomingState));}
    return MakeIdentity<HandoffSetIdentity>("SGE.V2.Composition.HandoffSet.V1",payload);
}
SynchronizationSetIdentity ComputeSynchronizationSetIdentityV1(std::span<const SynchronizationEdgeV1> synchronizations)
{
    std::vector<std::byte> payload; AppendU64(payload,synchronizations.size()); for(const auto& x:synchronizations){AppendDigest(payload,x.flowIdentity.Digest());AppendDigest(payload,x.producerLeafIdentity.Digest());AppendDigest(payload,x.consumerLeafIdentity.Digest());AppendU32(payload,x.signalOrdinal.Value());AppendU32(payload,x.waitOrdinal.Value());}
    return MakeIdentity<SynchronizationSetIdentity>("SGE.V2.Composition.SynchronizationSet.V1",payload);
}
RecoverySetIdentity ComputeRecoverySetIdentityV1(const RecoverySetV1& recoverySet)
{
    std::vector<std::byte> payload; AppendU8(payload,static_cast<std::uint8_t>(recoverySet.scope)); AppendU64(payload,recoverySet.leaves.size()); for(const auto& x:recoverySet.leaves)AppendDigest(payload,x.Digest()); AppendU64(payload,recoverySet.flows.size()); for(const auto& x:recoverySet.flows)AppendDigest(payload,x.Digest());
    return MakeIdentity<RecoverySetIdentity>("SGE.V2.Composition.RecoverySet.V1",payload);
}
CompositionPlanIdentity ComputeCompositionPlanIdentityV1(const CompositionPlanProposalV1& p)
{
    std::vector<std::byte> payload; AppendDigest(payload,p.contractIdentity.Digest()); AppendU64(payload,p.allocations.size()); for(const auto& x:p.allocations)AppendDigest(payload,x.identity.Digest()); AppendDigest(payload,p.scheduleIdentity.Digest()); AppendDigest(payload,p.bindingSetIdentity.Digest()); AppendDigest(payload,p.handoffSetIdentity.Digest()); AppendDigest(payload,p.synchronizationSetIdentity.Digest()); AppendBool(payload,p.presenter.has_value()); if(p.presenter){AppendDigest(payload,p.presenter->leafIdentity.Digest());AppendDigest(payload,p.presenter->endpointIdentity.Digest());} AppendDigest(payload,p.recoverySet.identity.Digest());
    return MakeIdentity<CompositionPlanIdentity>("SGE.V2.Composition.Plan.V1",payload);
}
CompositionSealIdentity ComputeCompositionSealIdentityV1(CompositionContractIdentity contractIdentity,CompositionPlanIdentity planIdentity,ScheduleIdentity scheduleIdentity,RecoverySetIdentity recoveryIdentity)
{
    std::vector<std::byte> payload; AppendDigest(payload,contractIdentity.Digest());AppendDigest(payload,planIdentity.Digest());AppendDigest(payload,scheduleIdentity.Digest());AppendDigest(payload,recoveryIdentity.Digest()); return MakeIdentity<CompositionSealIdentity>("SGE.V2.Composition.Seal.V1",payload);
}
FrozenCompositionIdentity ComputeFrozenCompositionIdentityV1(const VerifiedCompositionV1& v)
{
    std::vector<std::byte> payload; AppendDigest(payload,v.ContractIdentity().Digest());AppendDigest(payload,v.PlanIdentity().Digest());AppendDigest(payload,v.SealIdentity().Digest());AppendDigest(payload,v.ScheduleIdentityValue().Digest());AppendDigest(payload,v.RecoveryIdentity().Digest());AppendU32(payload,v.LeafCount());AppendU32(payload,v.FlowCount()); return MakeIdentity<FrozenCompositionIdentity>("SGE.V2.Composition.Frozen.V1",payload);
}
}
