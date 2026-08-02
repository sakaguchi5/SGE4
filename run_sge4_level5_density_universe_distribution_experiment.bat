@echo off
setlocal
chcp 65001 >nul

rem 旧Runnerが各Project直下へ生成した不要なbuild treeを削除する。
for /d %%P in ("%~dp0projects\*") do if exist "%%~fP\build" rmdir /s /q "%%~fP\build"
for /d %%P in ("%~dp0experiments\*") do if exist "%%~fP\build" rmdir /s /q "%%~fP\build"

call "%~dp0verify_source_manifest.bat" || exit /b 1
call "%~dp0tools\locate_msbuild.bat" || exit /b 1
"%MSBUILD_EXE%" "%~dp0NewSGE4.sln" /m /nr:false /t:65_Level5DensityUniverseDistributionExperiment /p:Configuration=Release /p:Platform=x64 || exit /b 1
if not exist "%~dp0build\evidence" mkdir "%~dp0build\evidence"

echo ============================================================
echo Level 5 垂直実験2b WARP交差面資格
"%~dp0build\bin\x64\Release\65_Level5DensityUniverseDistributionExperiment.exe" --warp --debug-layer --quick --output "%~dp0build\evidence\level5_vertical_v2b_warp.csv" || exit /b 1

echo ============================================================
echo Level 5 垂直実験2b 実GPU Universe 1024
"%~dp0build\bin\x64\Release\65_Level5DensityUniverseDistributionExperiment.exe" --hardware --width 32 --height 32 --global-warmup 12 --global-warmup-max 64 --warmup 16 --warmup-max 64 --warmup-window 8 --warmup-tolerance 0.05 --regime-threshold 1.20 --samples 16 --output "%~dp0build\evidence\level5_vertical_v2b_hardware_u1024.csv" || exit /b 1

echo ============================================================
echo Level 5 垂直実験2b 実GPU Universe 4096
"%~dp0build\bin\x64\Release\65_Level5DensityUniverseDistributionExperiment.exe" --hardware --width 64 --height 64 --global-warmup 12 --global-warmup-max 64 --warmup 16 --warmup-max 64 --warmup-window 8 --warmup-tolerance 0.05 --regime-threshold 1.20 --samples 16 --output "%~dp0build\evidence\level5_vertical_v2b_hardware_u4096.csv" || exit /b 1

echo ============================================================
echo Level 5 垂直実験2b 実GPU Universe 16384
"%~dp0build\bin\x64\Release\65_Level5DensityUniverseDistributionExperiment.exe" --hardware --width 128 --height 128 --global-warmup 12 --global-warmup-max 64 --warmup 16 --warmup-max 64 --warmup-window 8 --warmup-tolerance 0.05 --regime-threshold 1.20 --samples 16 --output "%~dp0build\evidence\level5_vertical_v2b_hardware_u16384.csv" || exit /b 1

echo ============================================================
echo SGE4 Level 5 垂直実験2bが完了しました。
echo WARP: build\evidence\level5_vertical_v2b_warp.csv
echo Hardware: build\evidence\level5_vertical_v2b_hardware_u1024.csv
echo Hardware: build\evidence\level5_vertical_v2b_hardware_u4096.csv
echo Hardware: build\evidence\level5_vertical_v2b_hardware_u16384.csv
echo 各Raw Evidenceと同じ場所に_surface.csvと_timing.csvを生成します。
echo ============================================================
exit /b 0
