@echo off
echo == Update binaries x86/x64 ==
echo.

call "C:\Program Files (x86)\Microsoft Visual Studio 11.0\VC\vcvarsall.bat" x86

echo == Generating projects ==
pushd ..\utils\flt2dds\
call build_vs2012.bat
popd

pushd ..\utils\flt2elev\
call build_vs2012.bat
popd 

pushd ..\utils\flt2xml\
call build_vs2012.bat
popd

pushd ..\utils\fltextent\
call build_vs2012.bat
popd

pushd ..\utils\fltfind\
call build_vs2012.bat
popd

pushd ..\utils\fltheader\
call build_vs2012.bat
popd

echo == Building latest binaries ==
msbuild /t:Rebuild /p:Configuration=Release;Platform=Win32 ..\utils\flt2dds\build\flt2dds.sln
msbuild /t:Rebuild /p:Configuration=Release;Platform=x64 ..\utils\flt2dds\build\flt2dds.sln

msbuild /t:Rebuild /p:Configuration=Release;Platform=Win32 ..\utils\flt2elev\buildvs2012\flt2elev.sln
msbuild /t:Rebuild /p:Configuration=Release;Platform=x64 ..\utils\flt2elev\buildvs2012\flt2elev.sln

msbuild /t:Rebuild /p:Configuration=Release;Platform=Win32 ..\utils\flt2xml\buildvs2012\flt2xml.sln
msbuild /t:Rebuild /p:Configuration=Release;Platform=x64 ..\utils\flt2xml\buildvs2012\flt2xml.sln

msbuild /t:Rebuild /p:Configuration=Release;Platform=Win32 ..\utils\fltextent\buildvs2012\fltextent.sln
msbuild /t:Rebuild /p:Configuration=Release;Platform=x64 ..\utils\fltextent\buildvs2012\fltextent.sln

msbuild /t:Rebuild /p:Configuration=Release;Platform=Win32 ..\utils\fltfind\buildvs2012\fltfind.sln
msbuild /t:Rebuild /p:Configuration=Release;Platform=x64 ..\utils\fltfind\buildvs2012\fltfind.sln

msbuild /t:Rebuild /p:Configuration=Release;Platform=Win32 ..\utils\fltheader\buildvs2012\fltheader.sln
msbuild /t:Rebuild /p:Configuration=Release;Platform=x64 ..\utils\fltheader\buildvs2012\fltheader.sln


echo == Copying binaries ==
xcopy ..\utils\flt2dds\build\bin\x32\release\*.exe x86\ /Y
xcopy ..\utils\flt2dds\build\bin\x64\release\*.exe x64\ /Y

xcopy ..\utils\flt2elev\buildvs2012\bin\x32\release\*.exe x86\ /Y
xcopy ..\utils\flt2elev\buildvs2012\bin\x64\release\*.exe x64\ /Y

xcopy ..\utils\flt2xml\buildvs2012\bin\x32\release\*.exe x86\ /Y
xcopy ..\utils\flt2xml\buildvs2012\bin\x64\release\*.exe x64\ /Y

xcopy ..\utils\fltextent\buildvs2012\bin\x32\release\*.exe x86\ /Y
xcopy ..\utils\fltextent\buildvs2012\bin\x64\release\*.exe x64\ /Y

xcopy ..\utils\fltfind\buildvs2012\bin\x32\release\*.exe x86\ /Y
xcopy ..\utils\fltfind\buildvs2012\bin\x64\release\*.exe x64\ /Y

xcopy ..\utils\fltheader\buildvs2012\bin\x32\release\*.exe x86\ /Y
xcopy ..\utils\fltheader\buildvs2012\bin\x64\release\*.exe x64\ /Y 

xcopy ..\utils\fltview\buildvs2012\bin\x32\release\*.exe x86\ /Y
xcopy ..\utils\fltview\buildvs2012\bin\x64\release\*.exe x64\ /Y

echo == Removing temp directories ==
rmdir /S /Q ..\utils\flt2dds\build
rmdir /S /Q ..\utils\flt2elev\buildvs2012
rmdir /S /Q ..\utils\flt2xml\buildvs2012
rmdir /S /Q ..\utils\fltextent\buildvs2012
rmdir /S /Q ..\utils\fltfind\buildvs2012
rmdir /S /Q ..\utils\fltheader\buildvs2012

echo.