from pathlib import Path
import re, sys, xml.etree.ElementTree as ET

root=Path(__file__).resolve().parents[1]
ns={'m':'http://schemas.microsoft.com/developer/msbuild/2003'}
errors=[]; guids={}; compile_owners={}; include_owners={}; graph={}; object_names={}
projects=sorted(root.rglob('*.vcxproj'))
product_projects=sorted((root/'projects').glob('*/*.vcxproj'))

for p in projects:
    try: tree=ET.parse(p)
    except Exception as exc:
        errors.append(f'XML parse failed: {p.relative_to(root)}: {exc}')
        continue
    guid=tree.find('.//m:ProjectGuid',ns)
    if guid is None:
        errors.append(f'Missing GUID: {p.relative_to(root)}')
        continue
    key=guid.text.upper()
    if key in guids: errors.append(f'Duplicate GUID: {key}')
    guids[key]=p
    graph[p.resolve()]=[]
    for item_name in ('ClCompile','ClInclude'):
        for src in tree.findall(f'.//m:{item_name}',ns):
            inc=src.get('Include')
            if not inc: continue
            target=(p.parent/inc.replace('\\','/')).resolve()
            if not target.exists(): errors.append(f'Missing {item_name} source: {p.relative_to(root)} -> {target}')
            if root/'projects' in p.parents:
                if item_name == 'ClCompile':
                    compile_owners.setdefault(target,[]).append(p)
                else:
                    include_owners.setdefault(target,[]).append(p)
            if item_name == 'ClCompile' and root/'projects' in p.parents:
                obj=src.find('m:ObjectFileName',ns)
                if obj is None or not (obj.text or '').strip():
                    errors.append(f'Missing unique ObjectFileName: {p.relative_to(root)} -> {target}')
                else:
                    key=(p.resolve(),obj.text.casefold())
                    object_names.setdefault(key,[]).append(target)
    for ref in tree.findall('.//m:ProjectReference',ns):
        inc=ref.get('Include')
        if not inc: continue
        target=(p.parent/inc.replace('\\','/')).resolve()
        graph[p.resolve()].append(target)
        if not target.exists():
            errors.append(f'Missing reference: {p.relative_to(root)} -> {target}')
            continue
        target_tree=ET.parse(target)
        target_guid=target_tree.find('.//m:ProjectGuid',ns)
        declared=ref.find('m:Project',ns)
        if target_guid is not None and declared is not None and target_guid.text.upper()!=declared.text.upper():
            errors.append(f'Reference GUID mismatch: {p.relative_to(root)} -> {target.relative_to(root)}')

if len(projects)!=18: errors.append(f'Expected 18 projects, found {len(projects)}')
if len(product_projects)!=15: errors.append(f'Expected 15 product projects, found {len(product_projects)}')
if (root/'legacy').exists(): errors.append('legacy/ still exists')
if (root/'src/internal').exists(): errors.append('src/internal/ still exists')

# Every active product translation unit has exactly one owner.
active_cpp={p.resolve() for p in (root/'src').rglob('*.cpp')}
owned_cpp=set(compile_owners)
for source,owners in compile_owners.items():
    if len(owners)!=1:
        errors.append(f'Product source owner count is {len(owners)}: {source.relative_to(root)}')
for source in sorted(active_cpp-owned_cpp): errors.append(f'Unowned active source: {source.relative_to(root)}')
for source in sorted(owned_cpp-active_cpp):
    if root/'src' in source.parents: errors.append(f'Project owns non-active source: {source.relative_to(root)}')


# Every active product header is listed by exactly one product project.
active_headers={p.resolve() for p in (root/'src').rglob('*.h')}
owned_headers=set(include_owners)
for source,owners in include_owners.items():
    if len(owners)!=1:
        errors.append(f'Product header owner count is {len(owners)}: {source.relative_to(root)}')
