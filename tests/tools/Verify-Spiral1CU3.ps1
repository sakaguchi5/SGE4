$ErrorActionPreference = 'Stop'
$testsRoot = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)
$root = Split-Path -Parent $testsRoot

$required = @(
    '66_Spiral1LeafCompiler/66_Spiral1LeafCompiler.vcxproj',
    '66_Spiral1LeafCompiler/Spiral1LeafCompiler.h',
    '66_Spiral1LeafCompiler/Spiral1LeafCompiler.cpp',
    '73_Spiral1LeafPackageTests/73_Spiral1LeafPackageTests.vcxproj',
    '73_Spiral1LeafPackageTests/main.cpp',
    'docs/spiral1/CU3_EVIDENCE_LEDGER.md'
)
foreach ($relative in $required) {
    if (-not (Test-Path -LiteralPath (Join-Path $root ($relative.Replace('/','\'))))) {
        throw "CU3 required file is missing: $relative"
    }
}

$solution = Get-Content -Raw -LiteralPath (Join-Path $root 'SemanticGpuEngine4-5.sln')
foreach ($project in @('66_Spiral1LeafCompiler','73_Spiral1LeafPackageTests')) {
    if ($solution -notmatch [regex]::Escape("$project\$project.vcxproj")) {
        throw "CU3 project is not registered in the solution: $project"
    }
}
$leafProject = Get-Content -Raw -LiteralPath (Join-Path $root '66_Spiral1LeafCompiler\66_Spiral1LeafCompiler.vcxproj')
if ($leafProject -match '64_D3D12RepresentationPlanner') {
    throw 'Leaf Compiler must consume only Verified Representation Plans and cannot reference the Raw Candidate Planner.'
}
foreach ($requiredReference in @('65_D3D12RepresentationVerifier','12_SGE4_5Compiler','09_FrozenPackageCore','10_D3D12PackageSchema')) {
    if ($leafProject -notmatch [regex]::Escape($requiredReference)) {
        throw "Leaf Compiler is missing required authority reference: $requiredReference"
    }
}
foreach ($backendProject in @('13_PackageRuntime','14_D3D12Backend')) {
    $text = Get-Content -Raw -LiteralPath (Join-Path $root "$backendProject\$backendProject.vcxproj")
    if ($text -match '60_PgaRigidTransformSemantic|63_D3D12RepresentationCandidate|64_D3D12RepresentationPlanner|65_D3D12RepresentationVerifier|66_Spiral1LeafCompiler') {
        throw "$backendProject illegally references Spiral 1 Semantic or planning projects."
    }
}
$source = Get-Content -Raw -LiteralPath (Join-Path $root '66_Spiral1LeafCompiler\Spiral1LeafCompiler.cpp')
foreach ($needle in @('D3DCOMPILE_IEEE_STRICTNESS','ValidatePlanContext','CompileCanonical','BuildFrozenPackage','ProgramBinaryAuthorityMismatch','lockedConstantPayload','DynamicSlots().size() != 1')) {
    if ($source -notmatch [regex]::Escape($needle)) { throw "CU3 authority implementation is missing: $needle" }
}
$tests = Get-Content -Raw -LiteralPath (Join-Path $root '73_Spiral1LeafPackageTests\main.cpp')
foreach ($needle in @('MatrixExpandedComputeV1','DirectPgaMotorComputeV1','packageExecutionDigest','ObserveCaseV1','CreateExternalBuffer','ReadExternalBuffer','DynamicDataBinding','tampered locked constant payload')) {
    if ($tests -notmatch [regex]::Escape($needle)) { throw "CU3 qualification test is missing: $needle" }
}
Write-Host 'SGE4-5 Spiral 1 CU3 structure and authority verification passed.'
