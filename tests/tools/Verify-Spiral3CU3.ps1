$ErrorActionPreference='Stop'
$root=Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
foreach($name in @('106_Spiral3LeafCompiler','113_Spiral3LeafPackageTests')){if(-not(Test-Path (Join-Path $root "$name\$name.vcxproj"))){throw "Missing CU3 project: $name"}}
[xml]$compilerProject=Get-Content -Raw (Join-Path $root '106_Spiral3LeafCompiler\106_Spiral3LeafCompiler.vcxproj')
$references=@($compilerProject.SelectNodes("//*[local-name()='ProjectReference']")|ForEach-Object{$_.Include})
if($references -match '104_ReuseRepresentationPlanner'){throw 'Spiral 3 Leaf Compiler references Planner'}
if(-not ($references -match '105_ReuseRepresentationVerifier')){throw 'Spiral 3 Leaf Compiler does not consume verifier authority'}
$header=Get-Content -Raw (Join-Path $root '106_Spiral3LeafCompiler\Spiral3LeafCompiler.h')
if($header -match 'RawCandidateGraphV1'){throw 'Leaf Compiler public API accepts Raw Candidate state'}
foreach($required in @('VerifiedReuseRepresentationV1','CompileVerifiedReuseLeafGroupV1','CompileDynamicInvocationSourceV1','CompileConsumerPointSourceV1')){if($header -notmatch [regex]::Escape($required)){throw "Missing CU3 verified Leaf boundary: $required"}}
$tests=Get-Content -Raw (Join-Path $root '113_Spiral3LeafPackageTests\main.cpp')
foreach($required in @('is_default_constructible_v','is_constructible_v','is_convertible_v','groups==18','leaves==60','mutations==80')){if($tests -notmatch [regex]::Escape($required)){throw "Missing CU3 type or evidence gate: $required"}}
Write-Host 'Spiral 3 CU3 Verified-only Leaf Compiler, Schema 17 package binding, and mutation boundaries passed.'
