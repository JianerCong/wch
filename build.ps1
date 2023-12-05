# 🦜 : In terms of finding packages.


# Use vcpkg to install rocksdb,boost and protobuf
vcpkg install rocksdb:x64-windows-static boost:x64-windows-static protobuf:x64-windows-static

# Build the project
cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE=C:\vcpkg\scripts\buildsystems\vcpkg.cmake

cmake --build build --config Release


