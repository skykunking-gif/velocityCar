#!/bin/sh
# Packages a Mac build (Shipping) into Packaged/Mac. Run this on a Mac:
# Unreal can only build for Mac on a Mac with Xcode installed.
# Requires UE 5.8 installed via the Epic Games Launcher.
set -e
UE_ROOT="${UE_ROOT:-/Users/Shared/Epic Games/UE_5.8}"
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT="$SCRIPT_DIR/../MyProject.uproject"
OUTDIR="$SCRIPT_DIR/../Packaged/Mac"

"$UE_ROOT/Engine/Build/BatchFiles/RunUAT.sh" BuildCookRun \
  -project="$PROJECT" -noP4 -utf8output \
  -platform=Mac -clientconfig=Shipping \
  -build -cook -map=/Game/Untitled -stage -pak -iostore -compressed -nodebuginfo \
  -archive -archivedirectory="$OUTDIR"
