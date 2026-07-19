$ErrorActionPreference = 'Stop'

$utf8NoBom = New-Object System.Text.UTF8Encoding($false)
[Console]::InputEncoding = $utf8NoBom
[Console]::OutputEncoding = $utf8NoBom
$OutputEncoding = $utf8NoBom

$root = Split-Path -Parent $MyInvocation.MyCommand.Path
$projectFiles = @(
    Get-ChildItem -Path $root -Recurse -File -Filter *.vcxproj |
        Where-Object {
            $_.FullName -notlike (Join-Path $root 'build\*') -and
            $_.FullName -notlike (Join-Path $root '.vs\*') -and
            $_.FullName -notlike (Join-Path $root '.git\*')
        } |
        Sort-Object FullName
)
if ($projectFiles.Count -eq 0) { throw 'No Visual Studio C++ projects were found.' }

$graph = @{}
foreach ($project in $projectFiles) {
    $projectPath = [IO.Path]::GetFullPath($project.FullName)
    [xml]$xml = Get-Content -Raw -LiteralPath $projectPath -Encoding UTF8
    $references = @()
    foreach ($node in $xml.SelectNodes("//*[local-name()='ProjectReference']")) {
        $referencePath = [IO.Path]::GetFullPath((Join-Path $project.DirectoryName $node.Include))
        if (-not (Test-Path -LiteralPath $referencePath)) {
            throw "Missing ProjectReference: $projectPath -> $referencePath"
        }
        $references += $referencePath
    }
    $graph[$projectPath] = $references
}

$visitState = @{}
$stack = New-Object System.Collections.Generic.List[string]
function Visit-Project([string]$projectPath) {
    $state = if ($visitState.ContainsKey($projectPath)) { $visitState[$projectPath] } else { 0 }
    if ($state -eq 1) {
        $cycle = (@($stack) + @($projectPath) | ForEach-Object { Split-Path (Split-Path $_ -Parent) -Leaf }) -join ' -> '
        throw "ProjectReference cycle detected: $cycle"
    }
    if ($state -eq 2) { return }
    $visitState[$projectPath] = 1
    [void]$stack.Add($projectPath)
    foreach ($dependency in $graph[$projectPath]) { Visit-Project $dependency }
    $stack.RemoveAt($stack.Count - 1)
    $visitState[$projectPath] = 2
}
foreach ($projectPath in $graph.Keys) { Visit-Project $projectPath }

function Get-Project([string]$name) {
    $p = $projectFiles | Where-Object { $_.Directory.Name -eq $name } | Select-Object -First 1
    if (-not $p) { throw "Project was not found: $name" }
    return $p
}
function Get-TransitiveDependencies([string]$projectPath) {
    $seen = @{}
    $pending = New-Object System.Collections.Generic.Stack[string]
    $pending.Push([IO.Path]::GetFullPath($projectPath))
    while ($pending.Count -gt 0) {
        $current = $pending.Pop()
        foreach ($dependency in $graph[$current]) {
            if (-not $seen.ContainsKey($dependency)) {
                $seen[$dependency] = $true
                $pending.Push($dependency)
            }
        }
    }
    return $seen
}
function Assert-NoForbiddenDependency([string]$targetName, [string[]]$forbiddenNames) {
    $targetProject = Get-Project $targetName
    $dependencies = Get-TransitiveDependencies $targetProject.FullName
    foreach ($dependency in $dependencies.Keys) {
        $dependencyName = Split-Path (Split-Path $dependency -Parent) -Leaf
        if ($forbiddenNames -contains $dependencyName) {
            throw "$targetName transitively references forbidden project $dependencyName"
        }
    }
}
function Assert-DirectReferences([string]$targetName, [string[]]$allowedNames) {
    $targetProject = Get-Project $targetName
    $targetPath = [IO.Path]::GetFullPath($targetProject.FullName)
    foreach ($dependency in $graph[$targetPath]) {
        $dependencyName = Split-Path (Split-Path $dependency -Parent) -Leaf
        if ($allowedNames -notcontains $dependencyName) {
            throw "$targetName directly references non-authorized project $dependencyName"
        }
    }
}

