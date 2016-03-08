@echo off
set WEBKIT_LIBRARIES=%CD%\..\..\..\WebKitLibraries\win
set WEBKIT_OUTPUTDIR=%CD%\..\..\..\WebKitBuild
set PATH=%WEBKIT_LIBRARIES%\bin32;%PATH%
start "%PROGRAMFILES%\Microsoft Visual Studio 12.0\Common7\IDE\devenv.exe" WebKit.sln
