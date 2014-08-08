echo off
echo Setting Environment Variables...
call "%VS120COMNTOOLS%\vsvars32.bat"

echo Creating Directories...

cd %HIFI_LIB_DIR%\build\qxmpp-master

echo Building qxmpp...

qmake PREFIX=%HIFI_LIB_DIR%\qxmpp
nmake
nmake install
cd ..\..\..\

echo Done!