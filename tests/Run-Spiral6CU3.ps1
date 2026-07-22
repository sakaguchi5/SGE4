$ErrorActionPreference = 'Stop'
$testsRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$root = Split-Path -Parent $testsRoot
. (Join-Path $testsRoot 'Spiral6Common.ps1')

Invoke-Spiral6Checks @('tools\Verify-Spiral6CU2.ps1','tools\Verify-Spiral6CU3.ps1')
Build-Spiral6Project '167_Spiral6AuthorityMutationTests\167_Spiral6AuthorityMutationTests.vcxproj'

$output = Join-Path $root 'build\tests\spiral6-cu3'
Remove-Item -Recurse -Force -ErrorAction SilentlyContinue $output
New-Item -ItemType Directory -Force $output | Out-Null
$debugExe = Join-Path $root 'build\bin\x64\Debug\167_Spiral6AuthorityMutationTests.exe'
$releaseExe = Join-Path $root 'build\bin\x64\Release\167_Spiral6AuthorityMutationTests.exe'
$debugBundle = Join-Path $output 'debug.bin'
$releaseBundle = Join-Path $output 'release.bin'

Invoke-Checked $debugExe @('--emit',$debugBundle)
Invoke-Checked $releaseExe @('--emit',$releaseBundle)
if (-not (Test-BytesEqual $debugBundle $releaseBundle)) {
    throw 'Spiral 6 CU3 independent-authority evidence differs across Debug/Release.'
}

Write-Host '============================================================'
Write-Host 'SGE4-5 SPIRAL 6 COMPLETION UNIT 3 PASSED'
Write-Host 'Authority: Raw Sparse Candidate -> Planner-independent Verifier -> opaque Verified Plan'
Write-Host 'Bindings: Exact Set / Representation Role / Program / Write Set / Artifact / Resource / Device epoch'
Write-Host 'Mutations: 36 Raw proposal attacks plus set/context/resource/epoch replay gates rejected'
Write-Host 'Verified WARP execution: HashScatterPermutation K=1024, Compact Index List, device epoch 1'
Write-Host 'Runtime sparse-policy decision: None'
Write-Host "CU3 authority evidence SHA-256: $(Get-SGE4FileSha256 $debugBundle)"
Write-Host '============================================================'