$sourceSide = @(
    '02_SemanticModel','03_SemanticBuilder','04_SemanticAnalysis','05_TargetContract','05A_CompilationInput',
    '06_ExecutionPlanModel','07_ExecutionPlanVerifier','08_CandidatePlanner',
    '11_D3D12PackageLowering','12_SGE4_5Compiler',
    '20_ExperimentDomain','21_ClassicalFrontend','22_SdfFrontend','23_PgaFrontend',
    '24_SliceScenarios','25_GeneralGraphScenarios','26_GeneratedGraphCorpus','27_RuntimeScenarios',
    '28_SGE3CompatibilityOracle',
    '60_PgaRigidTransformSemantic','61_Spiral1Contracts','62_Spiral1Corpus','67_Spiral1Observer'
)
$runtimeSide = @('13_PackageRuntime','14_D3D12Backend','15_PlatformWin32')

Assert-NoForbiddenDependency '09_FrozenPackageCore' ($sourceSide + $runtimeSide)
Assert-NoForbiddenDependency '10_D3D12PackageSchema' ($sourceSide + $runtimeSide)
Assert-NoForbiddenDependency '05A_CompilationInput' @('06_ExecutionPlanModel','07_ExecutionPlanVerifier','08_CandidatePlanner','09_FrozenPackageCore','10_D3D12PackageSchema','11_D3D12PackageLowering','12_SGE4_5Compiler','13_PackageRuntime','14_D3D12Backend','15_PlatformWin32')
Assert-NoForbiddenDependency '06_ExecutionPlanModel' @('07_ExecutionPlanVerifier','08_CandidatePlanner','11_D3D12PackageLowering','12_SGE4_5Compiler','13_PackageRuntime','14_D3D12Backend','15_PlatformWin32')
Assert-NoForbiddenDependency '07_ExecutionPlanVerifier' @('08_CandidatePlanner','11_D3D12PackageLowering','12_SGE4_5Compiler','13_PackageRuntime','14_D3D12Backend','15_PlatformWin32')
Assert-NoForbiddenDependency '08_CandidatePlanner' (@('09_FrozenPackageCore','10_D3D12PackageSchema','11_D3D12PackageLowering','12_SGE4_5Compiler') + $runtimeSide)
Assert-NoForbiddenDependency '11_D3D12PackageLowering' $runtimeSide
Assert-NoForbiddenDependency '12_SGE4_5Compiler' $runtimeSide
Assert-NoForbiddenDependency '13_PackageRuntime' $sourceSide
Assert-NoForbiddenDependency '14_D3D12Backend' $sourceSide
Assert-NoForbiddenDependency '15_PlatformWin32' $sourceSide
Assert-NoForbiddenDependency '36_D3D12ReadbackTests' $sourceSide
Assert-NoForbiddenDependency '50_Launcher' $sourceSide

Assert-DirectReferences '14_D3D12Backend' @('00_Foundation','09_FrozenPackageCore','10_D3D12PackageSchema','13_PackageRuntime')
Assert-DirectReferences '13_PackageRuntime' @('00_Foundation','09_FrozenPackageCore')
Assert-DirectReferences '05A_CompilationInput' @('00_Foundation','02_SemanticModel','04_SemanticAnalysis','05_TargetContract')
Assert-DirectReferences '08_CandidatePlanner' @('00_Foundation','02_SemanticModel','04_SemanticAnalysis','05_TargetContract','05A_CompilationInput','06_ExecutionPlanModel','07_ExecutionPlanVerifier')
Assert-DirectReferences '12_SGE4_5Compiler' @('00_Foundation','02_SemanticModel','05_TargetContract','07_ExecutionPlanVerifier','08_CandidatePlanner','11_D3D12PackageLowering')