for source in sorted(active_headers-owned_headers): errors.append(f'Unowned active header: {source.relative_to(root)}')
for source in sorted(owned_headers-active_headers):
    if root/'src' in source.parents: errors.append(f'Project owns non-active header: {source.relative_to(root)}')

# .filters is an IDE view only, but it must not drift from the owning project.
for project in projects:
    filters=project.with_suffix(project.suffix+'.filters')
    if not filters.exists():
        errors.append(f'Missing filters file: {project.relative_to(root)}')
        continue
    try:
        filter_tree=ET.parse(filters)
    except Exception as exc:
        errors.append(f'Filters XML parse failed: {filters.relative_to(root)}: {exc}')
        continue
    project_tree=ET.parse(project)
    for item_name in ('ClCompile','ClInclude'):
        project_items={x.get('Include').replace('\\','/') for x in project_tree.findall(f'.//m:{item_name}',ns) if x.get('Include')}
        filter_items={x.get('Include').replace('\\','/') for x in filter_tree.findall(f'.//m:{item_name}',ns) if x.get('Include')}
        if project_items != filter_items:
            errors.append(f'{item_name} filters mismatch: {filters.relative_to(root)}')
        for inc in filter_items:
            target=(filters.parent/inc).resolve()
            if not target.exists(): errors.append(f'Missing filters source: {filters.relative_to(root)} -> {target}')

for (project,obj),sources in object_names.items():
    if len(sources)!=1:
        errors.append(f'ObjectFileName collision in {project.parent.name}: {obj}')

# Project references must be acyclic.
visited=set(); stack=[]
def visit(node):
    if node in stack:
        errors.append('ProjectReference cycle: '+' -> '.join(x.parent.name for x in stack+[node]))
        return
    if node in visited: return
    stack.append(node)
    for child in graph.get(node,[]):
        if child in graph: visit(child)
    stack.pop(); visited.add(node)
for node in graph: visit(node)

solution=(root/'NewSGE4.sln').read_text(encoding='utf-8-sig')
solution_guids={x.upper() for x in re.findall(r'Project\("\{[^}]+\}"\) = ".*?", ".*?", "(\{[^}]+\})"',solution)}
missing=set(guids)-solution_guids
extra=solution_guids-set(guids)
if missing: errors.append(f'{len(missing)} project GUIDs absent from NewSGE4.sln')
if extra: errors.append(f'{len(extra)} unknown project GUIDs in NewSGE4.sln')
if '66A26720-8FB5-11D2-AA7E-00C04F688DDE' in solution.upper():
    errors.append('Solution-folder project type reintroduced')

map_text=(root/'docs/UNIFIED_INVARIANT_MAP_V1.md').read_text(encoding='utf-8')
ids=re.findall(r'`(V2-R0-I\d{3})`',map_text)
if len(ids)!=40 or len(set(ids))!=40: errors.append('Invariant map is not exactly 40 unique rows')

for p in list((root/'src').rglob('*'))+list((root/'tests').rglob('*')):
    if not p.is_file() or p.suffix not in {'.h','.cpp','.hpp','.cxx','.vcxproj'}: continue
    text=p.read_text(encoding='utf-8')
    for forbidden in ('src/internal','legacy/','legacy\\','sge4::v2','sge4_5','sge4::unified','runtime_v1'):
        if forbidden in text: errors.append(f'Active file contains retired reference {forbidden}: {p.relative_to(root)}')
    if '/reference/' in text.replace('\\','/') or '../reference/' in text.replace('\\','/'):
        errors.append(f'Active source depends on reference/: {p.relative_to(root)}')

required={
'01_CanonicalCore','10_LeafModel','11_LeafVerifier','12_LeafPlanner','13_LeafArtifact',
'20_CompositionModel','21_CompositionPlanner','22_CompositionVerifier','23_CompositionArtifactToolchain',
'30_DynamicModelArtifact','31_DynamicPlanner','32_DynamicVerifier','40_RuntimeCore',
'50_D3D12Compiler','51_D3D12Executor'}
found={p.parent.name for p in product_projects}
if found!=required: errors.append('Product project set does not match the 15 architectural boundaries')

