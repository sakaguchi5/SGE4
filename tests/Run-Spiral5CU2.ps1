$ErrorActionPreference = 'Stop'
$testsRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$root = Split-Path -Parent $testsRoot
. (Join-Path $testsRoot 'Spiral5Common.ps1')
Invoke-Spiral5Checks @('tools\Verify-Spiral5CU2.ps1')
Build-Spiral5Project '145_Spiral5TemporalArchitectureTests\145_Spiral5TemporalArchitectureTests.vcxproj'
$output = Join-Path $root 'build\tests\spiral5-cu2'
Remove-Item -Recurse -Force -ErrorAction SilentlyContinue $output
New-Item -ItemType Directory -Force $output | Out-Null
$debugExe = Join-Path $root 'build\bin\x64\Debug\145_Spiral5TemporalArchitectureTests.exe'
$releaseExe = Join-Path $root 'build\bin\x64\Release\145_Spiral5TemporalArchitectureTests.exe'
$debugBundle = Join-Path $output 'debug.bin'
$releaseBundle = Join-Path $output 'release.bin'
Invoke-Checked $debugExe @('--emit',$debugBundle)
Invoke-Checked $releaseExe @('--emit',$releaseBundle)
if (-not (Test-BytesEqual $debugBundle $releaseBundle)) { throw 'Spiral 5 CU2 architecture evidence differs across Debug/Release.' }
Write-Host '============================================================'
Write-Host 'SGE4-5 SPIRAL 5 COMPLETION UNIT 2 PASSED'
Write-Host 'Architecture: verified Update Schedule -> GPU update/hold argument -> Global Motor History -> Consumer'
Write-Host 'Schedules: P1/P4/P64/Irregular; 4 x 128 = 512 WARP invocations per build'
Write-Host 'History: depth 1, generation-bound, initial update required, Hold byte-stable'
Write-Host 'Completion: every Hold has explicit Consumer completion and retained-history authority'
Write-Host 'Legacy boundary: Schema 17 / Runtime 17 / canonical Backend unchanged'
Write-Host 'Runtime temporal-policy decision: None'
Write-Host "CU2 architecture evidence SHA-256: $(Get-SGE4FileSha256 $debugBundle)"
Write-Host '============================================================'
