param(
    [ValidateSet('SelfTest','Measurement','Report','All')]
    [string]$Mode='All',
    [string]$OutputDirectory='build/tests/spiral3/measurement'
)
$ErrorActionPreference='Stop'
$testsRoot=Split-Path -Parent $MyInvocation.MyCommand.Path
$root=Split-Path -Parent $testsRoot
. (Join-Path $testsRoot 'Spiral3Common.ps1')
. (Join-Path $testsRoot 'tools\Sha256.ps1')

Invoke-Spiral3Checks @('tools\Verify-Spiral3CU6.ps1')
$msbuild=Get-MSBuild
$targets='109_Spiral3PerformanceExperiment;118_Spiral3PerformanceTests;119_Spiral3Launcher'
foreach($configuration in @('Debug','Release')){
    Invoke-Checked $msbuild @((Join-Path $root 'SemanticGpuEngine4-5.sln'),'/m','/nr:false','/nologo',"/t:$targets", "/p:Configuration=$configuration",'/p:Platform=x64','/v:q')
}

$selfRoot=Join-Path $root 'build\tests\spiral3-cu6\self-test'
$debugSelf=Join-Path $selfRoot 'debug'
$releaseSelf=Join-Path $selfRoot 'release'
if($Mode -in @('SelfTest','All')){
    Remove-Item -Recurse -Force -ErrorAction SilentlyContinue $selfRoot
    Invoke-Checked (Join-Path $root 'build\bin\x64\Debug\118_Spiral3PerformanceTests.exe') @('--output',$debugSelf)
    Invoke-Checked (Join-Path $root 'build\bin\x64\Release\118_Spiral3PerformanceTests.exe') @('--output',$releaseSelf)
    foreach($name in @('spiral3_measurement_evidence_v1.bin','SPIRAL3_DECISION_EVIDENCE_REPORT.md','measurement_samples.csv','measurement_profile.json')){
        if(-not(Test-BytesEqual (Join-Path $debugSelf $name) (Join-Path $releaseSelf $name))){throw "CU6 synthetic $name differs across Debug/Release."}
    }
}

$output=Join-Path $root $OutputDirectory
$launcher=Join-Path $root 'build\bin\x64\Release\119_Spiral3Launcher.exe'
if($Mode -in @('Measurement','All')){
    Remove-Item -Recurse -Force -ErrorAction SilentlyContinue $output
    Invoke-Checked $launcher @('--measure','--output',$output,'--clock-policy','uncontrolled')
}
if($Mode -in @('Report','All')){
    $evidence=Join-Path $output 'spiral3_measurement_evidence_v1.bin'
    Invoke-Checked $launcher @('--evidence','--report','--input',$evidence,'--output',$output)
    Invoke-Checked powershell.exe @('-NoLogo','-NoProfile','-NonInteractive','-ExecutionPolicy','Bypass','-File',(Join-Path $testsRoot 'tools\Verify-Spiral3CU6.ps1'),'-RequireClosure','-OutputDirectory',$OutputDirectory)
}

if($Mode -in @('Report','All')){
    $evidence=Join-Path $output 'spiral3_measurement_evidence_v1.bin'
    $report=Join-Path $output 'SPIRAL3_DECISION_EVIDENCE_REPORT.md'
    $reportText=Get-Content -Raw -LiteralPath $report -Encoding UTF8
    $adapter=[regex]::Match($reportText,'(?m)^- Adapter: (.+)$').Groups[1].Value
    $classification=[regex]::Match($reportText,'(?m)^CrossoverClassification = (.+)$').Groups[1].Value
    $transitionLines=@([regex]::Matches($reportText,'(?m)^ObservedTransition = (.+)$')|ForEach-Object{$_.Groups[1].Value})
    Write-Host '============================================================'
    Write-Host 'SGE4-5 SPIRAL 3 COMPLETION UNIT 6 PASSED'
    Write-Host 'Real GPU: six fixed-R cases, six balanced A/B/C orders, 96 measured order samples per R'
    Write-Host "Adapter: $adapter"
    Write-Host "Crossover classification: $classification"
    foreach($line in $transitionLines){Write-Host "Observed transition: $line"}
    Write-Host "Measurement evidence SHA-256: $(Get-SGE4FileSha256 $evidence)"
    Write-Host 'NextCapabilitySelection = DeferredByOwner'
    Write-Host 'SelectionStatus = OWNER_DECISION_REQUIRED'
    Write-Host '============================================================'
}else{
    Write-Host "Spiral 3 CU6 runner passed: $Mode"
}
