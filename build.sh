# Here we document how to build wrack-chain
#

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

./b2 --with-json --with-program_options --with-test --with-headers --with-log link=static

echo "🐸 Boost installed"

# 2.Install rocksdb
cd ..
# 2.1 install the dependencies for rocksdb
sudo apt install libgflags-dev \
     libsnappy-dev zlib1g-dev libbz2-dev \
     liblz4-dev libzstd-dev libjemalloc-dev \
     liburing-dev -y

# 2.2 install rocksdb
wget https://github.com/facebook/rocksdb/archive/refs/tags/v8.3.2.tar.gz
tar zxf v8.3.2.tar.gz
cmake -S rocksdb-8.3.2/ -B build-rocksdb/ -DWITH_JEMALLOC=1 -DWITH_LIBURING=1 \
      -DWITH_SNAPPY=1 -DWITH_LZ4=1 -DWITH_ZLIB=1 -DWITH_ZSTD=1 -DCMAKE_BUILD_TYPE=Release

cmake --build build-rocksdb
cmake --install build-rocksdb --prefix installed-rocksdb

# if [ $? != 0 ]; then
#     echo "Error installing boost"
#     kill -USR1 "$pid"  # suicide
# else
# fi