# Proof-boundary project checks.
def refs(name):
    p=root/'projects'/name/(name+'.vcxproj')
    tree=ET.parse(p)
    return {Path(x.get('Include').replace('\\','/')).stem for x in tree.findall('.//m:ProjectReference',ns)}
for planner,verifier in [('12_LeafPlanner','11_LeafVerifier'),('21_CompositionPlanner','22_CompositionVerifier'),('31_DynamicPlanner','32_DynamicVerifier')]:
    if verifier in refs(planner): errors.append(f'{planner} illegally depends on {verifier}')
    if planner in refs(verifier): errors.append(f'{verifier} illegally depends on {planner} implementation')
for forbidden in ('31_DynamicPlanner','32_DynamicVerifier'):
    if forbidden in refs('40_RuntimeCore'): errors.append(f'40_RuntimeCore illegally depends on {forbidden}')
if '50_D3D12Compiler' in refs('51_D3D12Executor'):
    errors.append('51_D3D12Executor illegally depends on 50_D3D12Compiler')
runtime_text='\n'.join(p.read_text(encoding='utf-8') for p in (root/'src/runtime').rglob('*') if p.is_file() and p.suffix in {'.h','.cpp'})
if 'DynamicInvocationPlanner' in runtime_text or 'DynamicInvocationVerifier' in runtime_text:
    errors.append('Runtime source invokes Dynamic Planner/Verifier')
if 'PreviousHistoryIdentity' not in runtime_text:
    errors.append('Runtime source does not bind ContinueHistory submission to the accepted history identity')
dynamic_artifact_text='\n'.join(p.read_text(encoding='utf-8') for p in (root/'src/dynamic/artifact').rglob('*')
                                  if p.is_file() and p.suffix in {'.h','.cpp'})
if 'PreviousHistoryIdentity' not in dynamic_artifact_text:
    errors.append('Frozen Dynamic Invocation does not preserve its previous history identity')
if 'DynamicInvocationVerifier.h' in dynamic_artifact_text:
    errors.append('Dynamic Model/Artifact source depends on the Dynamic Verifier implementation')
windows_qualification=(root/'tests/61_UnifiedWindowsQualification/main.cpp').read_text(encoding='utf-8')
if 'mismatchedContinue' not in windows_qualification or 'Require(!rejected' not in windows_qualification:
    errors.append('Windows資格試験に異なるHistory Identityの拒否ケースがありません')


# Source ownership must match the final responsibilities, not only be unique.
def owned_sources(name):
    project=root/'projects'/name/(name+'.vcxproj')
    tree=ET.parse(project)
    return {(project.parent/x.get('Include').replace('\\','/')).resolve()
            for x in tree.findall('.//m:ClCompile',ns) if x.get('Include')}

if any('/src/composition/artifact/' in x.as_posix() for x in owned_sources('20_CompositionModel')):
    errors.append('20_CompositionModel still owns Composition Artifact implementation')
if not any(x.name == 'VerifiedCompositionArtifact.cpp' for x in owned_sources('23_CompositionArtifactToolchain')):
    errors.append('23_CompositionArtifactToolchain does not own VerifiedCompositionArtifact.cpp')
if any(x.name == 'TargetModel.cpp' for x in owned_sources('10_LeafModel')):
    errors.append('10_LeafModel still owns the D3D12 target implementation')
if not any(x.name == 'TargetModel.cpp' for x in owned_sources('50_D3D12Compiler')):
    errors.append('50_D3D12Compiler does not own TargetModel.cpp')

verifier_text='\n'.join(x.read_text(encoding='utf-8') for x in (root/'src/composition/verifier').rglob('*')
                         if x.is_file() and x.suffix in {'.h','.cpp'})
