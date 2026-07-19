param(
    [Parameter(Mandatory = $true)]
    [ValidateSet('Stage13','Stage14','CU6')]
    [string]$Mode,
    [uint32]$Warmup = 200,
    [uint32]$Measurement = 2000,
    [uint32]$Runs = 5,
    [uint32]$AdapterIndex = 0,
    [string]$ClockPolicy = 'uncontrolled'
)
$ErrorActionPreference = 'Stop'
$utf8NoBom = New-Object System.Text.UTF8Encoding($false)
[Console]::InputEncoding = $utf8NoBom
[Console]::OutputEncoding = $utf8NoBom
$OutputEncoding = $utf8NoBom
$testsRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$root = Split-Path -Parent $testsRoot
. (Join-Path $testsRoot 'tools\Sha256.ps1')
function Invoke-Checked([string]$FilePath,[string[]]$Arguments=@()){& $FilePath @Arguments;if($LASTEXITCODE-ne 0){throw "Command failed: $FilePath"}}
function Get-MSBuild{$vswhere=Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe';if(-not(Test-Path -LiteralPath $vswhere)){throw 'vswhere.exe was not found.'};$found=& $vswhere -latest -products * -requires Microsoft.Component.MSBuild -find 'MSBuild\**\Bin\MSBuild.exe';$msbuild=$found|Select-Object -Last 1;if(-not $msbuild){throw 'MSBuild was not found.'};return $msbuild}
function Build-Project([string]$ProjectName,[string]$Configuration){$msbuild=Get-MSBuild;$project=Join-Path $root "$ProjectName\$ProjectName.vcxproj";$solutionDir=$root.TrimEnd('\','/')+'\';Invoke-Checked $msbuild @($project,'/m','/nologo','/t:Build',"/p:Configuration=$Configuration",'/p:Platform=x64',"/p:SolutionDir=$solutionDir")}
foreach($script in @('tools\Verify-SGE4_5Identity.ps1','tools\Verify-ScriptContracts.ps1','tools\Verify-SourceManifest.ps1','tools\Verify-Spiral1ResearchContract.ps1','tools\Verify-Spiral1CU1.ps1','tools\Verify-Spiral1CU2.ps1','tools\Verify-Spiral1CU3.ps1','tools\Verify-Spiral1CU4.ps1','tools\Verify-Spiral1CU5.ps1','tools\Verify-Spiral1CU6.ps1')){Invoke-Checked 'powershell.exe' @('-NoLogo','-NoProfile','-NonInteractive','-ExecutionPolicy','Bypass','-File',(Join-Path $testsRoot $script))}
Build-Project '79_Spiral1Launcher' 'Debug';Build-Project '79_Spiral1Launcher' 'Release'
$output=Join-Path $root 'build\tests\spiral1-cu6\measurement';$selfTest=Join-Path $root 'build\tests\spiral1-cu6\self-test';New-Item -ItemType Directory -Force -Path $selfTest|Out-Null
Invoke-Checked (Join-Path $root 'build\bin\x64\Debug\79_Spiral1Launcher.exe') @('--self-test','--output',$selfTest)
Invoke-Checked (Join-Path $root 'build\bin\x64\Release\79_Spiral1Launcher.exe') @('--self-test','--output',$selfTest)
if($Mode -in @('Stage13','CU6')){Remove-Item -Recurse -Force -ErrorAction SilentlyContinue $output;New-Item -ItemType Directory -Force -Path $output|Out-Null;Invoke-Checked (Join-Path $root 'build\bin\x64\Release\79_Spiral1Launcher.exe') @('--measure','--output',$output,'--warmup',"$Warmup",'--measurement',"$Measurement",'--runs',"$Runs",'--adapter-index',"$AdapterIndex",'--clock-policy',$ClockPolicy)}
$evidence=Join-Path $output 'benchmark_evidence_v1.bin'
if($Mode -in @('Stage14','CU6')){if(-not(Test-Path -LiteralPath $evidence)){throw 'Stage 14 requires Stage 13 benchmark_evidence_v1.bin. Run Stage 13 first.'};Invoke-Checked (Join-Path $root 'build\bin\x64\Release\79_Spiral1Launcher.exe') @('--report','--input',$evidence,'--output',$output);$report=Join-Path $output 'SPIRAL1_DECISION_EVIDENCE_REPORT.md';$text=Get-Content -Raw -LiteralPath $report;if($text -notmatch 'DeferredByOwner'){throw 'Decision evidence did not preserve deferred selection.'}}
$evidenceDigest=if(Test-Path -LiteralPath $evidence){Get-SGE4FileSha256 $evidence}else{'NOT-RUN'}
$reportPath=Join-Path $output 'SPIRAL1_DECISION_EVIDENCE_REPORT.md';$reportDigest=if(Test-Path -LiteralPath $reportPath){Get-SGE4FileSha256 $reportPath}else{'NOT-RUN'}
Write-Host '============================================================'
if($Mode-eq'Stage13'){Write-Host 'SGE4-5 STAGE 13 REAL GPU MEASUREMENT PASSED'}elseif($Mode-eq'Stage14'){Write-Host 'SGE4-5 STAGE 14 DECISION EVIDENCE FREEZE PASSED'}else{Write-Host 'SGE4-5 SPIRAL 1 MEASUREMENT AND DECISION EVIDENCE COMPLETE';Write-Host 'Completion Unit 6: Stage 13 plus non-selection Stage 14 scope'}
Write-Host 'S1-I17: Qualified by hardware ABBA report'
Write-Host 'S1-I18: Policy preserved; next capability selection intentionally deferred by owner'
Write-Host 'No Spiral 2 option or Level 4 extension was selected'
Write-Host "Benchmark evidence SHA-256: $evidenceDigest"
Write-Host "Decision evidence report SHA-256: $reportDigest"
Write-Host 'Spiral 1 Experiment Complete banner remains withheld until the deferred owner decision'
Write-Host '============================================================'
