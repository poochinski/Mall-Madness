#!/bin/zsh
# Launches "Last Call at Larkspur Galleria" as a standalone game window.
cd "$(dirname "$0")"
ENGINE="/Users/Shared/Epic Games/UE_5.8/Engine"
if [ ! -f "Binaries/Mac/UnrealEditor.modules" ]; then
  echo "Building the MallMadness module (first run only)..."
  "$ENGINE/Build/BatchFiles/Mac/Build.sh" MallMadnessEditor Mac Development -Project="$PWD/MallMadness.uproject" -WaitMutex || exit 1
fi
exec "$ENGINE/Binaries/Mac/UnrealEditor.app/Contents/MacOS/UnrealEditor" "$PWD/MallMadness.uproject" -game -windowed -ResX=1600 -ResY=900 -nosplash "$@"
