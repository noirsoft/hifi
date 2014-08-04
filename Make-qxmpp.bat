echo off
echo Setting Environment Variables...
call "%VS120COMNTOOLS%\vsvars32.bat"
set HIFI_LIB_DIR="D:\Projects\hifi\external_libs"
set PATH=%PATH%;%HIFI_LIB_DIR%\Qt\5.2.0\msvc2010_opengl\bin

mkdir %HIFI_LIB_DIR%\build
cd %HIFI_LIB_DIR%\build\qxmpp-0.7.6
qmake PREFIX=%HIFI_LIB_DIR%\qxmpp
nmake
nmake install