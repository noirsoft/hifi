echo off
echo Setting Environment Variables...
call "%VS120COMNTOOLS%\vsvars32.bat"
set HIFI_LIB_DIR="D:\Projects\hifi\external_libs"
echo Done!

echo Building GnuTLS...
cd %HIFI_LIB_DIR%\gnutls\bin
lib /def:libgnutls-28.def 
copy libgnutls-28.lib ..\lib
echo Done!