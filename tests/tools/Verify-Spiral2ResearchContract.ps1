param([switch]$EnforceCU0ProjectAbsence)

$ErrorActionPreference = 'Stop'
$utf8NoBom = New-Object System.Text.UTF8Encoding($false)
[Console]::InputEncoding = $utf8NoBom
[Console]::OutputEncoding = $utf8NoBom
$OutputEncoding = $utf8NoBom

$toolsRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$testsRoot = Split-Path -Parent $toolsRoot
$root = Split-Path -Parent $testsRoot
$spiralRoot = Join-Path $root 'docs\spiral2'
. (Join-Path $toolsRoot 'Sha256.ps1')

$manifestPath = Join-Path $spiralRoot 'CONTRACT_MANIFEST_V1.json'
if (-not (Test-Path -LiteralPath $manifestPath)) { throw 'Spiral 2 contract manifest is missing.' }
$manifest = Get-Content -Raw -LiteralPath $manifestPath -Encoding UTF8 | ConvertFrom-Json
if ($manifest.schema -ne 'SGE4-5.Spiral2.ContractManifest.V1' -or $manifest.status -ne 'frozen') { throw 'Unexpected Spiral 2 contract manifest.' }
if ($manifest.completionUnit -ne 'CU0') { throw 'Contract manifest is not CU0.' }
if ($manifest.baselineCommit -ne 'afaed65d9400f3f80a7f5a9094edd9029082f95d') { throw 'Baseline commit mismatch.' }
if ($manifest.ownerSelection -ne 'FixedDynamicMotorPaletteAndHierarchy') { throw 'Owner selection mismatch.' }
if ($manifest.ownerOnlyGate -ne 'S2-22B') { throw 'Owner-only gate mismatch.' }
if ([int]$manifest.targetAbi.d3d12Schema -ne 17 -or [int]$manifest.targetAbi.runtime -ne 17) { throw 'Target ABI mismatch.' }

$expected = @(
    'docs/spiral2/NEXT_CAPABILITY_SELECTION_FROM_SPIRAL1.md',
    'docs/spiral2/SGE4-5_Spiral2_Completion_Spec_v0.1.md',
    'docs/spiral2/NON_GOALS_V1.md',
    'docs/spiral2/OBSERVATION_CONTRACT_V1.md',
    'docs/spiral2/CORPUS_V1.md',
    'docs/spiral2/PROJECT_BOUNDARIES_V1.md',
    'docs/spiral2/STAGE_MAP_V1.md',
    'docs/spiral2/PROOF_LEDGER_V1.md',
    'docs/spiral2/FOUNDATION_BASELINE_V1.md'
)
$paths = @($manifest.contracts | ForEach-Object { [string]$_.path })
if ($paths.Count -ne $expected.Count) { throw 'Unexpected Spiral 2 contract count.' }
foreach ($relative in $expected) { if ($paths -notcontains $relative) { throw "Missing contract manifest entry: $relative" } }
foreach ($entry in @($manifest.contracts)) {
    $relative = [string]$entry.path
    if ($relative -match '[^\x00-\x7F]') { throw "Non-ASCII contract path: $relative" }
    $path = Join-Path $root ($relative.Replace('/', '\'))
    if (-not (Test-Path -LiteralPath $path)) { throw "Missing contract: $relative" }
    if ((Get-Item -LiteralPath $path).Length -ne [long]$entry.bytes) { throw "Contract byte count mismatch: $relative" }
    if ((Get-SGE4FileSha256 $path).ToLowerInvariant() -ne [string]$entry.sha256) { throw "Contract digest mismatch: $relative" }
    $text = Get-Content -Raw -LiteralPath $path -Encoding UTF8
    if ($text -match '(?im)\b(TODO|TBD|PLACEHOLDER)\b') { throw "Unfrozen placeholder: $relative" }
}

$proof = Get-Content -Raw -LiteralPath (Join-Path $spiralRoot 'PROOF_LEDGER_V1.md') -Encoding UTF8
for ($index = 1; $index -le 24; ++$index) {
    $id = 'S2-I{0:D2}' -f $index
    if (([regex]::Matches($proof, [regex]::Escape($id))).Count -ne 1) { throw "Proof ledger must contain $id exactly once." }
}
$corpus = Get-Content -Raw -LiteralPath (Join-Path $spiralRoot 'CORPUS_V1.md') -Encoding UTF8
foreach ($index in 0..8) { $id = 'H{0:D2}' -f $index; if ($corpus -notmatch "(?m)^- ${id}:") { throw "Missing topology $id." } }
foreach ($index in 0..11) { $id = 'F{0:D2}' -f $index; if ($corpus -notmatch "(?m)^- ${id} ") { throw "Missing frame $id." } }

$selection = Get-Content -Raw -LiteralPath (Join-Path $spiralRoot 'NEXT_CAPABILITY_SELECTION_FROM_SPIRAL1.md') -Encoding UTF8
if ($selection -notmatch 'new post-baseline Owner decision' -or $selection -notmatch 'does not rewrite') { throw 'Historical Spiral 1 decision preservation is not explicit.' }
$progress = Get-Content -Raw -LiteralPath (Join-Path $spiralRoot 'SPIRAL2_PROGRESS.md') -Encoding UTF8
if ($progress -notmatch 'NextCapabilitySelection = DeferredByOwner') { throw 'Deferred owner selection marker is missing.' }

if ($EnforceCU0ProjectAbsence) {
    $future = @(Get-ChildItem -LiteralPath $root -Directory | Where-Object { $_.Name -match '^(8[0-9]|9[0-9])_' })
    if ($future.Count -ne 0) { throw 'CU0 must not contain Spiral 2 C++ projects.' }
}

Write-Host "SGE4-5 Spiral 2 research contract passed. Contracts: $($expected.Count), invariants: 24, topologies: 9, frames: 12."

