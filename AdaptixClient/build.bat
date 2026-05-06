@echo off
set "PATH=C:\msys64\mingw64\bin;%%PATH%%"
echo [1/4] Build Release...
cmake -S . -B cmake-build-release -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build cmake-build-release --config Release

echo [2/4] Create dist...
if exist dist rmdir /s /q dist
mkdir dist
move cmake-build-release\AdaptixClient.exe dist\

echo [3/4] windeployqt...
cd dist
windeployqt.exe AdaptixClient.exe

echo [4/4] Add DLL...
copy "C:\msys64\mingw64\bin\libwinpthread-1.dll".
copy "C:\msys64\mingw64\bin\libgcc_s_seh-1.dll".
copy "C:\msys64\mingw64\bin\libstdc++-6.dll".
copy "C:\msys64\mingw64\bin\libwinpthread-1.dll" .
copy "C:\msys64\mingw64\bin\libgcc_s_seh-1.dll" .
copy "C:\msys64\mingw64\bin\libstdc++-6.dll" .
copy "C:\msys64\mingw64\bin\libfreetype-6.dll" .
copy "C:\msys64\mingw64\bin\libharfbuzz-0.dll" .
copy "C:\msys64\mingw64\bin\libmd4c.dll" .
copy "C:\msys64\mingw64\bin\libpng16-16.dll" .
copy "C:\msys64\mingw64\bin\zlib1.dll" .
copy "C:\msys64\mingw64\bin\libb2-1.dll" .
copy "C:\msys64\mingw64\bin\libdouble-conversion.dll" .
copy "C:\msys64\mingw64\bin\libicuin78.dll" .
copy "C:\msys64\mingw64\bin\libicuuc78.dll" .
copy "C:\msys64\mingw64\bin\libpcre2-16-0.dll" .
copy "C:\msys64\mingw64\bin\libbrotlidec.dll" .
copy "C:\msys64\mingw64\bin\libzstd.dll" .
copy "C:\msys64\mingw64\bin\libbz2-1.dll" .
copy "C:\msys64\mingw64\bin\libglib-2.0-0.dll" .
copy "C:\msys64\mingw64\bin\libgraphite2.dll" .
copy "C:\msys64\mingw64\bin\libbrotlicommon.dll" .
copy "C:\msys64\mingw64\bin\libicudt78.dll" .
copy "C:\msys64\mingw64\bin\libpcre2-8-0.dll" .
copy "C:\msys64\mingw64\bin\libintl-8.dll" .
copy "C:\msys64\mingw64\bin\libiconv-2.dll" .

echo [OK] Complete!
pause


