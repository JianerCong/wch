# Here we document how to build wrack-chain
#

# --------------------------------------------------
# 1. Install Boost>=1.75
rm .pre -rfv
mkdir .pre -v
cd .pre
wget https://boostorg.jfrog.io/artifactory/main/release/1.82.0/source/boost_1_82_0.tar.bz2
echo "⚙️ Extracting boost"
tar -xf boost_1_82_0.tar.bz2 --verbose
echo "🐸 Boost extracted"
cd boost_1_82_0
./bootstrap.sh
sudo apt install libpython-all-dev
sudo apt install libpython3-all-dev
./b2 --show-libraries
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

# --------------------------------------------------
# 2.Install rocksdb [in .pre/ folder]
# 2.1 install the dependencies for rocksdb
sudo apt install libgflags-dev \
     libsnappy-dev zlib1g-dev libbz2-dev \
     liblz4-dev libzstd-dev libjemalloc-dev \
     liburing-dev -y

# 2.2 install rocksdb
wget https://github.com/facebook/rocksdb/archive/refs/tags/v8.3.2.tar.gz
tar zxf v8.3.2.tar.gz
# cmake -S rocksdb-8.3.2/ -B build-rocksdb/ -DWITH_JEMALLOC=1 -DWITH_LIBURING=1 \
#       -DWITH_SNAPPY=1 -DWITH_LZ4=1 -DWITH_ZLIB=1 -DWITH_ZSTD=1 -DCMAKE_BUILD_TYPE=Release

# try to build with lz4
sudo apt install libgflags-dev liblz4-dev liburing-dev -y
# cmake -S rocksdb-8.3.2/ -B build-rocksdb/ -DWITH_LZ4=1 -DCMAKE_BUILD_TYPE=Release
cmake --build build-rocksdb
cmake --install build-rocksdb --prefix installed-rocksdb


# --------------------------------------------------
# 3. add protobuf [in .pre/ folder]

# sudo apt install libabsl-dev -y # 🦜 : Nope, pd now is smart enough to take
# care of itself's dependencies

git clone -b v25.1 https://github.com/protocolbuffers/protobuf.git
cd protobuf
git submodule update --init --recursive
cd ..
cmake -S protobuf -B build-pb -DABSL_PROPAGATE_CXX_STD=ON
cmake --build build-pb
# 🦜 : It looks like you need to install the third party libraries that protobuf depends on.
cmake --install build-pb --prefix installed-pb
# ln -s ~/repo/installed-pb/ .pre/installed-pb

# --------------------------------------------------
# Finally we build the chain
cmake -S weak -B build-weak
cmake --build build-weak/
