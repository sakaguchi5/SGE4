$ErrorActionPreference='Stop'
$testsRoot=Split-Path -Parent $MyInvocation.MyCommand.Path
$root=Split-Path -Parent $testsRoot
. (Join-Path $testsRoot 'Spiral3Common.ps1')
. (Join-Path $testsRoot 'tools\Sha256.ps1')

Invoke-Spiral3Checks @('tools\Verify-Spiral3CU5.ps1')
$msbuild=Get-MSBuild
$targets='108_Spiral3Execution;115_Spiral3WarpObservationTests;116_Spiral3RecoveryTests;117_Spiral3FreezeTests'
foreach($configuration in @('Debug','Release')){
    Invoke-Checked $msbuild @((Join-Path $root 'SemanticGpuEngine4-5.sln'),'/m','/nr:false','/nologo',"/t:$targets", "/p:Configuration=$configuration",'/p:Platform=x64','/v:q')
}

$output=Join-Path $root 'build\tests\spiral3-cu5'
New-Item -ItemType Directory -Force -Path $output|Out-Null
foreach($configuration in @('Debug','Release')){
    $bin=Join-Path $root "build\bin\x64\$configuration"
    $warp=Join-Path $bin '115_Spiral3WarpObservationTests.exe'
    for($index=0;$index-lt6;++$index){
        Invoke-Checked $warp @('--warp-index',[string]$index,'--emit',(Join-Path $output "$configuration-R$index.bin"))
    }
    Invoke-Checked $warp @('--reorder')

    $recovery=Join-Path $bin '116_Spiral3RecoveryTests.exe'
    Invoke-Checked $recovery @('--controlled','--emit',(Join-Path $output "$configuration-controlled.bin"))
    $actual=Join-Path $output "$configuration-actual.bin"
    $fresh=Join-Path $output "$configuration-fresh.bin"
    Invoke-Checked $recovery @('--actual-removal','--emit',$actual)
    Invoke-Checked $recovery @('--fresh-rematerialize','--emit',$fresh)
    if(-not(Test-BytesEqual $actual $fresh)){throw "$configuration actual-removal/fresh-process evidence differs."}
}

for($index=0;$index-lt6;++$index){
    if(-not(Test-BytesEqual (Join-Path $output "Debug-R$index.bin") (Join-Path $output "Release-R$index.bin"))){throw "R-index $index WARP evidence differs across Debug/Release."}
}
if(-not(Test-BytesEqual (Join-Path $output 'Debug-controlled.bin') (Join-Path $output 'Release-controlled.bin'))){throw 'Controlled recovery evidence differs across Debug/Release.'}
if(-not(Test-BytesEqual (Join-Path $output 'Debug-fresh.bin') (Join-Path $output 'Release-fresh.bin'))){throw 'Fresh rematerialization evidence differs across Debug/Release.'}

$debugFreeze=Join-Path $root 'build\bin\x64\Debug\117_Spiral3FreezeTests.exe'
$releaseFreeze=Join-Path $root 'build\bin\x64\Release\117_Spiral3FreezeTests.exe'
$freezeA=Join-Path $output 'freeze-debug-a.bin'
$freezeB=Join-Path $output 'freeze-debug-b.bin'
$freezeRelease=Join-Path $output 'freeze-release.bin'
Invoke-Checked $debugFreeze @('--emit',$freezeA)
Invoke-Checked $debugFreeze @('--emit',$freezeB)
Invoke-Checked $releaseFreeze @('--emit',$freezeRelease)
if(-not(Test-BytesEqual $freezeA $freezeB)-or-not(Test-BytesEqual $freezeA $freezeRelease)){throw 'Spiral 3 Freeze bundle is not Debug/Release/fresh-process deterministic.'}

$warpCorpus=Join-Path $output 'warp-corpus-debug.bin'
$stream=[IO.File]::Open($warpCorpus,[IO.FileMode]::Create,[IO.FileAccess]::Write,[IO.FileShare]::None)
try{for($index=0;$index-lt6;++$index){$bytes=[IO.File]::ReadAllBytes((Join-Path $output "Debug-R$index.bin"));$stream.Write($bytes,0,$bytes.Length)}}finally{$stream.Dispose()}

Write-Host '============================================================'
Write-Host 'SGE4-5 SPIRAL 3 COMPLETION UNIT 5 PASSED'
Write-Host '72 WARP R/F executions; point-source authority, frame-order permutation, deterministic GPU observation records'
Write-Host 'Recovery: eleven-Leaf/twelve-Flow controlled rebuild, stale-epoch rejection, actual RemoveDevice quarantine'
Write-Host "Freeze evidence SHA-256: $(Get-SGE4FileSha256 $freezeA)"
Write-Host "WARP corpus SHA-256: $(Get-SGE4FileSha256 $warpCorpus)"
Write-Host "R256/F07 rematerialization SHA-256: $(Get-SGE4FileSha256 (Join-Path $output 'Debug-fresh.bin'))"
Write-Host '============================================================'
