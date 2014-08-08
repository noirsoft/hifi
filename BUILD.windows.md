Dependencies
===
1. Download Dependencies 
I put the raw files in a parallel directory (hifi-dependencies) to keep them in case things go wrong.
This list in in alphabetical order to ease checking that all have been downloaded. This is _not_ the installation order!

* [cmake](http://www.cmake.org/cmake/resources/software.html) ~> 2.8.12.2
* [freeglut MSVC](http://www.transmissionzero.co.uk/software/freeglut-devel/) ~> 2.8.1
* [GLEW](http://glew.sourceforge.net/) ~> 1.10.0
* [glm](http://glm.g-truc.net/0.9.5/index.html) ~> 0.9.5.2
* [GnuTLS](http://gnutls.org/download.html) ~> 3.2.12
  * IMPORTANT: GnuTLS 3.2.12 is critical to avoid a security vulnerability.
* [Qt](http://qt-project.org/downloads) ~> 5.2.0
  * Unfortunately, the geniuses at qt only provide a 64-bit version for openGL on VS2012, and we need a 32-bit version, so we have to download qt-windows-opensource-5.2.0-msvc2010_opengl-x86-offline.exe
* [qxmpp](https://github.com/qxmpp-project/qxmpp/) ~> 0.7.6
* [zLib](http://www.zlib.net/) ~> 1.2.8

 
Install dependencies in this order:
===
CMake
* Run installer, add CMake to system path in installer options
 
QT
* run installer, default options okay.
* allow to run QT Creator to ensure that DLLs are instaleld correctly.

Add C:\Qt\Qt5.2.0\5.2.0\msvc2010_opengl\bin to system PATH environment variable (installer does not do this)

set QT_CMAKE_PREFIX_PATH to C:\Qt\Qt5.2.0\5.2.0\msvc2010_opengl

Copying Dependencies to HIFI_LIB_DIR
===
Make a directory under hifi, it can be called anything, such as "dependencies" or "external".  Set environment variable "HIFI_LIB_DIR" to point to it. Then extract all the other dependencies to it, which should result in a structure as follows:

    HIFI_LIB_DIR
        -> freeglut
            -> bin
            -> include
            -> lib
        -> glew
            -> bin
            -> include
            -> lib
        -> glm
            -> glm
                -> glm.hpp
        -> gnutls
            -> bin
            -> include
            -> lib
        -> zlib
           -> include
           -> lib
           -> test

Except! qxmpp should be extracted to HIFI_LIB_DIR\build ! the top-level directory of the qxmpp zip archive is names qxmpp-master, so you should have HIFI_LIB_DIR/build/qxmpp-master. THis is 

You need to ensure that all the following entries are added to your PATH environment variable:
%HIFI_LIB_DIR%\zlib
---- existing %PATH% -----
C:\Program Files (x86)\CMake 2.8\bin
C:\Qt\Qt5.2.0\5.2.0\msvc2010_opengl\bin
%HIFI_LIB_DIR%\freeglut\bin
%HIFI_LIB_DIR%\glew\bin\Release\Win32
%HIFI_LIB_DIR%\gnutls\bin
%HIFI_LIB_DIR%\qxmpp\lib

Note that the zlib entry goes before your existing PATH, then the rest can go after.

== building QXMPP
open a command-prompt in your main hifi directory, and run Make-qxmpp.bat  If you have any problems, cry.

== building gnutls
open a command prompt in your main hifi directory (could be the same one as above) and run Make-GnuTLS.bat

== generating the solution
open a command prompt to your main hifi direcotry (yup, can be same one) then run Make-solution.bat

open up hifi\build\hifi.sln and see if you can build!

#####Windows SDK 7.1

Whichever version of Visual Studio you use, first install [Microsoft Windows SDK for Windows 7 and .NET Framework 4](http://www.microsoft.com/en-us/download/details.aspx?id=8279).

#####Visual Studio C++ 2010 Express

#### OpenSSL

QT will use OpenSSL if it's available, but it doesn't install it, so you must install it separately.

Your system may already have several versions of the OpenSSL DLL's (ssleay32.dll, libeay32.dll) lying around, but they may be the wrong version. If these DLL's are in the PATH then QT will try to use them, and if they're the wrong version then you will see the following errors in the console:

    QSslSocket: cannot resolve TLSv1_1_client_method
    QSslSocket: cannot resolve TLSv1_2_client_method
    QSslSocket: cannot resolve TLSv1_1_server_method
    QSslSocket: cannot resolve TLSv1_2_server_method
    QSslSocket: cannot resolve SSL_select_next_proto
    QSslSocket: cannot resolve SSL_CTX_set_next_proto_select_cb
    QSslSocket: cannot resolve SSL_get0_next_proto_negotiated

To prevent these problems, install OpenSSL yourself. Download the following binary packages [from this website](http://slproweb.com/products/Win32OpenSSL.html):
* Visual C++ 2008 Redistributables
* Win32 OpenSSL v1.0.1h

Install OpenSSL into the Windows system directory, to make sure that QT uses the version that you've just installed, and not some other version.


#### Build High Fidelity using Visual Studio
Follow the same build steps from the CMake section, but pass a different generator to CMake.

    cmake .. -DZLIB_LIBRARY=%ZLIB_LIBRARY% -DZLIB_INCLUDE_DIR=%ZLIB_INCLUDE_DIR% -G "Visual Studio 10"

If you're using Visual Studio 2013 then pass "Visual Studio 12" instead of "Visual Studio 10" (yes, 12, not 13).

Open %HIFI_DIR%\build\hifi.sln and compile.

####Running Interface
If you need to debug Interface, you can run interface from within Visual Studio (see the section below). You can also run Interface by launching it from command line or File Explorer from %HIFI_DIR%\build\interface\Debug\interface.exe

####Debugging Interface
* In the Solution Explorer, right click interface and click Set as StartUp Project
* Set the "Working Directory" for the Interface debugging sessions to the Debug output directory so that your application can load resources. Do this: right click interface and click Properties, choose Debugging from Configuration Properties, set Working Directory to .\Debug
* Now you can run and debug interface through Visual Studio
