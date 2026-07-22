$ErrorActionPreference = 'Stop'
$testsRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$root = Split-Path -Parent $testsRoot
. (Join-Path $testsRoot 'Spiral5Common.ps1')
Invoke-Spiral5Checks @('tools\Verify-Spiral5CU2.ps1','tools\Verify-Spiral5CU3.ps1')
Build-Spiral5Project '146_Spiral5AuthorityMutationTests\146_Spiral5AuthorityMutationTests.vcxproj'
$output = Join-Path $root 'build\tests\spiral5-cu3'
Remove-Item -Recurse -Force -ErrorAction SilentlyContinue $output
New-Item -ItemType Directory -Force $output | Out-Null
$debugExe = Join-Path $root 'build\bin\x64\Debug\146_Spiral5AuthorityMutationTests.exe'
$releaseExe = Join-Path $root 'build\bin\x64\Release\146_Spiral5AuthorityMutationTests.exe'
$debugBundle = Join-Path $output 'debug.bin'
$releaseBundle = Join-Path $output 'release.bin'
Invoke-Checked $debugExe @('--emit',$debugBundle)
Invoke-Checked $releaseExe @('--emit',$releaseBundle)
if (-not (Test-BytesEqual $debugBundle $releaseBundle)) { throw 'Spiral 5 CU3 authority evidence differs across Debug/Release.' }
Write-Host '============================================================'
Write-Host 'SGE4-5 SPIRAL 5 COMPLETION UNIT 3 PASSED'
Write-Host 'Authority: Raw Temporal Candidate -> Planner-independent Verifier -> opaque Verified Plan'
Write-Host 'Bindings: Schedule / Generation / History Role / CU2 artifact / Program / Resource / Device epoch'
Write-Host 'Mutations: 40 Raw proposal attacks plus schedule/context/resource/epoch replay gates rejected'
Write-Host 'Verified WARP execution: P4, 128 invocations, device epoch 1'
Write-Host 'Runtime temporal-policy decision: None'
Write-Host "CU3 authority evidence SHA-256: $(Get-SGE4FileSha256 $debugBundle)"
Write-Host '============================================================'