for forbidden in ('FrozenCompositionReader', 'FrozenCompositionWriter',
                  'FreezeVerifiedComposition', 'ReadVerifiedFrozenComposition'):
    if forbidden in verifier_text:
        errors.append(f'Composition Verifier still owns Artifact operation {forbidden}')
if '23_CompositionArtifactToolchain' in refs('22_CompositionVerifier'):
    errors.append('22_CompositionVerifier illegally depends on Composition ArtifactToolchain')

executor_text='\n'.join(x.read_text(encoding='utf-8') for x in (root/'src/backends/d3d12/executor').rglob('*')
                         if x.is_file() and x.suffix in {'.h','.cpp'})
if 'D3D12PackageLowering' in executor_text:
    errors.append('D3D12 Executor depends on target lowering implementation')

# Frozen Composition ABI 2.5 boundary checks.
abi2_header=(root/'src/composition/artifact/abi2/FrozenCompositionAbi2.h').read_text(encoding='utf-8')
abi2_source=(root/'src/composition/artifact/abi2/FrozenCompositionAbi2.cpp').read_text(encoding='utf-8')
production_reader=(root/'src/composition/artifact/VerifiedCompositionArtifact.cpp').read_text(encoding='utf-8')
toolchain_source=(root/'src/composition/toolchain/CompositionToolchain.cpp').read_text(encoding='utf-8')
migration_root=root/'src/composition/migration/abi1'
if 'FrozenCompositionAbi2FormatMajor = 2' not in abi2_header or 'FrozenCompositionAbi2FormatMinor = 5' not in abi2_header:
    errors.append('Frozen Composition production ABI is not fixed to SGE4UNI 2.5')
for required_kind in ('Manifest','LeafTable','LeafBytes','ContractData','VerifiedDecisionData',
                      'VerificationCertificate','AuthorityLedger','DynamicContract'):
    if required_kind not in abi2_header:
        errors.append(f'Frozen Composition ABI 2.5 is missing direct section {required_kind}')
if 'CompleteComposition' in abi2_header or 'CompleteComposition' in toolchain_source:
    errors.append('Production ABI 2.5 reintroduced the nested CompleteComposition section')
if (root/'src/composition/artifact/container').exists():
    errors.append('Legacy SGE4CMP container still resides in the production artifact tree')
for required in ('FrozenCompositionAbi1Migration.cpp','FrozenCompositionAbi1Migration.h'):
    if not (migration_root/required).exists():
        errors.append(f'Missing explicit ABI 1 migration source: {required}')
if not (migration_root/'container/FrozenCompositionReader.cpp').exists():
    errors.append('Legacy SGE4CMP reader is not isolated under migration/abi1')
if 'FrozenCompositionReader' in production_reader or 'FrozenCompositionWriter' in production_reader:
    errors.append('Production Composition reader directly references the legacy SGE4CMP reader/writer')
if 'ReadVerifiedFrozenCompositionAbi2' not in production_reader:
    errors.append('Production Composition reader does not route exclusively to ABI 2.5')
if 'FrozenCompositionAbi2EmbeddedSchemaVersion = 17' not in abi2_header or    'FrozenCompositionAbi2EmbeddedRuntimeVersion = 17' not in abi2_header:
    errors.append('ABI 2.5 does not explicitly preserve embedded Leaf Schema/Runtime 17')
dynamic_header=(root/'src/dynamic/artifact/DynamicInvocationPackage.h').read_text(encoding='utf-8')
if 'FrozenInvocationFormatMajor = 1' not in dynamic_header or 'FrozenInvocationFormatMinor = 4' not in dynamic_header:
    errors.append('Frozen Dynamic Invocation production ABI is not fixed to SGE4INV 1.4')
