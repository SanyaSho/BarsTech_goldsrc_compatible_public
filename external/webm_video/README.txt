# Dependencies for HL25 WebM video support

## libwebm
- cmake -S . -B build_win32_vs2019 -T v142 -A Win32 -DENABLE_WEBMTS=OFF -DENABLE_WEBMINFO=OFF -DENABLE_SAMPLE_PROGRAMS=OFF
- cmake --build build_win32_vs2019 --config Release
- cmake --build build_win32_vs2019 --config Debug

## libogg
- cmake -S . -B build_win32_vs2019 -T v142 -A Win32
- cmake --build build_win32_vs2019 --config Release
- cmake --build build_win32_vs2019 --config Debug

## libvorbis (Release)
- cmake -S . -B build_win32_vs2019 -T v142 -A Win32 -DINSTALL_CMAKE_PACKAGE_MODULE=OFF -DCMAKE_POLICY_VERSION_MINIMUM=3.5 -DOGG_ROOT=$PWD/../libogg -DOGG_LIBRARY=$PWD/../libogg/build_win32_vs2019/Release/ -DOGG_INCLUDE=$PWD/../libogg
- cmake --build build_win32_vs2019 --config Release

## libvorbis (Release)
- cmake -S . -B build_win32_vs2019 -T v142 -A Win32 -DINSTALL_CMAKE_PACKAGE_MODULE=OFF -DCMAKE_POLICY_VERSION_MINIMUM=3.5 -DOGG_ROOT=$PWD/../libogg -DOGG_LIBRARY=$PWD/../libogg/build_win32_vs2019/Debug/ -DOGG_INCLUDE=$PWD/../libogg
- cmake --build build_win32_vs2019 --config Release

## libvpx
- Install (MSYS2)[https://www.msys2.org/]
- Open `MSYS2 MSYS` window
- Run `export PATH=$PATH:/c/Program\\ Files/Microsoft\\ Visual\\ Studio/2022/Community/MSBuild/Current/Bin/`
- Run `pacman -S base-devel mingw-w64-x86_64-toolchain yasm git`
- ../configure --disable-examples --disable-tools --disable-docs --target=x86-win32-vs17
- make