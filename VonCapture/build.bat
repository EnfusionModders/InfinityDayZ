@echo off
REM Build VonCapture (Release x64) + its InfinityHost dependency.
setlocal enabledelayedexpansion

set "MSBUILD="
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if exist "%VSWHERE%" (
  for /f "usebackq tokens=*" %%i in (`"%VSWHERE%" -latest -products * -property installationPath 2^>nul`) do (
    if exist "%%i\MSBuild\Current\Bin\MSBuild.exe" set "MSBUILD=%%i\MSBuild\Current\Bin\MSBuild.exe"
  )
)
REM Fallbacks if vswhere didn't resolve it.
if not defined MSBUILD (
  for %%e in (Community Professional Enterprise BuildTools) do (
    if exist "%ProgramFiles%\Microsoft Visual Studio\2022\%%e\MSBuild\Current\Bin\MSBuild.exe" (
      set "MSBUILD=%ProgramFiles%\Microsoft Visual Studio\2022\%%e\MSBuild\Current\Bin\MSBuild.exe"
    )
  )
)
if not defined MSBUILD (
  echo [build] Could not locate MSBuild. Install VS2022 with the C++ workload.
  exit /b 1
)

echo [build] MSBuild: !MSBUILD!
"!MSBUILD!" "%~dp0VonCapture.vcxproj" -p:Configuration=Release -p:Platform=x64 -p:SolutionDir=%~dp0..\ -m -nologo
if errorlevel 1 (
  echo [build] FAILED
  exit /b 1
)
echo [build] OK -^> %~dp0..\x64\Release\Plugins\VonCapture.dll
endlocal