if ('ExecutionPayload = 5' not in dynamic_header or 'ConditionalExecution = 6' not in dynamic_header or
        'IndirectDispatch = 7' not in dynamic_header or
        'FrozenDynamicExecutionPayloadV1' not in dynamic_header):
    errors.append('SGE4INV 1.4 does not own verified payload, conditional execution, and indirect dispatch sections')
dynamic_contract_header=(root/'src/composition/model/DynamicExecutionContract.h').read_text(encoding='utf-8')
if 'VerifiedDenseSlot = 1' not in dynamic_contract_header or 'targetDynamicSlot' not in dynamic_contract_header:
    errors.append('Composition does not freeze the Verified Dense Slot execution route')
if ('ConditionalRegionV1' not in dynamic_contract_header or
        'ConditionalPredicateKindV1' not in dynamic_contract_header or
        'conditionalRegions' not in dynamic_contract_header):
    errors.append('Composition does not freeze non-nested Conditional Region contracts')
runtime_session=(root/'src/runtime/session/RuntimeSession.cpp').read_text(encoding='utf-8')
if 'PrepareDynamicExecution' not in runtime_session or 'dynamicExecutionShadow_' not in runtime_session:
    errors.append('Runtime Session does not apply verified execution payloads to a private shadow')
if ('ValidateConditionalExecution' not in runtime_session or
        'conditionalExecutionIdentity' not in runtime_session or
        'enabledLeaves' not in runtime_session):
    errors.append('Runtime Session does not consume sealed Conditional execution without replanning')
if 'DynamicInvocationPlanner' in runtime_session or 'DynamicInvocationVerifier' in runtime_session:
    errors.append('Verified Dynamic Runtime illegally invokes Planner/Verifier')

if ('IndirectExecutionModeV1' not in dynamic_contract_header or
        'VerifiedIndirectDispatchContractV1' not in dynamic_contract_header or
        'indirectDispatch' not in dynamic_contract_header):
    errors.append('Composition does not freeze the Verified Indirect Dispatch route')
if ('ValidateIndirectDispatch' not in runtime_session or
        'hasIndirectDispatch' not in (root/'src/runtime/session/RuntimeSession.h').read_text(encoding='utf-8')):
    errors.append('Runtime Session does not consume sealed Verified Indirect Dispatch arguments')
executor_instance=(root/'src/backends/d3d12/executor/detail/ExecutorInstance.inl').read_text(encoding='utf-8')
if ('CreateCommandSignature' not in executor_instance or
        'D3D12_INDIRECT_ARGUMENT_TYPE_DISPATCH' not in executor_instance or
        'ExecuteIndirect' not in executor_instance):
    errors.append('D3D12 Executor does not mechanically execute sealed DispatchIndirect arguments')
composition_contract=(root/'src/composition/model/CompositionContract.h').read_text(encoding='utf-8')
composition_plan=(root/'src/composition/model/plan/CompositionPlan.h').read_text(encoding='utf-8')
if 'Texture2DFlowShape' not in composition_contract or 'Texture2DFlowShape texture2D' not in composition_contract:
    errors.append('Composition Contract does not freeze limited Texture2D shape')
if 'Texture2DFlowShape texture2D' not in composition_plan:
    errors.append('Composition Plan does not freeze limited Texture2D allocation shape')
if 'FrozenCompositionAbi2ContractSchema = 2' not in abi2_header or 'FrozenCompositionAbi2DecisionSchema = 2' not in abi2_header:
    errors.append('SGE4UNI 2.5 does not use Contract/Decision schema 2')
executor_header=(root/'src/backends/d3d12/executor/Executor.h').read_text(encoding='utf-8')
shared_resources=(root/'src/backends/d3d12/runtime/resources/CompositionSharedResources.cpp').read_text(encoding='utf-8')
if 'CreateSharedTexture2D' not in executor_header or 'ReadSharedTexture2D' not in executor_header:
    errors.append('D3D12 Executor does not expose limited shared Texture2D materialization/readback')
