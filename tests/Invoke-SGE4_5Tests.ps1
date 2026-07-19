param(
    [Parameter(Mandatory = $true)]
    [ValidateSet(
        'Dev', 'Gate', 'Regression', 'Freeze',
        'Architecture', 'Semantic', 'Planning', 'Authority',
        'Package', 'Runtime', 'Frontend', 'Corpus', 'Adversarial')]
    [string]$Suite,

    [ValidateSet('Debug', 'Release')]
    [string]$Configuration = 'Debug',

    [switch]$NoBuild
)

$ErrorActionPreference = 'Stop'

$utf8NoBom = New-Object System.Text.UTF8Encoding($false)
[Console]::InputEncoding = $utf8NoBom
[Console]::OutputEncoding = $utf8NoBom
$OutputEncoding = $utf8NoBom

$testsRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$root = Split-Path -Parent $testsRoot
$bin = Join-Path $root "build\bin\x64\$Configuration"
$logRoot = Join-Path $root 'docs\test-logs'
$testOutputRoot = Join-Path $root 'build\tests'
New-Item -ItemType Directory -Force -Path $logRoot | Out-Null
New-Item -ItemType Directory -Force -Path $testOutputRoot | Out-Null
$timestamp = Get-Date -Format 'yyyyMMdd-HHmmss'
$logPath = Join-Path $logRoot "$timestamp-$Suite-$Configuration.log"

function Write-Section([string]$Text) {
    $line = '=' * 72
    Write-Host "`n$line"
    Write-Host $Text
    Write-Host $line
    Add-Content -LiteralPath $logPath -Value "`n$line`n$Text`n$line" -Encoding UTF8
}

function Invoke-Native([string]$FilePath, [string[]]$Arguments = @()) {
    $display = '"' + $FilePath + '"'
    if ($Arguments.Count -gt 0) { $display += ' ' + ($Arguments -join ' ') }
    Write-Host "> $display"
    Add-Content -LiteralPath $logPath -Value "> $display" -Encoding UTF8
    & $FilePath @Arguments 2>&1 | ForEach-Object {
        $text = [string]$_
        Write-Host $text
        Add-Content -LiteralPath $logPath -Value $text -Encoding UTF8
    }
    $code = $LASTEXITCODE
    if ($code -ne 0) {
        throw "Command failed with exit code ${code}: $display"
    }
}

function Get-MSBuild {
    $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
    if (-not (Test-Path -LiteralPath $vswhere)) {
        throw 'vswhere.exe was not found.'
    }
    $found = & $vswhere -latest -products * -requires Microsoft.Component.MSBuild -find 'MSBuild\**\Bin\MSBuild.exe'
    $msbuild = $found | Select-Object -Last 1
    if (-not $msbuild) { throw 'MSBuild was not found.' }
    return $msbuild
}

function Get-ProjectPath([string]$ProjectName) {
    $project = Get-ChildItem -Path $root -Recurse -Filter "$ProjectName.vcxproj" |
        Where-Object { $_.Directory.Name -eq $ProjectName } |
        Select-Object -First 1
    if (-not $project) { throw "Project was not found: $ProjectName" }
    return $project.FullName
}

