@echo off
rem Packages a Windows build (Shipping) into Packaged\Windows.
rem Only /Game/Untitled is cooked; Content holds many demo maps that the game doesn't use.
rem -nocompileeditor: cook with the existing editor binaries so this works while the editor is open (Live Coding blocks editor builds).
setlocal
set UE_ROOT=E:\Program Files\UE_5.8
set PROJECT=%~dp0..\MyProject.uproject
set OUTDIR=%~dp0..\Packaged\Windows

call "%UE_ROOT%\Engine\Build\BatchFiles\RunUAT.bat" BuildCookRun ^
  -project="%PROJECT%" -noP4 -utf8output ^
  -platform=Win64 -clientconfig=Shipping ^
  -build -nocompileeditor -cook -map=/Game/Untitled -stage -pak -iostore -compressed -prereqs -nodebuginfo ^
  -archive -archivedirectory="%OUTDIR%"
exit /b %ERRORLEVEL%
