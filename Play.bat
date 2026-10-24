@echo off
rem Launches "Last Call at Larkspur Galleria" as a standalone game window (Windows).
rem Edit UE_ROOT if Unreal Engine 5.8 is installed somewhere else.
set UE_ROOT=C:\Program Files\Epic Games\UE_5.8
cd /d "%~dp0"
if not exist "Binaries\Win64\UnrealEditor.modules" (
  echo Building the MallMadness module ^(first run only^)...
  call "%UE_ROOT%\Engine\Build\BatchFiles\Build.bat" MallMadnessEditor Win64 Development -Project="%CD%\MallMadness.uproject" -WaitMutex || exit /b 1
)
start "" "%UE_ROOT%\Engine\Binaries\Win64\UnrealEditor.exe" "%CD%\MallMadness.uproject" -game -windowed -ResX=1600 -ResY=900 -nosplash %*
