# 🦜 : In terms of finding packages.
# git config --list --show-origin
# git config --global http.proxy http://localhost:7890
# git config --global unset http.proxy
# git submodule  add https://github.com/Microsoft/vcpkg.git 


# fetch the vcpkg repo
git submodule update --init --recursive
# bootstrap vcpkg
.\vcpkg\bootstrap-vcpkg.bat

# Use vcpkg to install rocksdb,boost and protobuf
.\vcpkg\vcpkg.exe install rocksdb:x64-windows-static boost:x64-windows-static protobuf:x64-windows-static
.\vcpkg\vcpkg.exe install boost:x64-windows-static
# needs find_package(Boost 1.75...1.99 COMPONENTS json unit_test_framework log program_options)
.\vcpkg\vcpkg.exe install boost-test:x64-windows-static boost-log:x64-windows-static boost-program-options:x64-windows-static boost-json:x64-windows-static

Remove-Item .\build -Recurse -Force
cmake --build build --config Release
