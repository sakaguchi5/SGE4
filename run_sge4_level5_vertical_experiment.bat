@echo off
setlocal
chcp 65001 >nul

rem 旧Runnerが各Project直下へ生成した不要なbuild treeを削除する。
for /d %%P in ("%~dp0projects\*") do if exist "%%~fP\build" rmdir /s /q "%%~fP\build"
for /d %%P in ("%~dp0experiments\*") do if exist "%%~fP\build" rmdir /s /q "%%~fP\build"

call "%~dp0verify_source_manifest.bat" || exit /b 1
call "%~dp0tools\locate_msbuild.bat" || exit /b 1
"%MSBUILD_EXE%" "%~dp0NewSGE4.sln" /m /nr:false /t:63_Level5VerticalExperiment /p:Configuration=Release /p:Platform=x64 || exit /b 1
if not exist "%~dp0build\evidence" mkdir "%~dp0build\evidence"

echo ============================================================
echo Level 5 垂直実験1 WARP観測同値資格
"%~dp0build\bin\x64\Release\63_Level5VerticalExperiment.exe" --warp --debug-layer --quick --output "%~dp0build\evidence\level5_vertical_v1_warp.csv" || exit /b 1

echo ============================================================
echo Level 5 垂直実験1 実GPU測定
"%~dp0build\bin\x64\Release\63_Level5VerticalExperiment.exe" --hardware --width 64 --height 64 --warmup 6 --samples 24 --output "%~dp0build\evidence\level5_vertical_v1_hardware.csv" || exit /b 1

echo ============================================================
echo SGE4 Level 5 垂直実験1が完了しました。
echo Evidence: build\evidence\level5_vertical_v1_warp.csv
echo Evidence: build\evidence\level5_vertical_v1_hardware.csv
echo ============================================================
exit /b 0
