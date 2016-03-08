mkdir 2>NUL "%CONFIGURATIONBUILDDIR%\include\WebKit2"

set ResourcesDirectory=%CONFIGURATIONBUILDDIR%\bin%PlatformArchitecture%\WebKit.resources

echo Copying resources...
mkdir "%ResourcesDirectory%" 2>NUL
xcopy /y /d "%PROJECTDIR%\..\..\win\WebKit.resources\Info.plist" "%ResourcesDirectory%" >NUL

if exist "%WEBKIT_LIBRARIES%\tools\VersionStamper\VersionStamper.exe" "%WEBKIT_LIBRARIES%\tools\VersionStamper\VersionStamper.exe" --verbose "%TARGETPATH%"

if exist "%CONFIGURATIONBUILDDIR%\buildfailed" del "%CONFIGURATIONBUILDDIR%\buildfailed"
