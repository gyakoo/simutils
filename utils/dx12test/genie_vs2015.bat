@echo off
echo == Building for VS2015 ==
echo.
set GENIECMD=genie.exe

where %GENIECMD% > NUL 2>&1
if %ERRORLEVEL% NEQ 0 (
  echo '%GENIECMD%' command does not found.
  echo Make sure you have it in your PATH environment variable or in the current directory.
  echo Download it from: https://github.com/bkaradzic/genie
  goto end
)
premake5 --file=premake5.lua vs2015 


:end

xcopy ..\..\extern\vld\bin\Win32\*.* buildvs2015\bin\x32\debug\ /Y
xcopy ..\..\extern\vld\bin\Win64\*.* buildvs2015\bin\x64\debug\ /Y
echo.
pause