%SystemDrive%\cygwin\bin\which.exe bash
if errorlevel 1 set PATH=%SystemDrive%\cygwin\bin;%PATH%
cmd /c
if exist "%CONFIGURATIONBUILDDIR%\buildfailed" grep XX%PROJECTNAME%XX "%CONFIGURATIONBUILDDIR%\buildfailed"
if errorlevel 1 exit 1
echo XX%PROJECTNAME%XX > "%CONFIGURATIONBUILDDIR%\buildfailed"

mkdir 2>NUL "%CONFIGURATIONBUILDDIR%\include\JavaScriptCore"
xcopy /y /d "%WEBKIT_LIBRARIES%\include\JavaScriptCore\*" "%CONFIGURATIONBUILDDIR%\include\JavaScriptCore"

perl "%WEBKIT_LIBRARIES%\tools\scripts\auto-version.pl" "%INTDIR%"