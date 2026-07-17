#include "CompositionRuntime.h"

#include "../09_FrozenPackageCore/PackageReader.h"

#include <algorithm>
#include <map>
#include <set>

namespace sge4::composition::runtime_v1
{
namespace
{
CompositionRuntimeError Error(std::string stage,std::string message){return{std::move(stage),std::move(message)};}
CompositionRuntimeError Error(const runtime::RuntimeError& value){return{value.stage,value.message};}
}

std::shared_ptr<runtime::IExternalResource> LoadedComposition::SharedResource(SharedResourceId id) const
{ return id.value<resources_.size()?resources_[id.value].resource:nullptr; }
std::shared_ptr<runtime::ICompletionToken> LoadedComposition::AvailableAfter(SharedResourceId id) const
{ return id.value<resources_.size()?resources_[id.value].availableAfter:nullptr; }

base::Result<void,CompositionRuntimeError> LoadedComposition::Materialize()
{
    const auto& contract=artifact_.Contract();
    leafPackages_.clear();instances_.clear();resources_.clear();resourceStates_.clear();endpointTokens_.assign(contract.endpoints.size(),nullptr);
    leafPackages_.reserve(contract.leaves.size());instances_.resize(contract.leaves.size());
    for(const auto& leaf:contract.leaves)
    {
        auto decoded=package::PackageReader::Read(leaf.packageBytes);if(!decoded)return base::Result<void,CompositionRuntimeError>::Failure(Error("composition/load-leaf",decoded.Error().message));
        auto shared=std::make_shared<const package::FrozenExecutablePackage>(std::move(decoded).Value());leafPackages_.push_back(shared);
        auto instance=backend_->LoadIntoDomain(*domain_,shared,leaf.surfaceSlotCount?surface_:nullptr);if(!instance)return base::Result<void,CompositionRuntimeError>::Failure(Error(instance.Error()));
        instances_[leaf.id.value]=std::move(instance).Value();
    }
    resources_.reserve(contract.resources.size());resourceStates_.reserve(contract.resources.size());
    for(const auto& resource:contract.resources)
    {
        auto created=backend_->CreateSharedBuffer(*domain_,resource.id.value,resource.sizeBytes,resource.initialState,initialData_[resource.id.value]);
        if(!created)return base::Result<void,CompositionRuntimeError>::Failure(Error(created.Error()));
        resources_.push_back(std::move(created).Value());resourceStates_.push_back(resource.initialState);
    }
    return base::Result<void,CompositionRuntimeError>::Success();
}

base::Result<LoadedComposition, CompositionRuntimeError> LoadComposition(FrozenComposition artifact,d3d12::D3D12Backend& backend,CompositionLoadInput input)
{
    LoadedComposition loaded(std::move(artifact),backend,input);const auto& contract=loaded.artifact_.Contract();loaded.initialData_.resize(contract.resources.size());std::set<std::uint32_t> initialized;
    for(auto& value:input.initialResources)
    {
        if(value.resource.value>=contract.resources.size()||!initialized.insert(value.resource.value).second||value.bytes.size()>contract.resources[value.resource.value].sizeBytes)
            return base::Result<LoadedComposition,CompositionRuntimeError>::Failure(Error("composition/initial-data","shared resource initial data is invalid or duplicated"));
        loaded.initialData_[value.resource.value]=std::move(value.bytes);
    }
    auto domain=backend.CreateDeviceDomain();if(!domain)return base::Result<LoadedComposition,CompositionRuntimeError>::Failure(Error(domain.Error()));loaded.domain_=std::move(domain).Value();
    auto materialized=loaded.Materialize();if(!materialized)return base::Result<LoadedComposition,CompositionRuntimeError>::Failure(materialized.Error());
    return base::Result<LoadedComposition,CompositionRuntimeError>::Success(std::move(loaded));
}

base::Result<CompositionSubmission, CompositionRuntimeError> SubmitComposition(LoadedComposition& loaded,const CompositionFrameInvocation& invocation)
{
    const auto& contract=loaded.artifact_.Contract();const auto& plan=loaded.artifact_.VerifiedPlan().Plan();
    if (!loaded.domain_ || loaded.domain_->State()!=runtime::DeviceRuntimeState::Active || loaded.instances_.size()!=contract.leaves.size())
        return base::Result<CompositionSubmission,CompositionRuntimeError>::Failure(Error("composition/device-state","Composition DeviceDomain is not Active; adapter reacquisition is required"));
    std::vector<std::vector<runtime::DynamicDataBinding>> dynamics(contract.leaves.size());std::set<std::pair<std::uint32_t,std::uint32_t>> dynamicKeys;
    for(const auto& value:invocation.dynamicData)
    {
        if(value.leaf.value>=contract.leaves.size()||value.slot==package::InvalidIndex||!dynamicKeys.emplace(value.leaf.value,value.slot).second)
            return base::Result<CompositionSubmission,CompositionRuntimeError>::Failure(Error("composition/dynamic-data","dynamic binding is invalid or duplicated"));
        dynamics[value.leaf.value].push_back({value.slot,value.bytes});
    }
    std::vector<SharedResourceId> endpointResources(contract.endpoints.size());for(const auto& value:contract.bindings)endpointResources[value.endpoint.value]=value.resource;
    std::vector<CompositionEndpointId> declaredProducer(contract.endpoints.size(),CompositionEndpointId{package::InvalidIndex});
    for(const auto& dependency:plan.signalWaits)declaredProducer[dependency.waitEndpoint.value]=dependency.signalEndpoint;
    CompositionSubmission result;result.deviceEpoch=loaded.domain_->DeviceEpoch();result.leaves.reserve(plan.schedule.size());
    for(const auto& entry:plan.schedule)
    {
        const auto leaf=entry.leaf.value;std::vector<runtime::ExternalResourceBinding> external(contract.leaves[leaf].externalSlots.size());std::vector<bool> present(external.size(),false);
        for(const auto& endpoint:contract.endpoints)if(endpoint.leaf.value==leaf)
        {
            const auto resource=endpointResources[endpoint.id.value];const auto& binding=loaded.resources_[resource.value];
            auto token=binding.availableAfter;
            if(declaredProducer[endpoint.id.value].IsValid())
            {
                token=loaded.endpointTokens_[declaredProducer[endpoint.id.value].value];
                if(!token)return base::Result<CompositionSubmission,CompositionRuntimeError>::Failure(Error("composition/signal-wait","declared producer endpoint has not released its completion token"));
            }
            external[endpoint.externalSlot]={endpoint.externalSlot,binding.resource,std::move(token)};present[endpoint.externalSlot]=true;
        }
        if(std::ranges::any_of(present,[](bool value){return !value;}))return base::Result<CompositionSubmission,CompositionRuntimeError>::Failure(Error("composition/bindings","verified endpoint binding is incomplete"));
        runtime::FrameInvocation leafInvocation{invocation.frameNumber,dynamics[leaf],external};auto submitted=loaded.backend_->Submit(*loaded.instances_[leaf],leafInvocation);
        if(!submitted)return base::Result<CompositionSubmission,CompositionRuntimeError>::Failure(Error(submitted.Error()));
        for(const auto& release:submitted.Value().releasedExternalResources)
        {
            const auto found=std::ranges::find_if(contract.endpoints,[&](const auto& endpoint){return endpoint.leaf.value==leaf&&endpoint.externalSlot==release.slot;});
            if(found==contract.endpoints.end())return base::Result<CompositionSubmission,CompositionRuntimeError>::Failure(Error("composition/release","Leaf released an undeclared endpoint"));
            const auto resource=endpointResources[found->id.value];loaded.endpointTokens_[found->id.value]=release.safeAfter;
            loaded.resources_[resource.value].availableAfter=release.safeAfter;loaded.resourceStates_[resource.value]=found->contract.outgoingState;
        }
        result.leaves.push_back(std::move(submitted).Value());
    }
    return base::Result<CompositionSubmission,CompositionRuntimeError>::Success(std::move(result));
}

base::Result<d3d12::ExternalBufferReadback, CompositionRuntimeError> ReadCompositionBuffer(LoadedComposition& loaded,SharedResourceId resource)
{
    if (!loaded.domain_ || loaded.domain_->State()!=runtime::DeviceRuntimeState::Active)
        return base::Result<d3d12::ExternalBufferReadback,CompositionRuntimeError>::Failure(Error("composition/device-state","Composition DeviceDomain is not Active; adapter reacquisition is required"));
    if(resource.value>=loaded.resources_.size())return base::Result<d3d12::ExternalBufferReadback,CompositionRuntimeError>::Failure(Error("composition/readback","shared resource ID is invalid"));
    const auto& binding=loaded.resources_[resource.value];auto read=loaded.backend_->ReadSharedBuffer(*loaded.domain_,binding.resource,binding.availableAfter,loaded.resourceStates_[resource.value]);
    if(!read)return base::Result<d3d12::ExternalBufferReadback,CompositionRuntimeError>::Failure(Error(read.Error()));loaded.resources_[resource.value].availableAfter=read.Value().availableAfter;
    return base::Result<d3d12::ExternalBufferReadback,CompositionRuntimeError>::Success(std::move(read).Value());
}

base::Result<runtime::DeviceRecoveryReport, CompositionRuntimeError> RecoverComposition(LoadedComposition& loaded,runtime::DeviceRecoveryMode mode)
{
    loaded.instances_.clear();loaded.leafPackages_.clear();loaded.resources_.clear();loaded.resourceStates_.clear();
    auto recovered=loaded.backend_->RecoverDeviceDomain(*loaded.domain_,mode);if(!recovered)return base::Result<runtime::DeviceRecoveryReport,CompositionRuntimeError>::Failure(Error(recovered.Error()));
    if (recovered.Value().stateAfter!=runtime::DeviceRuntimeState::Active)
        return base::Result<runtime::DeviceRecoveryReport,CompositionRuntimeError>::Success(std::move(recovered).Value());
    auto materialized=loaded.Materialize();if(!materialized)return base::Result<runtime::DeviceRecoveryReport,CompositionRuntimeError>::Failure(materialized.Error());
    return base::Result<runtime::DeviceRecoveryReport,CompositionRuntimeError>::Success(std::move(recovered).Value());
}
}