if 'CreateSharedTexture2D' not in shared_resources or 'TransitionSharedResource' not in shared_resources:
    errors.append('Composition Runtime does not mechanically materialize and transition Texture2D Flow')
migration_source=(root/'src/composition/migration/abi1/FrozenCompositionAbi1Migration.cpp').read_text(encoding='utf-8')
if 'ABI 1移行CorpusはBuffer Flowだけ' not in migration_source:
    errors.append('ABI 1 migration does not explicitly reject Texture2D Flow inference')
semantic_model=(root/'src/leaf/model/semantic/SemanticModel.h').read_text(encoding='utf-8')
semantic_analysis=(root/'src/leaf/model/analysis/SemanticAnalysis.cpp').read_text(encoding='utf-8')
if ('Rgba32Float = 3' not in semantic_model or
        'StorageTexture2D = 11' not in semantic_model or
        'UnorderedTexture2D = 5' not in semantic_model):
    errors.append('Semantic Model does not own the limited RGBA32F Texture2D UAV vocabulary')
if ('StorageTexture2D' not in semantic_analysis or 'Rgba32Float' not in semantic_analysis):
    errors.append('Semantic Analysis does not validate the limited Texture2D UAV contract')
executor_api=(root/'src/backends/d3d12/executor/detail/ExecutorApi.inl').read_text(encoding='utf-8')
if ('D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS' not in executor_api or
        'R32G32B32A32Float' not in executor_api):
    errors.append('D3D12 Executor does not materialize the limited RGBA32F UAV Texture')
if ('D3D12_UAV_DIMENSION_TEXTURE2D' not in executor_instance or
        'CreateUnorderedAccessView(native->Native()' not in executor_instance):
    errors.append('D3D12 Executor does not create an external Texture2D UAV descriptor')
runtime_fixture=(root/'tests/fixtures/RuntimeFixture.h').read_text(encoding='utf-8')
windows_qualification=(root/'tests/61_UnifiedWindowsQualification/main.cpp').read_text(encoding='utf-8')
if ('BuildTextureUavProducerLeaf' not in runtime_fixture or
        'RWTexture2D<float4>' not in runtime_fixture or
        'BuildTextureFloatConsumerLeaf' not in runtime_fixture):
    errors.append('Qualification fixture does not compile the limited Compute UAV to SRV Texture path')
if ('VerifyLimitedTexture2DUavFlowQualification' not in windows_qualification or
        'EqualsFloatTexture' not in windows_qualification):
    errors.append('Windows qualification does not observe the RGBA32F UAV intermediate and BGRA8 output')
if not (root/'docs/LEVEL4_GENERALIZATION5_LIMITED_TEXTURE2D_UAV_COMPUTE_FLOW.md').exists():
    errors.append('Missing Generalization 5 design contract')
composition_runtime=(root/'src/backends/d3d12/runtime/composition/CompositionRuntime.cpp').read_text(encoding='utf-8')
if 'invocation.enabledLeaves' not in composition_runtime or 'if (!enabled[entry.leaf.value]) continue;' not in composition_runtime:
    errors.append('D3D12 Composition Runtime does not mechanically skip unselected Conditional leaves')
corruption_test=(root/'tests/60_UnifiedArchitectureTests/Abi2CorruptionTests.cpp')
portable_test=(root/'tests/60_UnifiedArchitectureTests/Abi2PortableSelfTest.cpp')
if not corruption_test.exists(): errors.append('ABI 2.5 corruption corpus is missing')
if not portable_test.exists(): errors.append('ABI 2.5 portable round-trip/migration self-test is missing')

if errors:
    print('New SGE4静的監査に失敗しました')
    for error in errors: print(' -',error)
    sys.exit(1)
print(f'New SGE4静的監査に合格しました。Project数={len(projects)}, 製品Project数={len(product_projects)}, Active Source数={len(active_cpp)}, 不変条件数=40')
