@echo off
echo == Building for VS2012 ==
echo.
set PREMAKECMD=premake5.exe

where %PREMAKECMD% > NUL 2>&1
if %ERRORLEVEL% NEQ 0 (
  echo '%PREMAKECMD%' command does not found.
  echo Make sure you have it in your PATH environment variable or in the current directory.
  echo Download it from: https://premake.github.io/
  goto end
)
premake5 vs2012


:end
echo.
pause