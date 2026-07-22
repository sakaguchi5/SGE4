$ErrorActionPreference='Stop'
$testsRoot=Split-Path -Parent $MyInvocation.MyCommand.Path
$root=Split-Path -Parent $testsRoot
. (Join-Path $testsRoot 'Spiral5Common.ps1')
Invoke-Spiral5Checks @('tools\Verify-Spiral5CU2.ps1','tools\Verify-Spiral5CU3.ps1','tools\Verify-Spiral5CU4.ps1')
Build-Spiral5Project '150_Spiral5TemporalCandidateFamilyTests\150_Spiral5TemporalCandidateFamilyTests.vcxproj'
$output=Join-Path $root 'build\tests\spiral5-cu4';Remove-Item -Recurse -Force -ErrorAction SilentlyContinue $output;New-Item -ItemType Directory -Force $output|Out-Null
$debug=Join-Path $root 'build\bin\x64\Debug\150_Spiral5TemporalCandidateFamilyTests.exe';$release=Join-Path $root 'build\bin\x64\Release\150_Spiral5TemporalCandidateFamilyTests.exe'
$debugBundle=Join-Path $output 'debug.bin';$releaseBundle=Join-Path $output 'release.bin'
Invoke-Checked $debug @('--emit',$debugBundle);Invoke-Checked $release @('--emit',$releaseBundle)
if(-not(Test-BytesEqual $debugBundle $releaseBundle)){throw 'Spiral 5 CU4 Candidate family evidence differs across Debug/Release.'}
Write-Host '============================================================'
Write-Host 'SGE4-5 SPIRAL 5 COMPLETION UNIT 4 PASSED'
Write-Host 'Candidates: A.EveryInvocationRecompute / B.GlobalMotorHistoryReuse / C.FinalOutputHistoryReuse'
Write-Host 'History positions: None / Global Motor / Final Output'
Write-Host 'Schedules: P1/P4/P64/Irregular; 4 x 3 x 128 = 1536 WARP Candidate invocations per build'
Write-Host 'Pairwise output equivalence: byte-identical at every Invocation'
Write-Host 'Authority: Candidate kind / Schedule / Generation / History Role / Resource / Device epoch'
Write-Host 'Runtime temporal-policy decision: None'
Write-Host "CU4 Candidate family evidence SHA-256: $(Get-SGE4FileSha256 $debugBundle)"
Write-Host '============================================================'
