$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
$manifest = Join-Path $root "SOURCE_MANIFEST.sha256"

function Get-RelativePathCompat {
  param(
    [Parameter(Mandatory = $true)][string]$BasePath,
    [Parameter(Mandatory = $true)][string]$Path
  )

  $baseFull = [System.IO.Path]::GetFullPath($BasePath)
  $pathFull = [System.IO.Path]::GetFullPath($Path)
  $separator = [System.IO.Path]::DirectorySeparatorChar

  if (-not $baseFull.EndsWith([string]$separator)) {
    $baseFull += $separator
  }

  if (-not $pathFull.StartsWith($baseFull, [System.StringComparison]::OrdinalIgnoreCase)) {
    throw "Root外のPathは処理できません： $Path"
  }

  return $pathFull.Substring($baseFull.Length).Replace('\', '/')
}

$expected = @{}
foreach ($line in Get-Content -LiteralPath $manifest) {
  if ([string]::IsNullOrWhiteSpace($line)) { continue }
  $parts = $line -split "  ", 2
  if ($parts.Count -ne 2) { throw "Manifestの行形式が無効です： $line" }
  if ($expected.ContainsKey($parts[1])) { throw "Manifestに重複したPathがあります： $($parts[1])" }
  $expected[$parts[1]] = $parts[0]
}

$actualPaths = @()
Get-ChildItem -LiteralPath $root -Recurse -File | ForEach-Object {
  $relative = (Get-RelativePathCompat -BasePath $root -Path $_.FullName)
  $parts = $relative.Split('/')
  if ($relative -eq "SOURCE_MANIFEST.sha256") { return }
  if ($parts -contains "build" -or $parts -contains ".vs") { return }
  if ($relative.EndsWith(".user") -or $relative.EndsWith(".suo") -or
      $relative.EndsWith(".VC.db") -or $relative.EndsWith(".VC.opendb")) { return }
  $actualPaths += $relative
}

$missing = @($expected.Keys | Where-Object { -not (Test-Path -LiteralPath (Join-Path $root $_)) })
if ($missing.Count -ne 0) {
  throw "Manifest登録ファイルがありません： $($missing -join ', ')"
}

$untracked = @($actualPaths | Where-Object { -not $expected.ContainsKey($_) })
if ($untracked.Count -ne 0) {
  throw "Manifestに未登録のファイルがあります： $($untracked -join ', ')"
}

foreach ($relative in ($expected.Keys | Sort-Object)) {
  $path = Join-Path $root $relative
  $actual = (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash.ToLowerInvariant()
  if ($actual -ne $expected[$relative]) { throw "Digestが一致しません： $relative" }
}
Write-Host "New SGE4 SOURCE_MANIFEST検証に合格しました。登録ファイル数：$($expected.Count)"
