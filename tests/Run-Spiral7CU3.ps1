$ErrorActionPreference = 'Stop'
$testsRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$root = Split-Path -Parent $testsRoot
. (Join-Path $testsRoot 'Spiral7Common.ps1')

Invoke-Spiral7Checks @('tools\Verify-Spiral7CU2.ps1','tools\Verify-Spiral7CU3.ps1')
Build-Spiral7Project '183_Spiral7AuthorityMutationTests\183_Spiral7AuthorityMutationTests.vcxproj'

$output = Join-Path $root 'build\tests\spiral7-cu3'
Remove-Item -Recurse -Force -ErrorAction SilentlyContinue $output
New-Item -ItemType Directory -Force $output | Out-Null
$debugExe = Join-Path $root 'build\bin\x64\Debug\183_Spiral7AuthorityMutationTests.exe'
$releaseExe = Join-Path $root 'build\bin\x64\Release\183_Spiral7AuthorityMutationTests.exe'
$debugBundle = Join-Path $output 'debug.bin'
$releaseBundle = Join-Path $output 'release.bin'

Invoke-Checked $debugExe @('--emit',$debugBundle)
Invoke-Checked $releaseExe @('--emit',$releaseBundle)
if (-not (Test-BytesEqual $debugBundle $releaseBundle)) {
    throw 'Spiral 7 CU3 authority evidence differs across Debug/Release.'
}

Write-Host '============================================================'
Write-Host 'SGE4-5 SPIRAL 7 COMPLETION UNIT 3 PASSED'
Write-Host 'Authority: Raw Candidate -> Planner-independent Verifier -> opaque Verified Plan -> Frozen resource/epoch binding'
Write-Host 'Bound meaning: A_(t-1), A_t, M_t, T_t, transition records, Update/Clear/Hold authority and prior extension identities'
Write-Host 'Mutation gates: 50 Raw attacks plus Verified context/invocation and resource/artifact/epoch replay rejection'
Write-Host 'Resources: Compact Delta ReadOnly, previous History ReadOnly, output History WriteOnly; identities must be distinct'
Write-Host 'Verified execution: one WARP Compact Delta transition at Device epoch 7'
Write-Host 'Runtime transition-policy decision: None'
Write-Host "CU3 authority evidence SHA-256: $(Get-SGE4FileSha256 $debugBundle)"
Write-Host '============================================================'
