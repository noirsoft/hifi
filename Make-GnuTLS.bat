echo off
echo Setting Environment Variables...
call "%VS120COMNTOOLS%\vsvars32.bat"

echo Building GnuTLS...
cd %HIFI_LIB_DIR%\gnutls\bin
lib /def:libgnutls-28.def 
copy libgnutls-28.lib ..\lib
cd ..\..\..

echo Done!