$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
function Require([bool]$Condition,[string]$Message){if(-not $Condition){throw $Message}}

$required = @(
    'docs\spiral7\CLOSURE_MANIFEST_V1.json',
    'docs\spiral7\SPIRAL7_FINAL_DECISION_SUMMARY.md',
    'docs\level4v2\README.md',
    'docs\level4v2\CANONICAL_RECONSTRUCTION_INPUT_V1.md',
    'docs\level4v2\CANONICAL_RECONSTRUCTION_INPUT_V1.json',
    'docs\level4v2\PROVEN_CAPABILITY_MATRIX_V1.md',
    'docs\level4v2\RETAINED_REFERENCE_LAYOUT_V1.md',
    'docs\level4v2\RECONSTRUCTION_ROADMAP_V1.md',
    'docs\level4v2\EXTERNAL_EVIDENCE_INDEX_V1.md',
    'docs\level4v2\PRE_V2_REPOSITORY_DECISION_V1.md'
)
foreach($relative in $required){Require (Test-Path -LiteralPath (Join-Path $root $relative) -PathType Leaf) "Level 4 v2 handoff file missing: $relative"}

$closure = Get-Content -Raw -LiteralPath (Join-Path $root 'docs\spiral7\CLOSURE_MANIFEST_V1.json') -Encoding UTF8 | ConvertFrom-Json
Require ($closure.schema -eq 'SGE4-5.Spiral7.ClosureManifest.V1') 'Spiral 7 closure schema mismatch.'
Require ($closure.status -eq 'Closed') 'Spiral 7 is not closed.'
Require ($closure.acceptedFinalCommit -eq 'f802c12b162569a869c214da22b80142b3a4a0dd') 'Spiral 7 accepted final commit mismatch.'
Require ($closure.architectureStatus -eq 'Complete' -and $closure.experimentStatus -eq 'Complete') 'Spiral 7 completion status mismatch.'
Require ($closure.cu6Evidence.canonicalEvidenceSha256 -eq '0d7a50c448269cbb346ea80b0ceda09e7f28b10c068ed20a96d6c47621b4802a') 'Canonical real-GPU evidence identity mismatch.'
Require ($closure.cu6Evidence.refinementEvidenceSha256 -eq '44fa4dd72e64aab300bec53fc45c6ae2bf7a7012630b0e15a83fe6717c7ff80b') 'Refinement real-GPU evidence identity mismatch.'
Require ($closure.cu6Evidence.externalEvidenceZipSha256 -eq '9ee852c2a2574a8cb9c163b6e94213b855317bba74319e6443e9aa98c10c24d9') 'External evidence bundle identity mismatch.'
Require ([int]$closure.cu6Evidence.combinedCoordinateCount -eq 220) 'Combined Decision coordinate count mismatch.'
Require ([int]$closure.cu6Evidence.decisionClasses.stableWinner -eq 41) 'Stable winner count mismatch.'
Require (($closure.cu6Evidence.stableSingleWinnerSet -join ',') -eq 'CompactDeltaIndexHistoryReuse') 'Stable winner set mismatch.'
Require ($closure.runtimeCandidatePolicyAuthorization -eq 'None') 'Runtime Candidate policy must remain unauthorized.'
Require ($closure.universalWinnerClaim -eq 'Forbidden') 'Universal winner claim must remain forbidden.'
Require ($closure.nextProgram -eq 'Level4V2CanonicalReconstruction') 'Next program mismatch.'

$input = Get-Content -Raw -LiteralPath (Join-Path $root 'docs\level4v2\CANONICAL_RECONSTRUCTION_INPUT_V1.json') -Encoding UTF8 | ConvertFrom-Json
Require ($input.schema -eq 'SGE4.Level4V2.CanonicalReconstructionInput.V1') 'Level 4 v2 input schema mismatch.'
Require ($input.mode -eq 'ReconstructionNotNewCapability') 'Level 4 v2 mode mismatch.'
Require ($input.runtimeCandidatePolicyAuthorization -eq 'None') 'Level 4 v2 input authorized Runtime policy.'
Require (($input.forbid -contains 'HardwareSpecificWinnerInCanonicalAbi')) 'Level 4 v2 hardware-specific policy boundary missing.'
Require (($input.forbid -contains 'GamePolicyInference')) 'Level 4 v2 game-policy boundary missing.'

$progress = Get-Content -Raw -LiteralPath (Join-Path $root 'docs\spiral7\SPIRAL7_PROGRESS.md') -Encoding UTF8
foreach($token in @('SGE4-5 SPIRAL 7 CLOSED','f802c12b162569a869c214da22b80142b3a4a0dd','0d7a50c448269cbb346ea80b0ceda09e7f28b10c068ed20a96d6c47621b4802a','44fa4dd72e64aab300bec53fc45c6ae2bf7a7012630b0e15a83fe6717c7ff80b')){Require ($progress -match [regex]::Escape($token)) "Spiral 7 progress handoff token missing: $token"}

Write-Host 'Level 4 v2 Canonical reconstruction handoff passed.'
Write-Host 'Spiral 7 is closed; reference implementation and external evidence identities are preserved.'
Write-Host 'Runtime Candidate-policy authorization remains None.'
