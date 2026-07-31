base::Expected<LoweredPackageStage, CompileError> LowerPackageStage(
    const ValidatedSourceStage& validated,
    const ProgramCompilationStage& compiledPrograms,
    const planning::ExecutionPlanIR* selectedPlan)
{
    if (validated.source == nullptr)
        return Failure<LoweredPackageStage>("package-lowering", "検証または実行の契約に違反しています。");
    const auto& graph = *validated.source;
    const auto& targetProfile = validated.targetProfile;
    const auto& analyzed = validated.analyzed;
    const auto& workOrder = selectedPlan == nullptr ? analyzed.canonicalWorkOrder : selectedPlan->workSchedule;

    std::map<std::uint32_t, const semantic::Resource*> resources;
    std::map<std::uint32_t, const semantic::ResourceUse*> uses;
    std::map<std::uint32_t, const semantic::Program*> programs;
    std::map<std::uint32_t, const semantic::Work*> works;
    for (const auto& value : graph.resources) resources[value.id.value] = &value;
    for (const auto& value : graph.resourceUses) uses[value.id.value] = &value;
    for (const auto& value : graph.programs) programs[value.id.value] = &value;
    for (const auto& value : graph.works) works[value.id.value] = &value;

    std::map<std::uint32_t, semantic::WorkKind> useWorkKinds;
    for (const auto& work : graph.works)
        for (const auto& operand : work.operands)
            useWorkKinds[operand.use.value] = work.kind;

    pkg::D3D12PackageDescription description;
    description.profile.minimumFeatureLevel = targetProfile.minimumFeatureLevel;
    description.profile.shaderModelMajor = targetProfile.shaderModelMajor;
    description.profile.shaderModelMinor = targetProfile.shaderModelMinor;
    description.profile.rootSignatureMajor = targetProfile.rootSignatureMajor;
    description.profile.rootSignatureMinor = targetProfile.rootSignatureMinor;
    description.profile.barrierModel = pkg::BarrierModel::Legacy;
    description.profile.shaderBinaryFormat = pkg::ShaderBinaryFormat::Dxbc;
    description.profile.framesInFlight = targetProfile.framesInFlight;
    description.profile.directQueueCount = targetProfile.directQueueCount;
    description.profile.computeQueueCount = targetProfile.computeQueueCount;
    description.profile.copyQueueCount = targetProfile.copyQueueCount;
    const bool packageHasSurface = std::any_of(graph.resources.begin(), graph.resources.end(),
        [](const semantic::Resource& resource) { return resource.kind == semantic::ResourceKind::SurfaceImage; });
    description.profile.surfaceImageCount = packageHasSurface ? targetProfile.surfaceImageCount : 0;
    description.profile.rtvDescriptorCount = targetProfile.rtvDescriptorCount;
    description.profile.dsvDescriptorCount = targetProfile.dsvDescriptorCount;
    description.profile.shaderDescriptorCount = targetProfile.shaderDescriptorCount;
    description.profile.samplerDescriptorCount = targetProfile.samplerDescriptorCount;

    std::map<std::uint32_t, pkg::ResourceId> resourceMap;
    std::map<std::uint32_t, std::vector<const semantic::ResourceUse*>> resourceUses;
    for (const auto& use : graph.resourceUses) resourceUses[use.resource.value].push_back(&use);
    for (auto& [resource, resourceUseList] : resourceUses)
        std::sort(resourceUseList.begin(), resourceUseList.end(), [](const auto* left, const auto* right) {
            return left->id.value < right->id.value;
        });

    std::map<std::uint32_t, std::uint32_t> physicalInstances;
    for (const auto resourceId : analyzed.canonicalResourceOrder)
    {
        const auto& source = *resources.at(resourceId.value);
        const pkg::ResourceId packageId{static_cast<std::uint32_t>(description.resources.size())};
        resourceMap[source.id.value] = packageId;

        pkg::ResourceArtifact artifact;
        artifact.id = packageId;
        artifact.resourceKind = ResourceKind(source.kind);
        artifact.format = ResourceFormat(source);
        artifact.sampleCount = 1;
        artifact.planeCount = 1;
        if (source.kind == semantic::ResourceKind::SurfaceImage)
        {
            artifact.origin = pkg::ResourceOrigin::Surface;
            artifact.rebuildPolicy = pkg::RebuildPolicy::RuntimeManaged;
            artifact.extentMode = pkg::ExtentMode::SurfaceRelative;
            artifact.physicalInstanceCount = 0;
            artifact.initialState = Present();
        }
        else if (source.lifetime == semantic::LifetimeIntent::External)
        {
            artifact.origin = pkg::ResourceOrigin::External;
            artifact.rebuildPolicy = pkg::RebuildPolicy::RequireExternalRebind;
            artifact.physicalInstanceCount = 1;
            artifact.initialState = Common();
        }
        else
        {
            artifact.origin = pkg::ResourceOrigin::PackageOwned;
            artifact.rebuildPolicy = pkg::RebuildPolicy::RecreateFromPackage;
            artifact.physicalInstanceCount =
                (source.lifetime == semantic::LifetimeIntent::FrameLocal ||
                 source.lifetime == semantic::LifetimeIntent::Temporal) ? targetProfile.framesInFlight : 1;
            artifact.initialState = source.update == semantic::UpdateIntent::DynamicPerFrame ?
                Explicit(pkg::ExplicitStateBits::ConstantBuffer) : Common();
        }
        physicalInstances[source.id.value] = artifact.physicalInstanceCount;
        if (source.lifetime == semantic::LifetimeIntent::FrameLocal)
            artifact.flags |= static_cast<std::uint32_t>(pkg::ResourceFlags::FrameLocal);
        if (source.lifetime == semantic::LifetimeIntent::Temporal)
            artifact.flags |= static_cast<std::uint32_t>(pkg::ResourceFlags::Temporal);

        if (source.kind == semantic::ResourceKind::Buffer)
        {
            artifact.sizeBytes = source.update == semantic::UpdateIntent::DynamicPerFrame ?
                AlignUp(source.buffer.sizeBytes, ConstantPlacementAlignment) : source.buffer.sizeBytes;
        }
        else if (source.kind == semantic::ResourceKind::Texture2D)
        {
            artifact.extentMode = source.texture2D.extentMeaning == semantic::TextureExtentMeaning::Fixed ?
                pkg::ExtentMode::Fixed : pkg::ExtentMode::SurfaceRelative;
            artifact.width = source.texture2D.width;
            artifact.height = source.texture2D.height;
            artifact.depthOrArraySize = 1;
            artifact.mipLevels = source.texture2D.mipLevels;
        }
        if (!source.initialContent.empty())
        {
            artifact.initialDataOffset = AppendAligned(description.initialData, source.initialContent);
            artifact.initialDataSize = source.initialContent.size();
        }
        description.resources.push_back(artifact);
    }

    std::map<std::uint32_t, pkg::ViewId> viewMap;
    std::uint32_t rtvDescriptors = 0;
    std::uint32_t dsvDescriptors = 0;
    std::uint32_t shaderDescriptors = 0;
    for (const auto resourceId : analyzed.canonicalResourceOrder)
    {
        const auto packageResource = resourceMap.at(resourceId.value);
        auto& resourceArtifact = description.resources[packageResource.value];
        resourceArtifact.firstView = static_cast<std::uint32_t>(description.views.size());
        const auto foundUses = resourceUses.find(resourceId.value);
        if (foundUses != resourceUses.end())
        {
            for (const auto* sourceUse : foundUses->second)
            {
                const auto& sourceResource = *resources.at(resourceId.value);
                pkg::ResourceViewArtifact view;
                view.id = {static_cast<std::uint32_t>(description.views.size())};
                view.resource = packageResource;
                view.viewClass = ViewClass(sourceUse->role);
                view.format = ResourceFormat(sourceResource);
                if (sourceResource.kind == semantic::ResourceKind::Buffer)
                {
                    view.byteSize = sourceResource.buffer.sizeBytes;
                    view.strideBytes = sourceResource.buffer.strideBytes;
                }
                else
                {
                    view.mipCount = sourceResource.kind == semantic::ResourceKind::Texture2D ?
                        sourceResource.texture2D.mipLevels : 1;
                    view.arrayLayerCount = 1;
                    view.planeCount = 1;
                }
                if (sourceResource.lifetime == semantic::LifetimeIntent::Temporal)
                    view.flags = static_cast<std::uint32_t>(
                        sourceUse->temporalRelation == semantic::TemporalRelation::Previous ?
                        pkg::ResourceViewFlags::TemporalPrevious : pkg::ResourceViewFlags::TemporalCurrent);

                if (DescriptorBacked(view.viewClass))
                {
                    const auto instances = sourceResource.kind == semantic::ResourceKind::SurfaceImage ?
                        targetProfile.surfaceImageCount : std::max(1u, physicalInstances[sourceResource.id.value]);
                    view.descriptorInstanceStride = instances > 1 ? 1u : 0u;
                    if (view.viewClass == pkg::ViewClass::RenderTarget)
                    {
                        view.descriptorHeapClass = 1;
                        view.descriptorIndex = rtvDescriptors;
                        rtvDescriptors += instances;
                    }
                    else if (view.viewClass == pkg::ViewClass::DepthStencil)
                    {
                        view.descriptorHeapClass = 3;
                        view.descriptorIndex = dsvDescriptors;
                        dsvDescriptors += instances;
                    }
                    else
                    {
                        view.descriptorHeapClass = 2;
                        view.descriptorIndex = shaderDescriptors;
                        shaderDescriptors += instances;
                    }
                }
                viewMap[sourceUse->id.value] = view.id;
                description.views.push_back(view);
            }
        }
        resourceArtifact.viewCount = static_cast<std::uint32_t>(description.views.size()) - resourceArtifact.firstView;
    }

    if (rtvDescriptors > targetProfile.rtvDescriptorCount ||
        dsvDescriptors > targetProfile.dsvDescriptorCount ||
        shaderDescriptors > targetProfile.shaderDescriptorCount)
        return Failure<LoweredPackageStage>("descriptor-planning", "Planが検証または実行の契約に違反しています。");

    std::map<std::uint32_t, pkg::DynamicSlotId> dynamicSlots;
    std::map<std::uint32_t, pkg::ExternalSlotId> externalSlots;
    std::map<std::uint32_t, pkg::SurfaceSlotId> surfaceSlots;
    for (const auto resourceId : analyzed.canonicalResourceOrder)
    {
        const auto& source = *resources.at(resourceId.value);
        const auto packageResource = resourceMap.at(resourceId.value);
        if (source.update == semantic::UpdateIntent::DynamicPerFrame)
        {
            pkg::DynamicDataSlotArtifact slot;
            slot.id = {static_cast<std::uint32_t>(description.dynamicSlots.size())};
            slot.destinationResource = packageResource;
            slot.requiredBytes = source.dynamicData.requiredBytes;
            slot.requiredAlignment = source.dynamicData.requiredAlignment;
            dynamicSlots[source.id.value] = slot.id;
            description.dynamicSlots.push_back(slot);
        }
        if (source.lifetime == semantic::LifetimeIntent::External &&
            source.kind != semantic::ResourceKind::SurfaceImage)
        {
            pkg::ExternalResourceSlotArtifact slot;
            slot.id = {static_cast<std::uint32_t>(description.externalSlots.size())};
            slot.resource = packageResource;
            slot.requiredKind = ResourceKind(source.kind);
            slot.requiredFormat = ResourceFormat(source);
            slot.minimumBytes = source.kind == semantic::ResourceKind::Buffer ? source.buffer.sizeBytes : 0;
            // Stage F finalizes the boundary states from canonical first/last use,
            // not from ResourceUse registration order.
            slot.requiredIncomingState = Common();
            slot.guaranteedOutgoingState = Common();
            description.resources[packageResource.value].initialState = Common();
            externalSlots[source.id.value] = slot.id;
            description.externalSlots.push_back(slot);
        }
        if (source.kind == semantic::ResourceKind::SurfaceImage)
        {
            pkg::SurfaceSlotArtifact slot;
            slot.id = {static_cast<std::uint32_t>(description.surfaceSlots.size())};
            slot.imageResource = packageResource;
            slot.requiredFormat = ResourceFormat(source);
            slot.acquiredState = Present();
            slot.presentedState = Present();
            surfaceSlots[source.id.value] = slot.id;
            description.surfaceSlots.push_back(slot);
        }
    }

    if (selectedPlan != nullptr)
    {
        for (const auto& planned : selectedPlan->allocations)
        {
            if (planned.id != description.allocations.size() || planned.resources.empty())
                return Failure<LoweredPackageStage>("verified-plan-lowering", "AllocationがCanonicalな順序または識別子規則に違反しています。");
            const auto firstSourceId = planned.resources.front().value;
            const auto& firstSource = *resources.at(firstSourceId);
            pkg::AllocationArtifact allocation;
            allocation.id = {planned.id};
            allocation.kind = planned.kind == planning::PlanAllocationKind::Placed ?
                pkg::AllocationKind::Placed : pkg::AllocationKind::Committed;
            allocation.heapClass = HeapClassFor(firstSource, resourceUses[firstSourceId]);
            allocation.physicalInstanceCount = planned.physicalInstanceCount;
            allocation.alignment = planned.alignment;
            allocation.sizeBytes = planned.sizeBytes;
            allocation.aliasGroup = planned.aliasGroup;
            description.allocations.push_back(allocation);
            for (const auto sourceId : planned.resources)
            {
                const auto packageResource = resourceMap.at(sourceId.value);
                description.resources[packageResource.value].allocation = allocation.id;
                if (planned.resources.size() > 1)
                    description.resources[packageResource.value].flags |= static_cast<std::uint32_t>(pkg::ResourceFlags::Aliased);
            }
        }
    }
    else
    {
        // Stage D: alias intent belongs to Resource, not to a shader-input role.
        // SemanticAnalysis has already validated shape compatibility and exclusive
        // ownership of each Preparation resource.
        std::map<std::uint32_t, analysis::ResourceLifetime> lifetimes;
        for (const auto& lifetime : analyzed.resourceLifetimes)
            lifetimes[lifetime.resource.value] = lifetime;
        std::map<std::uint32_t, std::uint32_t> aliasPairs;
        for (const auto resourceId : analyzed.canonicalResourceOrder)
        {
            const auto& targetResource = *resources.at(resourceId.value);
            if (!targetResource.aliasPreparation.IsValid()) continue;
            const auto& preparation = *resources.at(targetResource.aliasPreparation.value);
            const auto& targetLifetime = lifetimes.at(targetResource.id.value);
            const auto& preparationLifetime = lifetimes.at(preparation.id.value);
            const bool nonOverlapping = !preparationLifetime.usedByWork || !targetLifetime.usedByWork ||
                preparationLifetime.lastUse < targetLifetime.firstUse ||
                targetLifetime.lastUse < preparationLifetime.firstUse;
            if (!nonOverlapping)
                return Failure<LoweredPackageStage>("allocation-planning", "検証または実行の契約に違反しています。");
            aliasPairs[preparation.id.value] = targetResource.id.value;
        }

        std::set<std::uint32_t> allocatedResources;
        std::uint32_t aliasGroup = 0;
        for (const auto resourceId : analyzed.canonicalResourceOrder)
        {
            const auto packageResource = resourceMap.at(resourceId.value);
            const auto& source = *resources.at(resourceId.value);
            if (source.lifetime == semantic::LifetimeIntent::External || allocatedResources.contains(resourceId.value))
                continue;

            const auto pairAsPreparation = aliasPairs.find(resourceId.value);
            auto pairAsTarget = std::find_if(aliasPairs.begin(), aliasPairs.end(), [&](const auto& pair) {
                return pair.second == resourceId.value;
            });
            const bool aliased = pairAsPreparation != aliasPairs.end() || pairAsTarget != aliasPairs.end();
            std::uint32_t partnerId = package::InvalidIndex;
            if (pairAsPreparation != aliasPairs.end()) partnerId = pairAsPreparation->second;
            if (pairAsTarget != aliasPairs.end()) partnerId = pairAsTarget->first;

            const auto& sourceUseList = resourceUses[source.id.value];
            pkg::AllocationArtifact allocation;
            allocation.id = {static_cast<std::uint32_t>(description.allocations.size())};
            allocation.kind = aliased ? pkg::AllocationKind::Placed : pkg::AllocationKind::Committed;
            allocation.heapClass = HeapClassFor(source, sourceUseList);
            allocation.physicalInstanceCount = description.resources[packageResource.value].physicalInstanceCount;
            allocation.alignment = source.update == semantic::UpdateIntent::DynamicPerFrame ?
                ConstantPlacementAlignment : DefaultPlacementAlignment;
            allocation.sizeBytes = source.kind == semantic::ResourceKind::Buffer ?
                (aliased ? AlignUp(source.buffer.sizeBytes, DefaultPlacementAlignment) :
                 description.resources[packageResource.value].sizeBytes) : 0;
            allocation.aliasGroup = aliased ? aliasGroup++ : package::InvalidIndex;
            description.allocations.push_back(allocation);

            description.resources[packageResource.value].allocation = allocation.id;
            allocatedResources.insert(source.id.value);
            if (aliased)
            {
                const auto partnerPackage = resourceMap.at(partnerId);
                if (description.resources[partnerPackage.value].physicalInstanceCount != allocation.physicalInstanceCount)
                    return Failure<LoweredPackageStage>("allocation-planning", "検証または実行の契約に違反しています。");
                description.resources[partnerPackage.value].allocation = allocation.id;
                description.resources[packageResource.value].flags |= static_cast<std::uint32_t>(pkg::ResourceFlags::Aliased);
                description.resources[partnerPackage.value].flags |= static_cast<std::uint32_t>(pkg::ResourceFlags::Aliased);
                allocatedResources.insert(partnerId);
            }
        }
    }

    std::map<std::uint32_t, ShaderIds> shaderMap;
    for (const auto& compiledProgram : compiledPrograms.programs)
    {
        ShaderIds ids;
        for (const auto& shader : compiledProgram.shaders)
        {
            pkg::ShaderStage packageStage{};
            pkg::ShaderId* destination = nullptr;
            if (shader.stage == semantic::ShaderStage::Vertex)
            {
                packageStage = pkg::ShaderStage::Vertex;
                destination = &ids.vertex;
            }
            else if (shader.stage == semantic::ShaderStage::Pixel)
            {
                packageStage = pkg::ShaderStage::Pixel;
                destination = &ids.pixel;
            }
            else if (shader.stage == semantic::ShaderStage::Compute)
            {
                packageStage = pkg::ShaderStage::Compute;
                destination = &ids.compute;
            }
            else
            {
                return Failure<LoweredPackageStage>("package-lowering", "Shaderが検証または実行の契約に違反しています。");
            }
            *destination = {static_cast<std::uint32_t>(description.shaders.size())};
            const auto offset = AppendAligned(description.shaderData, shader.bytecode);
            description.shaders.push_back({*destination, packageStage, pkg::ShaderBinaryFormat::Dxbc,
                targetProfile.shaderModelMajor, targetProfile.shaderModelMinor, 0,
                {package::SectionKind::ShaderData, 0, offset, shader.bytecode.size()},
                base::Sha256(shader.bytecode)});
        }
        shaderMap[compiledProgram.sourceProgram.value] = ids;
    }

    std::map<std::uint32_t, WorkArtifacts> workArtifacts;
    std::uint32_t descriptorBindingOffset = 0;
    std::uint32_t staticSamplerOffset = 0;
    for (const auto workId : workOrder)
    {
        const auto& work = *works.at(workId.value);
        if (work.kind != semantic::WorkKind::Raster && work.kind != semantic::WorkKind::Compute) continue;
        const auto sourceProgramId = work.kind == semantic::WorkKind::Raster ?
            work.raster.program : work.compute.program;
        const auto& sourceProgram = *programs.at(sourceProgramId.value);

        std::vector<const semantic::WorkOperand*> bindingOperands;
        for (const auto& operand : work.operands)
            if (operand.kind == semantic::WorkOperandKind::ProgramParameter)
                bindingOperands.push_back(&operand);
        std::sort(bindingOperands.begin(), bindingOperands.end(), [](const auto* left, const auto* right) {
            return left->parameter.value < right->parameter.value;
        });

        const std::uint32_t rootCost = static_cast<std::uint32_t>(std::count_if(
            bindingOperands.begin(), bindingOperands.end(), [&](const auto* operand) {
                return FindParameter(sourceProgram.interface, operand->parameter)->kind ==
                       semantic::ProgramParameterKind::ConstantBuffer;
            })) * 2u + static_cast<std::uint32_t>(std::count_if(
            bindingOperands.begin(), bindingOperands.end(), [&](const auto* operand) {
                return FindParameter(sourceProgram.interface, operand->parameter)->kind !=
                       semantic::ProgramParameterKind::ConstantBuffer;
            }));
        if (rootCost > 64)
            return Failure<LoweredPackageStage>("binding-layout", "入力または内部状態が検証または実行の契約に違反しています。");

        const pkg::BindingLayoutId layoutId{static_cast<std::uint32_t>(description.bindingLayouts.size())};
        const auto parameterFirst = static_cast<std::uint32_t>(description.rootParameters.size());
        std::uint32_t descriptorCount = 0;
        std::vector<NativeBinding> nativeBindings;
        bool hasStaticSampler = false;
        for (std::uint32_t index = 0; index < bindingOperands.size(); ++index)
        {
            const auto* operand = bindingOperands[index];
            const auto* use = uses.at(operand->use.value);
            const auto* sourceParameterPointer = FindParameter(sourceProgram.interface, operand->parameter);
            if (sourceParameterPointer == nullptr)
                return Failure<LoweredPackageStage>("binding-layout", "Programが検証または実行の契約に違反しています。");
            const auto& sourceParameter = *sourceParameterPointer;
            pkg::RootParameterArtifact parameter;
            parameter.id = {static_cast<std::uint32_t>(description.rootParameters.size())};
            parameter.rootParameterIndex = index;
            parameter.shaderRegister = sourceParameter.shaderRegister;
            parameter.visibility = sourceParameter.stage == semantic::ShaderStage::Vertex ?
                pkg::ShaderVisibility::Vertex :
                sourceParameter.stage == semantic::ShaderStage::Pixel ?
                pkg::ShaderVisibility::Pixel : pkg::ShaderVisibility::All;
            switch (sourceParameter.kind)
            {
            case semantic::ProgramParameterKind::ConstantBuffer:
                parameter.kind = pkg::RootParameterKind::ConstantBuffer;
                parameter.dynamicSlot = dynamicSlots.at(use->resource.value);
                nativeBindings.push_back({NativeBindingKind::Constant, parameter.shaderRegister, sourceParameter.stage});
                break;
            case semantic::ProgramParameterKind::UnorderedBuffer:
                parameter.kind = pkg::RootParameterKind::UnorderedAccessTable;
                parameter.staticView = viewMap.at(use->id.value);
                nativeBindings.push_back({NativeBindingKind::UnorderedAccess, parameter.shaderRegister, sourceParameter.stage});
                ++descriptorCount;
                break;
            case semantic::ProgramParameterKind::SampledTexture:
                hasStaticSampler = true;
                [[fallthrough]];
            case semantic::ProgramParameterKind::ReadOnlyBuffer:
                parameter.kind = pkg::RootParameterKind::ShaderResourceTable;
                parameter.staticView = viewMap.at(use->id.value);
                nativeBindings.push_back({NativeBindingKind::ShaderResource, parameter.shaderRegister, sourceParameter.stage});
                ++descriptorCount;
                break;
            default:
                return Failure<LoweredPackageStage>("binding-layout", "Programが検証または実行の契約に違反しています。");
            }
            description.rootParameters.push_back(parameter);
        }

        auto rootBytes = SerializeBindingLayout(nativeBindings,
            work.kind == semantic::WorkKind::Raster, hasStaticSampler);
        if (!rootBytes) return base::Failure<LoweredPackageStage, CompileError>(rootBytes.error());
        const auto rootOffset = AppendAligned(description.nativeObjectData, rootBytes.value());
        pkg::BindingLayoutArtifact layout;
        layout.id = layoutId;
        layout.rootSignatureMajor = targetProfile.rootSignatureMajor;
        layout.rootSignatureMinor = targetProfile.rootSignatureMinor;
        layout.parameterRange = {parameterFirst, static_cast<std::uint32_t>(bindingOperands.size())};
        layout.descriptorRange = {descriptorBindingOffset, descriptorCount};
        layout.staticSamplerRange = {staticSamplerOffset, hasStaticSampler ? 1u : 0u};
        layout.serializedRootSignature = {package::SectionKind::NativeObjectData, 0,
                                          rootOffset, rootBytes.value().size()};
        layout.layoutDigest = base::Sha256(rootBytes.value());
        description.bindingLayouts.push_back(layout);
        descriptorBindingOffset += descriptorCount;
        if (hasStaticSampler) ++staticSamplerOffset;

        WorkArtifacts artifacts;
        artifacts.layout = layoutId;
        artifacts.program = {static_cast<std::uint32_t>(description.programs.size())};
        pkg::ProgramArtifact programArtifact;
        programArtifact.id = artifacts.program;
        programArtifact.bindingLayout = layoutId;
        programArtifact.interfaceDigest = InterfaceDigest(sourceProgram.interface);
        const auto shaderIds = shaderMap.at(sourceProgram.id.value);
        if (work.kind == semantic::WorkKind::Raster)
        {
            programArtifact.kind = pkg::ProgramKind::Raster;
            programArtifact.vertexShader = shaderIds.vertex;
            programArtifact.pixelShader = shaderIds.pixel;
        }
        else
        {
            programArtifact.kind = pkg::ProgramKind::Compute;
            programArtifact.computeShader = shaderIds.compute;
        }
        description.programs.push_back(programArtifact);

        if (work.kind == semantic::WorkKind::Raster)
        {
            const auto vertexFirst = static_cast<std::uint32_t>(description.vertexElements.size());
            for (const auto* input : CanonicalVertexInputs(sourceProgram.interface))
            {
                pkg::VertexElementArtifact element;
                element.id = static_cast<std::uint32_t>(description.vertexElements.size());
                element.meaning = static_cast<pkg::VertexMeaning>(input->meaning);
                element.format = input->componentCount == 2 ? pkg::Format::R32G32Float :
                                 input->componentCount == 3 ? pkg::Format::R32G32B32Float :
                                                            pkg::Format::R32G32B32A32Float;
                element.alignedByteOffset = input->byteOffset;
                description.vertexElements.push_back(element);
            }

            const semantic::ResourceUse* vertex = nullptr;
            const semantic::ResourceUse* color = nullptr;
            const semantic::ResourceUse* depth = nullptr;
            for (const auto& operand : work.operands)
            {
                const auto* use = uses.at(operand.use.value);
                if (operand.kind == semantic::WorkOperandKind::VertexData) vertex = use;
                if (operand.kind == semantic::WorkOperandKind::ColorAttachment) color = use;
                if (operand.kind == semantic::WorkOperandKind::DepthAttachment) depth = use;
            }
            artifacts.rasterExecutable = {static_cast<std::uint32_t>(description.executables.size())};
            pkg::RasterExecutableArtifact executable;
            executable.id = artifacts.rasterExecutable;
            executable.program = artifacts.program;
            executable.bindingLayout = layoutId;
            executable.vertexElementRange = {vertexFirst, static_cast<std::uint32_t>(sourceProgram.interface.vertexInputs.size())};
            executable.colorFormatRange = {0, 1};
            executable.colorFormat = ResourceFormat(*resources.at(color->resource.value));
            executable.depthFormat = depth ? ResourceFormat(*resources.at(depth->resource.value)) : pkg::Format::Unknown;
            executable.primitiveTopology = pkg::PrimitiveTopology::TriangleList;
            executable.primitiveTopologyType = pkg::PrimitiveTopologyType::Triangle;
            executable.depthStateId = depth ? 1u : 0u;
            executable.sampleCount = 1;
            executable.specializationDigest = RasterSpecializationDigest(executable);
            description.executables.push_back(executable);

            pkg::AttachmentOperationArtifact attachment;
            attachment.id = {static_cast<std::uint32_t>(description.attachmentOperations.size())};
            attachment.depthLoad = depth ? pkg::AttachmentLoadOp::Clear : pkg::AttachmentLoadOp::Discard;
            attachment.depthStore = depth ? pkg::AttachmentStoreOp::Store : pkg::AttachmentStoreOp::Discard;
            description.attachmentOperations.push_back(attachment);

            artifacts.rasterCommand = {static_cast<std::uint32_t>(description.rasterCommands.size())};
            pkg::RasterCommandArtifact command;
            command.id = artifacts.rasterCommand;
            command.executable = artifacts.rasterExecutable;
            command.vertexViewRange = {viewMap.at(vertex->id.value).value, 1};
            command.colorAttachmentRange = {viewMap.at(color->id.value).value, 1};
            if (depth) command.depthAttachment = viewMap.at(depth->id.value);
            command.vertexCount = work.raster.vertexCount;
            command.instanceCount = 1;
            command.viewportId = 0;
            command.scissorId = 0;
            command.attachmentOperation = attachment.id;
            description.rasterCommands.push_back(command);
        }
        else
        {
            artifacts.computeExecutable = {static_cast<std::uint32_t>(description.computeExecutables.size())};
            pkg::ComputeExecutableArtifact executable;
            executable.id = artifacts.computeExecutable;
            executable.program = artifacts.program;
            executable.bindingLayout = layoutId;
            executable.specializationDigest = ComputeSpecializationDigest(executable);
            description.computeExecutables.push_back(executable);

            artifacts.computeCommand = {static_cast<std::uint32_t>(description.computeCommands.size())};
            description.computeCommands.push_back({artifacts.computeCommand, artifacts.computeExecutable,
                work.compute.threadGroupCountX, work.compute.threadGroupCountY,
                work.compute.threadGroupCountZ, 0});
        }
        workArtifacts[work.id.value] = artifacts;
    }

    const pkg::QueueId noQueue{};
    const pkg::QueueId directQueue{0};
    const pkg::QueueId computeQueue = targetProfile.computeQueueCount != 0 ?
        pkg::QueueId{targetProfile.directQueueCount} : directQueue;
    const pkg::QueueId copyQueue = targetProfile.copyQueueCount != 0 ?
        pkg::QueueId{targetProfile.directQueueCount + targetProfile.computeQueueCount} : directQueue;
    std::map<std::uint32_t, pkg::QueueId> workQueues;
    if (selectedPlan == nullptr)
    {
        for (const auto workId : workOrder)
        {
            const auto kind = works.at(workId.value)->kind;
            workQueues[workId.value] = kind == semantic::WorkKind::Copy ? copyQueue :
                                       kind == semantic::WorkKind::Compute ? computeQueue : directQueue;
        }
    }
    else
    {
        for (const auto& assignment : selectedPlan->queueAssignments)
        {
            pkg::QueueId queue;
            if (assignment.queueClass == planning::QueueClass::Direct)
                queue = {assignment.queueIndex};
            else if (assignment.queueClass == planning::QueueClass::Compute)
                queue = {targetProfile.directQueueCount + assignment.queueIndex};
            else
                queue = {targetProfile.directQueueCount + targetProfile.computeQueueCount + assignment.queueIndex};
            workQueues[assignment.work.value] = queue;
        }
    }

    // SignalPointId is local to one operation stream.  Frame points are assigned
    // before lowering so WaitTemporal can reference a producer that appears later
    // in the current frame but belongs to the previous frame generation.
    std::map<std::uint32_t, pkg::SignalPointId> workSignalPoints;
    std::uint32_t nextFrameSignalPoint = 0;
    for (const auto workId : workOrder)
        workSignalPoints[workId.value] = {nextFrameSignalPoint++};
    const pkg::SignalPointId presentSignalPoint = description.surfaceSlots.empty() ?
        pkg::SignalPointId{} : pkg::SignalPointId{nextFrameSignalPoint++};

    std::map<std::uint32_t, std::uint32_t> temporalCurrentWriterWork;
    for (const auto workId : workOrder)
    {
        const auto& work = *works.at(workId.value);
        for (const auto& operand : work.operands)
        {
            const auto* use = uses.at(operand.use.value);
            const auto& resource = *resources.at(use->resource.value);
            if (resource.lifetime == semantic::LifetimeIntent::Temporal &&
                use->temporalRelation == semantic::TemporalRelation::Current &&
                (use->effect == semantic::Effect::Write || use->effect == semantic::Effect::ReadWrite))
                temporalCurrentWriterWork[use->resource.value] = work.id.value;
        }
    }

    const auto addOperation = [&](pkg::D3D12OperationCode code, pkg::QueueId queue,
                                  std::vector<std::byte> payload = {})
    {
        const auto operationVersion = pkg::OperationVersion(code);
        description.operations.push_back({code, operationVersion, 0, queue, std::move(payload)});
    };

    addOperation(pkg::D3D12OperationCode::CreateDescriptorHeaps, noQueue);
    for (const auto& resource : description.resources)
        if (resource.origin == pkg::ResourceOrigin::PackageOwned)
            addOperation(pkg::D3D12OperationCode::CreateResource, noQueue,
                         pkg::Encode(pkg::CreateResourcePayload{resource.id}));

    std::vector<std::uint32_t> loadResources;
    for (const auto resourceId : analyzed.canonicalResourceOrder)
        if (resources.at(resourceId.value)->lifetime == semantic::LifetimeIntent::Preparation)
            loadResources.push_back(resourceId.value);
    for (const auto resourceId : analyzed.canonicalResourceOrder)
        if (resources.at(resourceId.value)->lifetime != semantic::LifetimeIntent::Preparation &&
            resources.at(resourceId.value)->lifetime != semantic::LifetimeIntent::External)
            loadResources.push_back(resourceId.value);

    const bool hasLoadWork = std::any_of(loadResources.begin(), loadResources.end(), [&](std::uint32_t id) {
        const auto packageResource = resourceMap.at(id);
        return description.resources[packageResource.value].initialDataSize != 0 ||
               (description.resources[packageResource.value].flags & static_cast<std::uint32_t>(pkg::ResourceFlags::Aliased)) != 0;
    });
    if (hasLoadWork)
    {
        addOperation(pkg::D3D12OperationCode::BeginQueueBatch, copyQueue);
        std::map<std::uint32_t, pkg::ResourceId> activeAlias;
        for (const auto sourceId : loadResources)
        {
            const auto packageResource = resourceMap.at(sourceId);
            const auto& source = *resources.at(sourceId);
            const auto& artifact = description.resources[packageResource.value];
            const bool aliased = (artifact.flags & static_cast<std::uint32_t>(pkg::ResourceFlags::Aliased)) != 0;
            if (aliased)
            {
                const auto allocation = description.allocations[artifact.allocation.value];
                auto before = activeAlias.find(allocation.aliasGroup);
                addOperation(pkg::D3D12OperationCode::ActivateAlias, copyQueue,
                    pkg::Encode(pkg::ActivateAliasPayload{
                        before == activeAlias.end() ? pkg::ResourceId{} : before->second, packageResource}));
                activeAlias[allocation.aliasGroup] = packageResource;
            }
            if (artifact.initialDataSize == 0) continue;
            addOperation(pkg::D3D12OperationCode::InitializeState, copyQueue,
                pkg::Encode(pkg::InitializeStatePayload{packageResource, artifact.initialState,
                    Explicit(pkg::ExplicitStateBits::CopyDestination)}));
            if (source.kind == semantic::ResourceKind::Buffer)
            {
                addOperation(pkg::D3D12OperationCode::UploadBuffer, copyQueue,
                    pkg::Encode(pkg::UploadBufferPayload{packageResource, artifact.initialDataOffset,
                                                         artifact.initialDataSize}));
                addOperation(pkg::D3D12OperationCode::InitializeState, copyQueue,
                    pkg::Encode(pkg::InitializeStatePayload{packageResource,
                        Explicit(pkg::ExplicitStateBits::CopyDestination),
                        Explicit(pkg::ExplicitStateBits::CopySource)}));
                addOperation(pkg::D3D12OperationCode::VerifyBufferContents, copyQueue,
                    pkg::Encode(pkg::VerifyBufferContentsPayload{packageResource, 0,
                        artifact.initialDataOffset, artifact.initialDataSize}));
            }
            else
            {
                addOperation(pkg::D3D12OperationCode::UploadTexture, copyQueue,
                    pkg::Encode(pkg::UploadTexturePayload{packageResource, artifact.initialDataOffset,
                        source.texture2D.rowBytes, static_cast<std::uint32_t>(artifact.initialDataSize), 0, 0, 0, 0}));
                addOperation(pkg::D3D12OperationCode::InitializeState, copyQueue,
                    pkg::Encode(pkg::InitializeStatePayload{packageResource,
                        Explicit(pkg::ExplicitStateBits::CopyDestination),
                        Explicit(pkg::ExplicitStateBits::CopySource)}));
                addOperation(pkg::D3D12OperationCode::VerifyTextureContents, copyQueue,
                    pkg::Encode(pkg::VerifyTextureContentsPayload{packageResource, artifact.initialDataOffset,
                        source.texture2D.rowBytes, source.texture2D.width, source.texture2D.height, 0}));
            }
            addOperation(pkg::D3D12OperationCode::InitializeState, copyQueue,
                pkg::Encode(pkg::InitializeStatePayload{packageResource,
                    Explicit(pkg::ExplicitStateBits::CopySource), Common()}));
        }
        addOperation(pkg::D3D12OperationCode::EndQueueBatch, copyQueue);
        addOperation(pkg::D3D12OperationCode::SignalQueue, copyQueue,
                     pkg::Encode(pkg::SignalQueuePayload{pkg::SignalPointId{0}}));
    }

    for (const auto& layout : description.bindingLayouts)
        addOperation(pkg::D3D12OperationCode::CreateRootSignature, noQueue,
                     pkg::Encode(pkg::CreateRootSignaturePayload{layout.id}));
    for (const auto& executable : description.executables)
        addOperation(pkg::D3D12OperationCode::CreateGraphicsPipeline, noQueue,
                     pkg::Encode(pkg::CreateGraphicsPipelinePayload{executable.id}));
    for (const auto& executable : description.computeExecutables)
        addOperation(pkg::D3D12OperationCode::CreateComputePipeline, noQueue,
                     pkg::Encode(pkg::CreateComputePipelinePayload{executable.id}));

    const auto loadCount = static_cast<std::uint32_t>(description.operations.size());
    for (const auto& slot : description.dynamicSlots)
        addOperation(pkg::D3D12OperationCode::ApplyDynamicData, noQueue,
                     pkg::Encode(pkg::ApplyDynamicDataPayload{slot.id}));
    for (const auto& slot : description.externalSlots)
        addOperation(pkg::D3D12OperationCode::AcquireExternal, noQueue,
                     pkg::Encode(pkg::AcquireExternalPayload{slot.id}));
    for (const auto& slot : description.surfaceSlots)
        addOperation(pkg::D3D12OperationCode::AcquireSurfaceImage, noQueue,
                     pkg::Encode(pkg::AcquireSurfaceImagePayload{slot.id}));

    std::map<std::uint32_t, std::uint32_t> workPosition;
    for (std::uint32_t index = 0; index < workOrder.size(); ++index)
        workPosition[workOrder[index].value] = index;
    std::map<std::uint32_t, std::uint32_t> firstExternalUse;
    std::map<std::uint32_t, std::uint32_t> lastExternalUse;
    std::map<std::uint32_t, const semantic::ResourceUse*> firstExternalUseContract;
    std::map<std::uint32_t, const semantic::ResourceUse*> lastExternalUseContract;
    for (std::uint32_t position = 0; position < workOrder.size(); ++position)
    {
        const auto& work = *works.at(workOrder[position].value);
        for (const auto* operand : CanonicalOperands(work))
        {
            const auto* use = uses.at(operand->use.value);
            if (!externalSlots.contains(use->resource.value)) continue;
            if (!firstExternalUse.contains(use->resource.value))
            {
                firstExternalUse[use->resource.value] = position;
                firstExternalUseContract[use->resource.value] = use;
            }
            lastExternalUse[use->resource.value] = position;
            lastExternalUseContract[use->resource.value] = use;
        }
    }
    for (const auto& [resourceId, slotId] : externalSlots)
    {
        const auto firstUse = firstExternalUseContract.at(resourceId);
        const auto lastUse = lastExternalUseContract.at(resourceId);
        auto& slot = description.externalSlots[slotId.value];
        slot.requiredIncomingState = RequiredState(firstUse->role,
            useWorkKinds.at(firstUse->id.value));
        slot.guaranteedOutgoingState = RequiredState(lastUse->role,
            useWorkKinds.at(lastUse->id.value));
        description.resources[slot.resource.value].initialState = slot.requiredIncomingState;
    }

    const auto cellFor = [&](const semantic::ResourceUse& use) {
        const auto& source = *resources.at(use.resource.value);
        std::uint32_t relation = 0;
        if (source.lifetime == semantic::LifetimeIntent::Temporal)
            relation = use.temporalRelation == semantic::TemporalRelation::Previous ? 2u : 1u;
        return StateCell{resourceMap.at(use.resource.value).value, relation};
    };
    const auto baselineState = [&](StateCell cell) {
        const auto& artifact = description.resources[cell.resource];
        if (artifact.initialDataSize != 0) return Common();
        return artifact.initialState;
    };
    std::map<StateCell, pkg::ResourceState> states;
    const auto currentState = [&](StateCell cell) {
        const auto found = states.find(cell);
        return found == states.end() ? baselineState(cell) : found->second;
    };
    const auto emitTransition = [&](pkg::QueueId queue, pkg::ViewId view, StateCell cell,
                                    pkg::ResourceState after)
    {
        const auto before = currentState(cell);
        if (before == after) return;
        addOperation(pkg::D3D12OperationCode::Transition, queue,
            pkg::Encode(pkg::TransitionPayload{view, 0, before, after}));
        states[cell] = after;
    };

    for (std::uint32_t position = 0; position < workOrder.size(); ++position)
    {
        const auto workId = workOrder[position];
        const auto& work = *works.at(workId.value);
        const auto queue = workQueues.at(work.id.value);

        // One wait per producer queue is sufficient, but it must reference the
        // latest producer Work on that queue, not whichever dependency happens
        // to appear first in the analysis record vector.
        std::map<std::uint32_t, std::pair<std::uint32_t, pkg::SignalPointId>> queueWaits;
        for (const auto& edge : analyzed.dependencies)
        {
            if (edge.consumer != work.id) continue;
            const auto producerQueue = workQueues.at(edge.producer.value);
            if (producerQueue == queue) continue;
            const auto producerPosition = workPosition.at(edge.producer.value);
            auto found = queueWaits.find(producerQueue.value);
            if (found == queueWaits.end() || producerPosition > found->second.first)
                queueWaits[producerQueue.value] = {producerPosition, workSignalPoints.at(edge.producer.value)};
        }
        for (const auto& [producerQueue, wait] : queueWaits)
            addOperation(pkg::D3D12OperationCode::WaitQueue, queue,
                         pkg::Encode(pkg::WaitQueuePayload{wait.second}));
        const auto canonicalOperands = CanonicalOperands(work);
        for (const auto* operand : canonicalOperands)
        {
            const auto* use = uses.at(operand->use.value);
            const auto first = firstExternalUse.find(use->resource.value);
            if (first != firstExternalUse.end() && first->second == position)
                addOperation(pkg::D3D12OperationCode::WaitExternal, queue,
                             pkg::Encode(pkg::WaitExternalPayload{externalSlots.at(use->resource.value)}));
            if (use->temporalRelation == semantic::TemporalRelation::Previous)
                addOperation(pkg::D3D12OperationCode::WaitTemporal, queue,
                             pkg::Encode(pkg::WaitTemporalPayload{resourceMap.at(use->resource.value),
                                 workSignalPoints.at(temporalCurrentWriterWork.at(use->resource.value))}));
        }

        addOperation(pkg::D3D12OperationCode::BeginQueueBatch, queue);
        std::map<StateCell, const semantic::ResourceUse*> activeUses;
        std::set<std::uint32_t> rasterPresentationResources;
        if (work.kind == semantic::WorkKind::Raster)
            for (const auto* operand : canonicalOperands)
            {
                const auto* use = uses.at(operand->use.value);
                if (operand->kind == semantic::WorkOperandKind::PresentSource)
                    rasterPresentationResources.insert(use->resource.value);
            }

        for (const auto* operand : canonicalOperands)
        {
            const auto* use = uses.at(operand->use.value);
            if (work.kind == semantic::WorkKind::Raster && use->role == semantic::ViewRole::PresentSource)
                continue;
            const auto cell = cellFor(*use);
            const auto required = RequiredState(use->role, work.kind);
            const auto existing = activeUses.find(cell);
            if (existing != activeUses.end() && RequiredState(existing->second->role, work.kind) != required)
                return Failure<LoweredPackageStage>("state-planning", "Stateの状態または世代が実行契約と一致しません。");
            activeUses[cell] = use;
            emitTransition(queue, viewMap.at(use->id.value), cell, required);
        }

        if (work.kind == semantic::WorkKind::Copy)
        {
            const semantic::ResourceUse* sourceUse = nullptr;
            const semantic::ResourceUse* destinationUse = nullptr;
            for (const auto* operand : canonicalOperands)
            {
                if (operand->kind == semantic::WorkOperandKind::CopySource)
                    sourceUse = uses.at(operand->use.value);
                if (operand->kind == semantic::WorkOperandKind::CopyDestination)
                    destinationUse = uses.at(operand->use.value);
            }
            if (sourceUse == nullptr || destinationUse == nullptr)
                return Failure<LoweredPackageStage>("operation-lowering", "Workが検証または実行の契約に違反しています。");
            addOperation(pkg::D3D12OperationCode::ExecuteCopy, queue,
                pkg::Encode(pkg::CopyBufferPayload{viewMap.at(sourceUse->id.value),
                    viewMap.at(destinationUse->id.value), 0, 0, work.copy.bytes}));
        }
        else if (work.kind == semantic::WorkKind::Compute)
        {
            addOperation(pkg::D3D12OperationCode::ExecuteCompute, queue,
                pkg::Encode(pkg::ExecuteComputePayload{workArtifacts.at(work.id.value).computeCommand}));
        }
        else if (work.kind == semantic::WorkKind::Raster)
        {
            addOperation(pkg::D3D12OperationCode::ExecuteRaster, queue,
                pkg::Encode(pkg::ExecuteRasterPayload{workArtifacts.at(work.id.value).rasterCommand}));
            for (const auto* operand : canonicalOperands)
            {
                const auto* use = uses.at(operand->use.value);
                if (operand->kind != semantic::WorkOperandKind::PresentSource) continue;
                const auto cell = cellFor(*use);
                emitTransition(queue, viewMap.at(use->id.value), cell, Present());
                activeUses[cell] = use;
            }
        }

        for (const auto& [cell, use] : activeUses)
        {
            if (rasterPresentationResources.contains(use->resource.value)) continue;
            pkg::ResourceState desired = baselineState(cell);
            bool nextFound = false;
            for (std::uint32_t next = position + 1; next < workOrder.size() && !nextFound; ++next)
            {
                const auto& nextWork = *works.at(workOrder[next].value);
                for (const auto* nextOperand : CanonicalOperands(nextWork))
                {
                    const auto* nextUse = uses.at(nextOperand->use.value);
                    if (cellFor(*nextUse) != cell) continue;
                    desired = workQueues.at(nextWork.id.value) == queue ? RequiredState(nextUse->role, nextWork.kind) : Common();
                    nextFound = true;
                    break;
                }
            }
            if (!nextFound && externalSlots.contains(use->resource.value))
                desired = description.externalSlots[externalSlots.at(use->resource.value).value].guaranteedOutgoingState;
            emitTransition(queue, viewMap.at(use->id.value), cell, desired);
        }
        addOperation(pkg::D3D12OperationCode::EndQueueBatch, queue);
        addOperation(pkg::D3D12OperationCode::SignalQueue, queue,
                     pkg::Encode(pkg::SignalQueuePayload{workSignalPoints.at(work.id.value)}));
    }

    for (const auto& slot : description.surfaceSlots)
        addOperation(pkg::D3D12OperationCode::PresentSurface, noQueue,
                     pkg::Encode(pkg::PresentSurfacePayload{slot.id}));
    if (!description.surfaceSlots.empty())
        addOperation(pkg::D3D12OperationCode::SignalQueue, directQueue,
                     pkg::Encode(pkg::SignalQueuePayload{presentSignalPoint}));
    for (const auto& slot : description.externalSlots)
    {
        const auto sourceResource = std::find_if(resourceMap.begin(), resourceMap.end(), [&](const auto& pair) {
            return pair.second == slot.resource;
        });
        if (sourceResource == resourceMap.end())
            return Failure<LoweredPackageStage>("external-boundary", "検証または実行の契約に違反しています。");
        const auto lastPosition = lastExternalUse.at(sourceResource->first);
        const auto lastWork = workOrder[lastPosition];
        addOperation(pkg::D3D12OperationCode::ReleaseExternal, noQueue,
                     pkg::Encode(pkg::ReleaseExternalPayload{slot.id, workSignalPoints.at(lastWork.value)}));
    }

    description.operationStreams.push_back({pkg::OperationStreamKind::Load, 0, loadCount, 0});
    description.operationStreams.push_back({pkg::OperationStreamKind::Frame, loadCount,
        static_cast<std::uint32_t>(description.operations.size()) - loadCount, 0});

    LoweredPackageStage output;
    output.description = std::move(description);
    return base::Success<LoweredPackageStage, CompileError>(std::move(output));
}

