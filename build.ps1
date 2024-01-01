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
cd .\boost_1_84_0\
.\bootstrap.bat
.\b2 --show-libraries
.\b2 --with-json --with-program_options --with-test --with-headers --with-log link=static

# The Boost C++ Libraries were successfully built!
# The following directory should be added to compiler include paths:
# C:\Users\congj\Music\wrack-chain\.pre\boost_1_84_0
# The following directory should be added to linker library paths:
# C:\Users\congj\Music\wrack-chain\.pre\boost_1_84_0\stage\lib

# 2. get and install other pkgs using vcpkg
# git submodule  add https://github.com/Microsoft/vcpkg.git
# fetch the vcpkg repo (1 level)
git submodule update --init --recursive --depth 1
cd vcpkg
# checkout 2023.12.12
git checkout 2023.12.12
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
# 🦜 : Kinda nightmare, we need to get the protoc and compile the .proto files
# before we do anything

# Create the ./.pre directory if it doesn't exist
if (!(Test-Path -Path ./.pre )) {
    New-Item -ItemType Directory -Force -Path ./.pre
}
# Download the protoc zip file
Invoke-WebRequest -Uri https://github.com/protocolbuffers/protobuf/releases/download/v25.1/protoc-25.1-win64.zip -OutFile ./.pre/protoc-25.1-win64.zip
# Unzip the protoc zip file
Expand-Archive -Path ./.pre/protoc-25.1-win64.zip -DestinationPath ./.pre/protoc-25.1-win64

# generate the protobuf files
$s1 = '.\weak\include'
$s2 = '.\weak\include\cnsss'
$DST_DIR=".\weak\include\.generated_pb"
md $DST_DIR
.\.pre\protoc-25.1-win64\bin\protoc.exe "-I=$s1" "-I=$s2" "--cpp_out=$DST_DIR" `
  "$s2/pure-rbft.proto" "$s2/pure-listenToOne.proto"
Remove-Item .\build -Recurse -Force

# 4. build the project
cmake -S weak -B build
cmake --build build --config Release


# --------------------------------------------------
cmake -S hi -B build
cmake --build build --config Release