$frontendIsolation = @{
    '21_ClassicalFrontend' = @('22_SdfFrontend','23_PgaFrontend')
    '22_SdfFrontend'       = @('21_ClassicalFrontend','23_PgaFrontend')
    '23_PgaFrontend'       = @('21_ClassicalFrontend','22_SdfFrontend')
    '20_ExperimentDomain'  = @('21_ClassicalFrontend','22_SdfFrontend','23_PgaFrontend')
}
foreach ($frontendName in $frontendIsolation.Keys) {
    $frontendProject = Get-Project $frontendName
    $dependencies = Get-TransitiveDependencies $frontendProject.FullName
    foreach ($dependency in $dependencies.Keys) {
        $dependencyName = Split-Path (Split-Path $dependency -Parent) -Leaf
        if ($frontendIsolation[$frontendName] -contains $dependencyName) {
            throw "$frontendName transitively references forbidden sibling frontend $dependencyName"
        }
    }
}

# The historical canonical path is a qualification oracle only.
$oracle = Get-Project '28_SGE3CompatibilityOracle'
$oraclePath = [IO.Path]::GetFullPath($oracle.FullName)
foreach ($projectPath in $graph.Keys) {
    if ($graph[$projectPath] -contains $oraclePath) {
        $consumer = Split-Path (Split-Path $projectPath -Parent) -Leaf
        if ($consumer -ne '33_PlanningTests') {
            throw "$consumer references the SGE3 compatibility oracle; only 33_PlanningTests may do so"
        }
    }
}


# The reference-canonical internal header is test-only.  It may be included only
# by the compatibility Oracle implementation; production code cannot bypass the
# sealed-plan public API by including an internal declaration.
$internalHeaderName = 'D3D12PackageLoweringInternal.h'
$allowedInternalConsumer = [IO.Path]::GetFullPath((Join-Path $root '28_SGE3CompatibilityOracle\SGE3CompatibilityOracle.cpp'))
$sourceFiles = Get-ChildItem -Path $root -Recurse -File | Where-Object { $_.Extension -in '.h','.cpp' }
foreach ($source in $sourceFiles) {
    $text = Get-Content -Raw -LiteralPath $source.FullName
    if ($text -match [regex]::Escape($internalHeaderName)) {
        if ([IO.Path]::GetFullPath($source.FullName) -ne $allowedInternalConsumer) {
            throw "Reference-canonical internal header leaked outside the compatibility Oracle: $($source.FullName)"
        }
    }
}

# Candidate planning must remain Package-free. This is the Stage-0C boundary:
# only SGE4_5Compiler may combine candidate planning with Package lowering.
$candidatePlannerFiles = Get-ChildItem -Path (Join-Path $root '08_CandidatePlanner') -Recurse -File |
    Where-Object { $_.Extension -in '.h','.cpp' }
foreach ($source in $candidatePlannerFiles) {
    $text = Get-Content -Raw -LiteralPath $source.FullName
    if ($text -match 'D3D12PackageLowering|LowerVerifiedPlan|CompileOutput') {
        throw "CandidatePlanner leaked Package-lowering responsibility: $($source.FullName)"
    }
}

# Among production projects, only SGE4_5Compiler may call the public sealed
# Package-lowering entry. The lowerer defines it and the SGE3 Oracle is test-only.
$allowedLoweringUsers = @(
    [IO.Path]::GetFullPath((Join-Path $root '11_D3D12PackageLowering\D3D12PackageLowering.cpp')),
    [IO.Path]::GetFullPath((Join-Path $root '12_SGE4_5Compiler\SGE4_5Compiler.cpp')),
    [IO.Path]::GetFullPath((Join-Path $root '28_SGE3CompatibilityOracle\SGE3CompatibilityOracle.cpp'))
)
foreach ($source in $sourceFiles) {
    $full = [IO.Path]::GetFullPath($source.FullName)
    $projectName = $source.Directory.Name
    if ($source.Extension -eq '.cpp' -and
        $projectName -match '^(0|1|2)[0-9A-Z_]' -and
        (Get-Content -Raw -LiteralPath $source.FullName) -match 'LowerVerifiedPlan' -and
        $allowedLoweringUsers -notcontains $full) {
        throw "Production Package-lowering orchestration leaked outside SGE4_5Compiler: $($source.FullName)"
    }
}

# Production project names must not retain Level-2/Level-3 generation numbering.
foreach ($project in $projectFiles) {
    $name = $project.Directory.Name
    if ($name -notmatch '^28_' -and ($name -match 'Level2|Level3|SGE2|SGE3')) {
        throw "Historical generation name leaked into SGE4 project topology: $name"
    }
}



