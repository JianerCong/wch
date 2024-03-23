# 🦜 : In terms of finding packages.
# git config --list --show-origin
# git config --global http.proxy http://localhost:7890
# git config --global unset http.proxy

# --------------------------------------------------
# 1. get and install boost
# Create the ./.pre directory if it doesn't exist
if (!(Test-Path -Path ./.pre )) {
    New-Item -ItemType Directory -Force -Path ./.pre
}
# Download the boost zip file from https://sourceforge.net/projects/boost/files/boost/1.84.0/boost_1_84_0.7z/download
mv ~\Downloads\boost_1_84_0 .\.pre
cd .\.pre\boost_1_84_0
.\bootstrap.bat
.\b2 --show-libraries
# .\b2 --with-json --with-program_options --with-test --with-headers --with-log -a link=static cxxflags="/utf-8 /MT" 
.\b2 --with-json --with-program_options --with-test --with-headers --with-log -a link=static runtime-link=static cxxflags="/utf-8"
# 🦜 : Windows offers two way to link to MSVC runtime, either dynamically (/MD)
# or statically, (/Mt), here let's use /Mt, because that's what vcpkg used. But
# instead of using the cxxflags, you should use the `runtime-link=static` when
# calling `b2`, they are in almost equivalent, but the later does the current
# renaming (by adding the `-s` to the lib)

# build 
# -a : rebuild all

# The Boost C++ Libraries were successfully built!
# The following directory should be added to compiler include paths:
# C:\Users\congj\Music\wrack-chain\.pre\boost_1_84_0
# The following directory should be added to linker library paths:
# C:\Users\congj\Music\wrack-chain\.pre\boost_1_84_0\stage\lib

# 2. get and install other pkgs using vcpkg
# git submodule add --force https://github.com/Microsoft/vcpkg.git
# fetch the vcpkg repo (1 level)
git submodule init
git submodule update --init --recursive --depth 1
cd vcpkg
# checkout 2023.12.12
git checkout 2023.12.12
cd ..
.\vcpkg\bootstrap-vcpkg.bat
# list 
.\vcpkg\vcpkg.exe --help
.\vcpkg\vcpkg.exe list
# .\vcpkg\vcpkg.exe remove boost-test:x64-windows-static
# .\vcpkg\vcpkg.exe remove boost-json:x64-windows-static
# .\vcpkg\vcpkg.exe remove boost-program-options:x64-windows-static

.\vcpkg\vcpkg.exe install rocksdb:x64-windows-static protobuf:x64-windows-static
.\vcpkg\vcpkg.exe install openssl:x64-windows-static

# --------------------------------------------------
# 3. make the proto
# 🦜 : nope, we solved it using cmake magic..

# 4. build the project
rm -r build
cmake -S weak -B build-weak
cmake --build build-weak --config Release --verbose


# --------------------------------------------------
cmake -S hi -B build
cmake --build build --config Release
