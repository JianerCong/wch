# Here we document how to build wrack-chain
Clean=true

# --------------------------------------------------
# 0.
if $Clean; then
    rm -rfv .pre
    rm -rfv build-weak
    mkdir .pre
fi
cd .pre

#
# --------------------------------------------------
# 1. Install Boost>=1.75
# wget https://boostorg.jfrog.io/artifactory/main/release/1.84.0/source/boost_1_84_0.tar.bz2
if [ ! -f boost_1_84_0.tar.bz2 ]; then
    wget https://boostorg.jfrog.io/artifactory/main/release/1.84.0/source/boost_1_84_0.tar.bz2
fi

if [ ! -d boost_1_84_0 ]; then
    tar -xf boost_1_84_0.tar.bz2 --verbose
    echo "⚙️ Extracting boost"
    tar -xf boost_1_84_0.tar.bz2 --verbose
    echo "🐸 Boost extracted, start building"

    cd boost_1_84_0
    ./bootstrap.sh
    # sudo apt install libpython3-all-dev
    # ./b2 --show-libraries
    # - atomic - chrono - container - context - contract - coroutine - date_time
    # - exception - fiber
    # - filesystem - graph - graph_parallel - headers - iostreams - json - locale
    # - log - math - mpi - nowide - program_options - python
    # - random - regex - serialization - stacktrace - system - test
    # - thread - timer - type_erasure - url - wave

    # build the static libraries (for deployment)
    ./b2 --with-json --with-program_options --with-test --with-headers --with-log link=static
    # build the shared libraries (for development)
    ./b2 --with-json --with-program_options --with-test --with-headers --with-log link=shared
    echo "🐸 Boost installed"
    cd ..
fi

# --------------------------------------------------
# 2.Install rocksdb [in .pre/ folder]
# 2.1 install the dependencies for rocksdb

# 2.2 install rocksdb
if [ ! -f v9.0.0.tar.gz ]; then
    wget https://github.com/facebook/rocksdb/archive/refs/tags/v9.0.0.tar.gz
fi

# tar zxf v9.0.0.tar.gz
if [ ! -d rocksdb-9.0.0 ]; then
    tar zxf v9.0.0.tar.gz
    sudo apt install libgflags-dev liblz4-dev liburing-dev -y
    # try to build with lz4 ? 🦜
    cmake -S rocksdb-9.0.0/ -B build-rocksdb/ -DWITH_LZ4=1 -DCMAKE_BUILD_TYPE=Release
fi

cmake --build build-rocksdb
cmake --install build-rocksdb --prefix installed-rocksdb

# --------------------------------------------------
# 3. add protobuf [in .pre/ folder]

# sudo apt install libabsl-dev -y # 🦜 : Nope, pd now is smart enough to take
# care of itself's dependencies

git clone --depth 1 -b v26.1 https://github.com/protocolbuffers/protobuf.git
# 🦜 : --depth 1 only fetches the latest commit
cd protobuf
git switch -c b1
git submodule update --init --recursive

cd ..
cmake -S protobuf -B build-pb -DABSL_PROPAGATE_CXX_STD=ON
cmake --build build-pb
# 🦜 : It looks like you need to install the third party libraries that protobuf depends on.
cmake --install build-pb --prefix installed-pb
# ln -s ~/repo/installed-pb/ .pre/installed-pb

# --------------------------------------------------
# Finally we build the chain
cd ..
cmake -S weak -B build-weak -DUSE_DYNAMIC_LIBS=1 # for development
cmake -S weak -B build-weak -DUSE_DYNAMIC_LIBS=0 # for distribution
cmake --build build-weak/
v=1.0
cp build-weak/wch ~/wch-$v-linux-x86_64 -v

# try
./build-weak/wch toolbox -h
./build-weak/wch toolbox new-keypair out-pk.pem out-sk.pem

