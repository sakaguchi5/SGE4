#include "./ExecutionPlanVerifier.h"

#include <algorithm>
#include <map>
#include <set>
#include <tuple>

namespace sge4::planning::verification
{
namespace
{
void Add(VerificationReport& report, DiagnosticCode code, std::string stage,
         std::string message, std::uint32_t work = base::InvalidIndex,
         std::uint32_t resource = base::InvalidIndex, std::uint32_t use = base::InvalidIndex)
{
    report.violations.push_back({code, std::move(stage), std::move(message), work, resource, use});
}

bool QueueAvailable(const D3D12PlanningContract& contract, QueueClass queueClass, std::uint32_t index)
{
    if (queueClass == QueueClass::Direct) return index < contract.directQueueCount;
    if (queueClass == QueueClass::Compute) return index < contract.computeQueueCount;
    return index < contract.copyQueueCount;
}

bool QueueCanExecute(semantic::WorkKind kind, QueueClass queueClass)
{
    if (kind == semantic::WorkKind::Raster || kind == semantic::WorkKind::Present)
        return queueClass == QueueClass::Direct;
    if (kind == semantic::WorkKind::Compute)
        return queueClass == QueueClass::Direct || queueClass == QueueClass::Compute;
    return queueClass == QueueClass::Direct || queueClass == QueueClass::Copy;
}

bool SameQueue(const WorkQueueAssignment& left, const WorkQueueAssignment& right)
{
    return left.queueClass == right.queueClass && left.queueIndex == right.queueIndex;
}

// Deliberately duplicated from neither Planner nor Package lowering. This is
// the verifier's independent D3D12 v1 state oracle.
AbstractState ExpectedState(semantic::ViewRole role, semantic::WorkKind workKind)
{
    switch (role)
    {
    case semantic::ViewRole::VertexData: return AbstractState::VertexBuffer;
    case semantic::ViewRole::ConstantData: return AbstractState::ConstantBuffer;
    case semantic::ViewRole::SampledTexture:
    case semantic::ViewRole::ShaderBuffer: return AbstractState::ShaderRead;
    case semantic::ViewRole::StorageBuffer:
    case semantic::ViewRole::StorageTexture2D: return AbstractState::UnorderedWrite;
    case semantic::ViewRole::ColorAttachment: return AbstractState::RenderTarget;
    case semantic::ViewRole::PresentSource: return AbstractState::Present;
    case semantic::ViewRole::DepthAttachment: return AbstractState::DepthWrite;
    case semantic::ViewRole::CopySource: return AbstractState::CopySource;
    case semantic::ViewRole::CopyDestination: return AbstractState::CopyDestination;
    }
    return workKind == semantic::WorkKind::Copy ? AbstractState::CopySource : AbstractState::Common;
}
}

VerificationReport Verify(const SemanticObligation& obligation,
                          const D3D12PlanningContract& contract,
                          const ExecutionPlanIR& plan)
{
    VerificationReport report;
    if (obligation.digest != base::Sha256(EncodeCanonical(obligation)))
        Add(report, DiagnosticCode::ObligationIdentityMismatch, "identity",
            "DigestがCanonicalな順序または識別子規則に違反しています。");
    if (contract.digest != base::Sha256(EncodeCanonical(contract)))
        Add(report, DiagnosticCode::PlanningContractIdentityMismatch, "identity",
            "ContractがCanonicalな順序または識別子規則に違反しています。");
    if (plan.identity != ComputePlanIdentity(plan))
        Add(report, DiagnosticCode::PlanIdentityMismatch, "identity", "PlanがCanonicalな順序または識別子規則に違反しています。");

    std::map<std::uint32_t, const ObligationWork*> works;
    for (const auto& work : obligation.works) works[work.id.value] = &work;
    std::map<std::uint32_t, std::uint32_t> positions;
    for (std::uint32_t position = 0; position < plan.workSchedule.size(); ++position)
    {
        const auto id = plan.workSchedule[position].value;
        if (!works.contains(id)) Add(report, DiagnosticCode::UnknownWork, "schedule", "Scheduleが検証または実行の契約に違反しています。", id);
        if (!positions.emplace(id, position).second) Add(report, DiagnosticCode::DuplicateWork, "schedule", "Scheduleが検証または実行の契約に違反しています。", id);
    }
    for (const auto& [id, work] : works)
        if (!positions.contains(id)) Add(report, DiagnosticCode::MissingWork, "schedule", "Scheduleが検証または実行の契約に違反しています。", id);
    for (const auto& dependency : obligation.dependencies)
    {
        const auto producer = positions.find(dependency.producer.value);
        const auto consumer = positions.find(dependency.consumer.value);
        if (producer != positions.end() && consumer != positions.end() && producer->second >= consumer->second)
            Add(report, DiagnosticCode::DependencyOrderViolation, "schedule", "Scheduleが検証または実行の契約に違反しています。",
                dependency.consumer.value, dependency.resource.value);
    }

    std::map<std::uint32_t, WorkQueueAssignment> assignments;
    for (const auto& assignment : plan.queueAssignments)
    {
        if (!works.contains(assignment.work.value))
        {
            Add(report, DiagnosticCode::UnknownWork, "queue", "Queueが検証または実行の契約に違反しています。", assignment.work.value);
            continue;
        }
        if (!assignments.emplace(assignment.work.value, assignment).second)
            Add(report, DiagnosticCode::DuplicateQueueAssignment, "queue", "Queueが検証または実行の契約に違反しています。", assignment.work.value);
        if (!QueueAvailable(contract, assignment.queueClass, assignment.queueIndex) ||
            !QueueCanExecute(works.at(assignment.work.value)->kind, assignment.queueClass))
            Add(report, DiagnosticCode::QueueCapabilityViolation, "queue", "Queueが検証または実行の契約に違反しています。",
                assignment.work.value);
    }
    for (const auto& [id, work] : works)
        if (!assignments.contains(id)) Add(report, DiagnosticCode::MissingQueueAssignment, "queue", "Queueが検証または実行の契約に違反しています。", id);

    std::map<std::uint32_t, std::uint32_t> producerSignals;
    std::set<std::pair<std::uint32_t, std::uint32_t>> synchronized;
    for (const auto& edge : plan.synchronization)
    {
        const auto producer = positions.find(edge.producer.value);
        const auto consumer = positions.find(edge.consumer.value);
        if (producer == positions.end() || consumer == positions.end() || producer->second >= consumer->second ||
            edge.signalPoint == base::InvalidIndex)
        {
            Add(report, DiagnosticCode::InvalidSynchronization, "synchronization", "Signalが検証または実行の契約に違反しています。",
                edge.consumer.value);
            continue;
        }
        if (edge.signalPoint != producer->second)
            Add(report, DiagnosticCode::InvalidSynchronization, "synchronization",
                "ScheduleがCanonicalな順序または識別子規則に違反しています。", edge.producer.value);
        const auto producerQueue = assignments.find(edge.producer.value);
        const auto consumerQueue = assignments.find(edge.consumer.value);
        if (producerQueue != assignments.end() && consumerQueue != assignments.end() &&
            SameQueue(producerQueue->second, consumerQueue->second))
            Add(report, DiagnosticCode::InvalidSynchronization, "synchronization",
                "Queueが検証または実行の契約に違反しています。", edge.consumer.value);
        const auto existing = producerSignals.find(edge.producer.value);
        if (existing != producerSignals.end() && existing->second != edge.signalPoint)
            Add(report, DiagnosticCode::InvalidSynchronization, "synchronization", "Signalが検証または実行の契約に違反しています。",
                edge.producer.value);
        producerSignals[edge.producer.value] = edge.signalPoint;
        if (!synchronized.emplace(edge.producer.value, edge.consumer.value).second)
            Add(report, DiagnosticCode::InvalidSynchronization, "synchronization", "Signalが検証または実行の契約に違反しています。",
                edge.consumer.value);
    }
    for (const auto& dependency : obligation.dependencies)
    {
        const auto producerQueue = assignments.find(dependency.producer.value);
        const auto consumerQueue = assignments.find(dependency.consumer.value);
        if (producerQueue == assignments.end() || consumerQueue == assignments.end()) continue;
        const bool sameQueue = SameQueue(producerQueue->second, consumerQueue->second);
        if (!sameQueue && !synchronized.contains({dependency.producer.value, dependency.consumer.value}))
            Add(report, DiagnosticCode::MissingSynchronization, "synchronization", "Queueが検証または実行の契約に違反しています。",
                dependency.consumer.value, dependency.resource.value);
    }

    std::map<std::uint32_t, const ObligationUse*> uses;
    for (const auto& use : obligation.uses) uses[use.id.value] = &use;
    std::map<std::uint32_t, const ObligationResource*> resources;
    for (const auto& resource : obligation.resources) resources[resource.id.value] = &resource;
    std::map<std::uint32_t, ResourceInstancePlan> resourcePlans;
    for (const auto& resourcePlan : plan.resourceInstances)
    {
        if (!resources.contains(resourcePlan.resource.value) ||
            !resourcePlans.emplace(resourcePlan.resource.value, resourcePlan).second)
        {
            Add(report, DiagnosticCode::DuplicateResourcePlan, "resource", "Resourceが検証または実行の契約に違反しています。",
                base::InvalidIndex, resourcePlan.resource.value);
            continue;
        }
        const auto& resource = *resources.at(resourcePlan.resource.value);
        const std::uint32_t requiredInstances = resource.kind == semantic::ResourceKind::SurfaceImage ? 0u :
            resource.lifetime == semantic::LifetimeIntent::External ? 1u :
            (resource.lifetime == semantic::LifetimeIntent::FrameLocal || resource.lifetime == semantic::LifetimeIntent::Temporal) ?
                contract.framesInFlight : 1u;
        if (resourcePlan.physicalInstanceCount != requiredInstances)
            Add(report, DiagnosticCode::ResourceInstanceViolation, "resource", "入力または内部状態が検証または実行の契約に違反しています。",
                base::InvalidIndex, resource.id.value);

        std::uint32_t first = base::InvalidIndex;
        std::uint32_t last = base::InvalidIndex;
        for (const auto& use : obligation.uses)
        {
            if (use.resource != resource.id) continue;
            const auto position = positions.find(use.owner.value);
            if (position == positions.end()) continue;
            first = first == base::InvalidIndex ? position->second : std::min(first, position->second);
            last = last == base::InvalidIndex ? position->second : std::max(last, position->second);
        }
        if (resourcePlan.firstUse != first || resourcePlan.lastUse != last)
            Add(report, DiagnosticCode::LifetimeViolation, "lifetime", "ResourceがCanonicalな順序または識別子規則に違反しています。",
                base::InvalidIndex, resource.id.value);
    }
    for (const auto& [id, resource] : resources)
        if (!resourcePlans.contains(id)) Add(report, DiagnosticCode::DuplicateResourcePlan, "resource", "Resourceが検証または実行の契約に違反しています。", base::InvalidIndex, id);

    std::map<std::uint32_t, const AllocationPlan*> allocations;
    std::map<std::uint32_t, std::uint32_t> allocationCoverage;
    std::map<std::uint32_t, std::uint32_t> allocationForResource;
    for (const auto& allocation : plan.allocations)
    {
        if (allocation.id == base::InvalidIndex || !allocations.emplace(allocation.id, &allocation).second)
            Add(report, DiagnosticCode::AllocationCoverageViolation, "allocation", "Allocationが検証または実行の契約に違反しています。");
        if (allocation.resources.empty())
            Add(report, DiagnosticCode::AllocationCoverageViolation, "allocation", "Resourceが検証または実行の契約に違反しています。");
        if (allocation.kind == PlanAllocationKind::Committed && allocation.resources.size() != 1)
            Add(report, DiagnosticCode::AllocationAliasViolation, "allocation", "Resourceが検証または実行の契約に違反しています。");
        if (allocation.kind == PlanAllocationKind::Placed && allocation.resources.size() == 1 &&
            !contract.standalonePlacedAllocation)
            Add(report, DiagnosticCode::AllocationAliasViolation, "allocation",
                "Allocationが検証または実行の契約に違反しています。");
        std::uint64_t requiredSize = 0;
        std::uint32_t requiredInstances = 0;
        for (const auto resourceId : allocation.resources)
        {
            if (!resources.contains(resourceId.value) || ++allocationCoverage[resourceId.value] != 1)
            {
                Add(report, DiagnosticCode::AllocationCoverageViolation, "allocation", "Resourceが検証または実行の契約に違反しています。",
                    base::InvalidIndex, resourceId.value);
                continue;
            }
            allocationForResource[resourceId.value] = allocation.id;
            const auto& resource = *resources.at(resourceId.value);
            if (resource.lifetime == semantic::LifetimeIntent::External || resource.kind == semantic::ResourceKind::SurfaceImage)
                Add(report, DiagnosticCode::AllocationCoverageViolation, "allocation", "Packageが検証または実行の契約に違反しています。",
                    base::InvalidIndex, resourceId.value);
            requiredSize = std::max(requiredSize, resource.sizeBytes);
            const auto resourcePlan = resourcePlans.find(resourceId.value);
            if (resourcePlan != resourcePlans.end())
            {
                requiredInstances = std::max(requiredInstances, resourcePlan->second.physicalInstanceCount);
                if (resourcePlan->second.allocation != allocation.id)
                    Add(report, DiagnosticCode::AllocationCoverageViolation, "allocation",
                        "Resourceが検証または実行の契約に違反しています。",
                        base::InvalidIndex, resourceId.value);
            }
        }
        if (allocation.sizeBytes < requiredSize || allocation.physicalInstanceCount != requiredInstances)
            Add(report, DiagnosticCode::AllocationCoverageViolation, "allocation", "Allocationが検証または実行の契約に違反しています。");
        for (std::size_t left = 0; left < allocation.resources.size(); ++left)
            for (std::size_t right = left + 1; right < allocation.resources.size(); ++right)
            {
                const auto a = resourcePlans.find(allocation.resources[left].value);
                const auto b = resourcePlans.find(allocation.resources[right].value);
                if (a == resourcePlans.end() || b == resourcePlans.end()) continue;
                const bool overlap = a->second.firstUse != base::InvalidIndex && b->second.firstUse != base::InvalidIndex &&
                    !(a->second.lastUse < b->second.firstUse || b->second.lastUse < a->second.firstUse);
                const auto& ar = *resources.at(allocation.resources[left].value);
                const auto& br = *resources.at(allocation.resources[right].value);
                const bool compatible = ar.aliasCompatibleWith == br.id || br.aliasCompatibleWith == ar.id;
                if (overlap || !compatible || ar.kind != br.kind || allocation.aliasGroup == base::InvalidIndex)
                    Add(report, DiagnosticCode::AllocationAliasViolation, "alias", "Resourceが検証または実行の契約に違反しています。");
            }
    }
    for (const auto& [id, resource] : resources)
    {
        const bool packageOwned = resource->lifetime != semantic::LifetimeIntent::External && resource->kind != semantic::ResourceKind::SurfaceImage;
        if (packageOwned && allocationCoverage[id] != 1)
            Add(report, DiagnosticCode::AllocationCoverageViolation, "allocation", "Packageが検証または実行の契約に違反しています。", base::InvalidIndex, id);
        if (!packageOwned && allocationCoverage[id] != 0)
            Add(report, DiagnosticCode::AllocationCoverageViolation, "allocation", "Packageが検証または実行の契約に違反しています。", base::InvalidIndex, id);
        const auto planFound = resourcePlans.find(id);
        if (planFound != resourcePlans.end())
        {
            if (packageOwned)
            {
                const auto covering = allocationForResource.find(id);
                if (covering == allocationForResource.end() || planFound->second.allocation != covering->second)
                    Add(report, DiagnosticCode::AllocationCoverageViolation, "allocation",
                        "Resourceが検証または実行の契約に違反しています。",
                        base::InvalidIndex, id);
            }
            else if (planFound->second.allocation != base::InvalidIndex)
            {
                Add(report, DiagnosticCode::AllocationCoverageViolation, "allocation",
                    "Packageが検証または実行の契約に違反しています。", base::InvalidIndex, id);
            }
        }
    }

    std::map<std::uint32_t, AbstractState> states;
    for (const auto& state : plan.useStates)
    {
        if (!uses.contains(state.use.value) || !states.emplace(state.use.value, state.required).second)
            Add(report, DiagnosticCode::StatePlanViolation, "state", "ResourceUseが検証または実行の契約に違反しています。", base::InvalidIndex, base::InvalidIndex, state.use.value);
        else if (state.required != ExpectedState(uses.at(state.use.value)->role, works.at(uses.at(state.use.value)->owner.value)->kind))
            Add(report, DiagnosticCode::StatePlanViolation, "state", "ResourceUseが検証または実行の契約に違反しています。", base::InvalidIndex, uses.at(state.use.value)->resource.value, state.use.value);
    }
    for (const auto& [id, use] : uses)
        if (!states.contains(id)) Add(report, DiagnosticCode::StatePlanViolation, "state", "ResourceUseが検証または実行の契約に違反しています。", use->owner.value, use->resource.value, id);

    using BindingKey = std::pair<std::uint32_t, std::uint32_t>;
    std::map<BindingKey, const BindingPlan*> actualBindings;
    for (const auto& binding : plan.bindings)
    {
        const BindingKey key{binding.program.value, binding.parameter.value};
        if (!actualBindings.emplace(key, &binding).second)
            Add(report, DiagnosticCode::BindingPlanViolation, "binding", "Bindingが検証または実行の契約に違反しています。");
    }
    std::set<BindingKey> requiredBindings;
    std::uint32_t expectedRoot = 0;
    std::uint32_t expectedDescriptor = 0;
    std::uint32_t previousProgram = base::InvalidIndex;
    for (const auto& parameter : obligation.parameters)
    {
        const BindingKey key{parameter.program.value, parameter.id.value};
        requiredBindings.insert(key);
        if (parameter.program.value != previousProgram)
        {
            previousProgram = parameter.program.value;
            expectedRoot = 0;
        }
        const auto found = actualBindings.find(key);
        if (found == actualBindings.end())
        {
            Add(report, DiagnosticCode::BindingPlanViolation, "binding", "Bindingが検証または実行の契約に違反しています。");
            ++expectedRoot;
            if (parameter.kind != semantic::ProgramParameterKind::ConstantBuffer) ++expectedDescriptor;
            continue;
        }
        const auto& binding = *found->second;
        if (binding.rootParameterIndex != expectedRoot)
            Add(report, DiagnosticCode::BindingPlanViolation, "binding",
                "ProgramがCanonicalな順序または識別子規則に違反しています。");
        const bool descriptorBacked = parameter.kind != semantic::ProgramParameterKind::ConstantBuffer;
        const auto requiredDescriptor = descriptorBacked ? expectedDescriptor : base::InvalidIndex;
        if (binding.descriptorIndex != requiredDescriptor)
            Add(report, DiagnosticCode::BindingPlanViolation, "binding",
                "BindingがCanonicalな順序または識別子規則に違反しています。");
        ++expectedRoot;
        if (descriptorBacked) ++expectedDescriptor;
    }
    for (const auto& [key, binding] : actualBindings)
        if (!requiredBindings.contains(key))
            Add(report, DiagnosticCode::BindingPlanViolation, "binding", "Programが検証または実行の契約に違反しています。");

    std::vector<semantic::ResourceId> expectedExternal;
    std::vector<semantic::ResourceId> expectedPresent;
    for (const auto& resource : obligation.resources)
    {
        if (resource.lifetime == semantic::LifetimeIntent::External && resource.kind != semantic::ResourceKind::SurfaceImage)
            expectedExternal.push_back(resource.id);
        if (resource.kind == semantic::ResourceKind::SurfaceImage) expectedPresent.push_back(resource.id);
    }
    if (plan.externalBoundaries != expectedExternal || plan.presentBoundaries != expectedPresent)
        Add(report, DiagnosticCode::BoundaryViolation, "boundary", "入力または内部状態の情報が途中で切れているか不足しています。");

    if (plan.resourceInstances.size() > contract.maximumResources || plan.allocations.size() > contract.maximumAllocations ||
        plan.useStates.size() > contract.maximumViews)
        Add(report, DiagnosticCode::ArtifactCardinalityViolation, "cardinality", "Contractが検証または実行の契約に違反しています。");

    report.verified = report.violations.empty();
    return report;
}

base::Expected<VerifiedExecutionPlan, VerificationReport> VerifyAndSeal(
    const SemanticObligation& obligation,
    const D3D12PlanningContract& contract,
    const ExecutionPlanIR& plan)
{
    auto report = Verify(obligation, contract, plan);
    if (!report.verified)
        return base::Failure<VerifiedExecutionPlan, VerificationReport>(std::move(report));

    PlanVerificationCertificate certificate;
    certificate.obligationDigest = obligation.digest;
    certificate.planningContractDigest = contract.digest;
    certificate.planIdentity = plan.identity;
    certificate.seal = ComputeVerificationSeal(certificate);
    return base::Success<VerifiedExecutionPlan, VerificationReport>(
        VerifiedExecutionPlan(plan, std::move(certificate)));
}
}