# Spiral 1 Completion Unit 1 boundaries.
Assert-DirectReferences '60_PgaRigidTransformSemantic' @('00_Foundation')
Assert-DirectReferences '61_Spiral1Contracts' @('00_Foundation','60_PgaRigidTransformSemantic')
Assert-DirectReferences '62_Spiral1Corpus' @('00_Foundation','60_PgaRigidTransformSemantic','61_Spiral1Contracts')
Assert-DirectReferences '67_Spiral1Observer' @('00_Foundation','60_PgaRigidTransformSemantic','61_Spiral1Contracts','62_Spiral1Corpus')
Assert-DirectReferences '70_Spiral1SemanticTests' @('00_Foundation','60_PgaRigidTransformSemantic','61_Spiral1Contracts','62_Spiral1Corpus','67_Spiral1Observer')
Assert-NoForbiddenDependency '60_PgaRigidTransformSemantic' @(
    '01_MathVocabulary','02_SemanticModel','03_SemanticBuilder','04_SemanticAnalysis','05_TargetContract','05A_CompilationInput',
    '06_ExecutionPlanModel','07_ExecutionPlanVerifier','08_CandidatePlanner','09_FrozenPackageCore','10_D3D12PackageSchema',
    '11_D3D12PackageLowering','12_SGE4_5Compiler','13_PackageRuntime','14_D3D12Backend','15_PlatformWin32',
    '16_FrozenCompositionArtifact','17_CompositionContract','18_CompositionPlan','19_CompositionVerifier',
    '20_CompositionDeviceDomain','21_CompositionSharedResources','22_CompositionRuntime','23_CompositionRecovery',
    '61_Spiral1Contracts','62_Spiral1Corpus','67_Spiral1Observer')
Assert-NoForbiddenDependency '61_Spiral1Contracts' @('62_Spiral1Corpus','67_Spiral1Observer','13_PackageRuntime','14_D3D12Backend')
Assert-NoForbiddenDependency '62_Spiral1Corpus' @('67_Spiral1Observer','13_PackageRuntime','14_D3D12Backend')
Assert-NoForbiddenDependency '67_Spiral1Observer' @('13_PackageRuntime','14_D3D12Backend')

# Spiral 2 Completion Unit 1 boundaries. Dynamic values terminate at the
# invocation contract; the canonical hierarchy is static and runtime-agnostic.
Assert-DirectReferences '80_HierarchySemantic' @('00_Foundation')
Assert-DirectReferences '81_Spiral2Contracts' @('00_Foundation','80_HierarchySemantic')
Assert-DirectReferences '82_Spiral2Corpus' @('00_Foundation','80_HierarchySemantic','81_Spiral2Contracts')
Assert-DirectReferences '90_Spiral2SemanticTests' @('00_Foundation','80_HierarchySemantic','81_Spiral2Contracts','82_Spiral2Corpus')
Assert-DirectReferences '91_Spiral2DynamicInvocationTests' @('00_Foundation','80_HierarchySemantic','81_Spiral2Contracts','82_Spiral2Corpus')
Assert-NoForbiddenDependency '80_HierarchySemantic' @('13_PackageRuntime','14_D3D12Backend','16_FrozenCompositionArtifact','22_CompositionRuntime')
Assert-NoForbiddenDependency '81_Spiral2Contracts' @('13_PackageRuntime','14_D3D12Backend','16_FrozenCompositionArtifact','22_CompositionRuntime')
Assert-NoForbiddenDependency '82_Spiral2Corpus' @('13_PackageRuntime','14_D3D12Backend','16_FrozenCompositionArtifact','22_CompositionRuntime')
Assert-NoForbiddenDependency '13_PackageRuntime' @('80_HierarchySemantic','81_Spiral2Contracts','82_Spiral2Corpus')
Assert-NoForbiddenDependency '14_D3D12Backend' @('80_HierarchySemantic','81_Spiral2Contracts','82_Spiral2Corpus')

Write-Host "SGE4-5 dependency boundary check passed. Projects: $($projectFiles.Count), references: $((($graph.Values | ForEach-Object { $_.Count }) | Measure-Object -Sum).Sum)."
