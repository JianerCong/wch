# 🐢: Here are some pytest-powered tests. Some of our components (those relating
# to network) need some helps from the outside in order to do unit-tests. For
# example, client module needs a mocked server. Server module needs a mocked
# client. We use python as our helper.

# This script is supposed to be run in the folder where `weak` exists, which is
# the grandparent dir of this file.

# Following commands will assume that you have `python` in the environment. To
# get prepared for the dependent pkgs:

# pip install requests web.py pytest

# --------------------------------------------------

# 1. Test `pure-httpNetAsstn.hpp`. This starts a web.py server that will respond to
# the incomming requests.
cmake -S weak/include/net/test-asstn-client -B build-test-aclient
cmake --build build-test-aclient

# --------------------------------------------------

# 2. Test `pure-weakHttpClient.hpp`. This starts a web.py server that will
# respond to the incomming requests.
cmake -S weak/include/net/test-client -B build-test-client
cmake --build build-test-client


# --------------------------------------------------

# 3. Test `pure-weakHttpServer.hpp`. This uses python's `requests` module to
# send requests.

cmake -S weak/include/net/test-server -B build-test-net
cmake --build build-test-net
