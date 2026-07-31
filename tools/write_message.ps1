param(
    [Parameter(Mandatory = $true)]
    [ValidateSet(
        "BuildPassed",
        "ArchitecturePassed",
        "WindowsQualificationPassed",
        "ActualRemovalPassed",
        "FullGatePassed",
        "VsWhereNotFound",
        "MsBuildNotFound"
    )]
    [string]$Key
)

$utf8 = New-Object System.Text.UTF8Encoding($false)
[Console]::OutputEncoding = $utf8
$OutputEncoding = $utf8

$messages = @{
    BuildPassed = "New SGE4のDebug／Releaseビルドに合格しました。"
    ArchitecturePassed = "New SGE4の統合設計試験に合格しました。"
    WindowsQualificationPassed = "New SGE4のWindows資格試験に合格しました。"
    ActualRemovalPassed = "New SGE4の実Device削除資格試験に合格しました。"
    FullGatePassed = "NEW SGE4 統合再構築 全資格GATE 合格"
    VsWhereNotFound = "vswhere.exeが見つかりません。Visual Studio Installerを確認してください。"
    MsBuildNotFound = "MSBuild.exeが見つかりません。Visual StudioのMSBuildコンポーネントを確認してください。"
}

Write-Host $messages[$Key]
