mkdir build
cd build
cmake .. -DZLIB_LIBRARY=%ZLIB_LIBRARY% -DZLIB_INCLUDE_DIR=%ZLIB_INCLUDE_DIR% -G "Visual Studio 12"