function Build-Projects([string[]]$ProjectNames) {
    if ($NoBuild) {
        Write-Host 'Build skipped because -NoBuild was specified.'
        Add-Content -LiteralPath $logPath -Value 'Build skipped because -NoBuild was specified.' -Encoding UTF8
        return
    }
    $msbuild = Get-MSBuild
    $solutionDir = $root.TrimEnd('\', '/') + '\'
    foreach ($name in ($ProjectNames | Select-Object -Unique)) {
        $projectPath = Get-ProjectPath $name
        Invoke-Native $msbuild @(
            $projectPath,
            '/m', '/nr:false', '/nologo', '/t:Build',
            "/p:Configuration=$Configuration",
            '/p:Platform=x64',
            "/p:SolutionDir=$solutionDir"
        )
    }
}

function Build-Solution {
    if ($NoBuild) {
        Write-Host 'Solution build skipped because -NoBuild was specified.'
        Add-Content -LiteralPath $logPath -Value 'Solution build skipped because -NoBuild was specified.' -Encoding UTF8
        return
    }
    $msbuild = Get-MSBuild
    Invoke-Native $msbuild @(
        (Join-Path $root 'SemanticGpuEngine4-5.sln'),
        '/m', '/nr:false', '/nologo', '/t:Build',
        "/p:Configuration=$Configuration",
        '/p:Platform=x64'
    )
}

function Invoke-Test([string]$Name, [string[]]$Arguments = @()) {
    $exe = Join-Path $bin "$Name.exe"
    if (-not (Test-Path -LiteralPath $exe)) {
        throw "Test executable was not found: $exe"
    }
    Write-Section "Running $Name"
    Invoke-Native $exe $Arguments
}

function Invoke-DependencyChecks {
    Write-Section 'Architectural dependency checks'
    Invoke-Native 'powershell.exe' @(
        '-NoLogo', '-NoProfile', '-NonInteractive',
        '-ExecutionPolicy', 'Bypass',
        '-File', (Join-Path $root 'verify_dependencies.ps1')
    )
}

function Invoke-ScriptChecks {
    Write-Section 'Script syntax and encoding checks'
    Invoke-Native 'powershell.exe' @(
        '-NoLogo', '-NoProfile', '-NonInteractive',
        '-ExecutionPolicy', 'Bypass',
        '-File', (Join-Path $testsRoot 'tools\Verify-ScriptContracts.ps1')
    )
}

function Invoke-SourceInventoryCheck {
    Write-Section 'Frozen source inventory check'
    Invoke-Native 'powershell.exe' @(
        '-NoLogo', '-NoProfile', '-NonInteractive',
        '-ExecutionPolicy', 'Bypass',
        '-File', (Join-Path $testsRoot 'tools\Verify-SourceManifest.ps1')
    )
}

function Invoke-ArchitectureChecks {
    Invoke-DependencyChecks
    Invoke-ScriptChecks
    Invoke-SourceInventoryCheck
}

function Generate-QualificationPackage {
    $package = Join-Path $testOutputRoot 'SGE4_5_TieredTests_CommonExperiment.sgep'
    $compiler = Join-Path $bin '51_PackageCompiler.exe'
    if (-not (Test-Path -LiteralPath $compiler)) {
        throw "Package compiler was not found: $compiler"
    }
    Write-Section 'Generating Runtime qualification Package'
    Invoke-Native $compiler @($package, 'all')
    return $package
}

Set-Content -LiteralPath $logPath -Value "SGE4-5 tiered test run`nSuite=$Suite`nConfiguration=$Configuration`nStarted=$(Get-Date -Format o)" -Encoding UTF8

try {
    switch ($Suite) {
        'Architecture' {
            Invoke-ArchitectureChecks
        }
        'Semantic' {
            Build-Projects @('30_SemanticContractTests')
            Invoke-Test '30_SemanticContractTests'
        }
        'Planning' {
            Build-Projects @('33_PlanningTests', '34_AuthorityTests')
            Invoke-Test '33_PlanningTests'
            Invoke-Test '34_AuthorityTests'
        }
        'Authority' {
            Build-Projects @('34_AuthorityTests')
            Invoke-Test '34_AuthorityTests'
        }
        'Package' {
            Build-Projects @(
                '31_PackageContractTests',
                '32_CompilerPipelineTests',
                '35_D3D12ConformanceTests'
            )
            Invoke-Test '31_PackageContractTests'
            Invoke-Test '32_CompilerPipelineTests'
            Invoke-Test '35_D3D12ConformanceTests'
        }
        'Runtime' {
            Build-Projects @(
                '51_PackageCompiler',
                '37_RuntimeSemanticTests',
                '41_SliceExecutionTests',
                '36_D3D12ReadbackTests',
                '45_PlannedRuntimeTests'
            )
            $package = Generate-QualificationPackage
            Invoke-Test '37_RuntimeSemanticTests'
            Invoke-Test '41_SliceExecutionTests'
            Invoke-Test '36_D3D12ReadbackTests' @($package)
            Invoke-Test '45_PlannedRuntimeTests'
        }
        'Frontend' {
            Build-Projects @('39_FrontendEquivalenceTests', '40_SliceScenarioTests')
            Invoke-Test '39_FrontendEquivalenceTests'
            Invoke-Test '40_SliceScenarioTests'
        }
        'Corpus' {
            Build-Projects @(
                '42_MetamorphicTests',
                '43_GeneratedGraphTests',
                '44_CanonicalFreezeTests'
            )
            Invoke-Test '42_MetamorphicTests'
            Invoke-Test '43_GeneratedGraphTests'
            Invoke-Test '44_CanonicalFreezeTests'
        }
        'Adversarial' {
            Build-Projects @('38_AdversarialBoundaryTests')
            Invoke-Test '38_AdversarialBoundaryTests'
        }
        'Dev' {
            Invoke-DependencyChecks
            Build-Projects @(
                '30_SemanticContractTests',
                '31_PackageContractTests',
                '32_CompilerPipelineTests',
                '34_AuthorityTests'
            )
            Invoke-Test '30_SemanticContractTests'
            Invoke-Test '31_PackageContractTests'
            Invoke-Test '32_CompilerPipelineTests'
            Invoke-Test '34_AuthorityTests'
        }
        'Gate' {
            Invoke-ArchitectureChecks
            Build-Solution
            foreach ($name in @(
                '30_SemanticContractTests',
                '31_PackageContractTests',
                '32_CompilerPipelineTests',
                '33_PlanningTests',
                '34_AuthorityTests',
                '35_D3D12ConformanceTests',
                '38_AdversarialBoundaryTests',
                '39_FrontendEquivalenceTests',
                '40_SliceScenarioTests',
                '42_MetamorphicTests'
            )) {
                Invoke-Test $name
            }
        }
        'Regression' {
            Invoke-ArchitectureChecks
            Write-Section 'Full Debug regression qualification'
            Invoke-Native (Join-Path $root 'run_tests.bat') @($Configuration)
        }
        'Freeze' {
            Write-Section 'Formal Stage 0D freeze qualification'
            Invoke-Native (Join-Path $root 'run_sge4_5_stage0d.bat')
        }
    }

    $message = "SGE4-5 test suite passed: $Suite ($Configuration)"
    Write-Section $message
    Write-Host "Log: $logPath"
    exit 0
}
catch {
    $message = "SGE4-5 test suite failed: $Suite ($Configuration)`n$($_.Exception.Message)"
    Write-Host $message -ForegroundColor Red
    Add-Content -LiteralPath $logPath -Value $message -Encoding UTF8
    Write-Host "Log: $logPath"
    exit 1
}
