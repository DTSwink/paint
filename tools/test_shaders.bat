@echo off
setlocal
set ROOT=%~dp0..
set SHADER=%ROOT%\shaders\material_ps.hlsl
set FXC="C:\Program Files (x86)\Windows Kits\10\bin\10.0.26100.0\x64\fxc.exe"
if not exist %FXC% (
  echo fxc not found, skipping offline compile test
  exit /b 0
)
echo Testing HLSL compile...
%FXC% /nologo /T ps_5_0 /E PSMain /I "%ROOT%\shaders" "%SHADER%" >nul
if errorlevel 1 (
  echo FAIL: material_ps.hlsl did not compile
  exit /b 1
)
echo PASS: material_ps.hlsl compiles
exit /b 0
