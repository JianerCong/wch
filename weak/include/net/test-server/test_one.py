# 🦜 : These tests can fail for some subtle reasons, run pytest repeatedly.
# There should be a chance that all tests pass. (I tried.)
"""
(myenv)  [] ~/Templates/lrn/cpp
cmake --build build-hi/
[ 66%] Built target myexe
[100%] Runing pytest 🐸
==================================================================== test session starts =====================================================================
platform linux -- Python 3.10.6, pytest-7.1.2, pluggy-1.0.0 -- /home/me/work/lcode/myenv/bin/python3
cachedir: .pytest_cache
rootdir: /home/me/Templates/lrn/cpp/build-hi
collected 4 items

test_one.py::test_serv_open_close PASSED                                                                                                               [ 25%]
test_one.py::test_serv_log PASSED                                                                                                                      [ 50%]
test_one.py::test_serv_basic_request PASSED                                                                                                            [ 75%]
test_one.py::test_serv_get PASSED                                                                                                                      [100%]

===================================================================== 4 passed in 8.07s ======================================================================
[100%] Built target run
(myenv)  [] ~/Templates/lrn/cpp

"""



from subprocess import Popen, PIPE
import pathlib
from pathlib import Path
import re

import time
def test_serv_open_close():
    time.sleep(2)                   #  2sec
    # this wait for PIPEs  and port binding/unbinding
    p = Popen(['./myexe'],stdin=PIPE,stdout=PIPE,stderr=PIPE,text=True)
    o,e = p.communicate('\n')         # send a \n and wait for completion
    print(f'OUT--------------------------------------------------')
    print(o)
    print(f'ERR--------------------------------------------------')
    print(e)
    assert p.returncode == 0

def test_serv_log():
    time.sleep(2)                   #  1sec
    f = 'hi.log'
    # Remove file if exists
    Path(f).unlink(missing_ok=True)

    port = '7778'
    p = Popen(['./myexe',port],stdin=PIPE,stdout=PIPE,stderr=PIPE,text=True)
    o,e = p.communicate('\n')         # send a \n and wait for completion
    print(f'OUT--------------------------------------------------')
    print(o)
    print(f'ERR--------------------------------------------------')
    print(e)

    assert p.returncode == 0
    assert Path(f).exists()
    # now can be read

    i = open(f).read()                 # default to read
    assert f'start listening on port {port}' in i


import requests #pip install requests
import json


def test_serv_basic_404():
    time.sleep(2)                   #  1sec
    f = 'hi.log'
    # Remove file if exists
    Path(f).unlink(missing_ok=True)

    p = Popen(['./myexe','7779'],stdin=PIPE,stdout=PIPE,stderr=PIPE,text=True)
    url = 'http://localhost:7779/'
    # send request --------------------------------------------------

    result = requests.get(url + 'trash')
    assert not result.ok
    assert result.status_code == 404

    # stop server--------------------------------------------------
    o,e = p.communicate('\n')         # send a \n and wait for completion
    print(f'OUT--------------------------------------------------')
    print(o)
    print(f'ERR--------------------------------------------------')
    print(e)

    assert p.returncode == 0

def test_serv_basic_request():
    time.sleep(2)                   #  1sec
    f = 'hi.log'
    # Remove file if exists
    Path(f).unlink(missing_ok=True)

    p = Popen(['./myexe','7780'],stdin=PIPE,stdout=PIPE,stderr=PIPE,text=True)

    # send request --------------------------------------------------

    a={'url' : '/ccc'}
    url = 'http://localhost:7780/'
    result = requests.post(url + 'addHandler',json=a)
    assert result.ok
    assert result.content == b'/ccc is added to the url too'

    # stop server--------------------------------------------------
    o,e = p.communicate('\n')         # send a \n and wait for completion
    print(f'OUT--------------------------------------------------')
    print(o)
    print(f'ERR--------------------------------------------------')
    print(e)

    assert p.returncode == 0

def test_serv_dynamically_added_handler():
    time.sleep(2)                   #  1sec
    f = 'hi.log'
    # Remove file if exists
    Path(f).unlink(missing_ok=True)

    p = Popen(['./myexe','7781'],stdin=PIPE,stdout=PIPE,stderr=PIPE,text=True)

    url = 'http://localhost:7781/'
    # send request --------------------------------------------------

    a={'url' : '/ccc'}
    result = requests.post(url + 'addHandler',json=a)
    assert result.ok
    assert result.content == b'/ccc is added to the url too'

    result = requests.post(url + 'ccc',json=a)
    assert result.ok

    # stop server--------------------------------------------------
    o,e = p.communicate('\n')         # send a \n and wait for completion
    print(f'OUT--------------------------------------------------')
    print(o)
    print(f'ERR--------------------------------------------------')
    print(e)

    assert p.returncode == 0

def test_serv_dynamically_remove_nonexisting_handler():
    time.sleep(2)                   #  1sec
    f = 'hi.log'
    # Remove file if exists
    Path(f).unlink(missing_ok=True)

    p = Popen(['./myexe','7782'],stdin=PIPE,stdout=PIPE,stderr=PIPE,text=True)
    url = 'http://localhost:7782/'

    # send request --------------------------------------------------

    a={'url' : '/ccc'}
    result = requests.post(url + 'removeHandler',json=a)
    assert result.ok
    assert result.content == b'/ccc doesn\'t exist in the url'

    # stop server--------------------------------------------------
    o,e = p.communicate('\n')         # send a \n and wait for completion
    print(f'OUT--------------------------------------------------')
    print(o)
    print(f'ERR--------------------------------------------------')
    print(e)

    assert p.returncode == 0

def test_serv_dynamically_remove_existing_handler():

    time.sleep(2)                   #  1sec
    f = 'hi.log'
    # Remove file if exists
    Path(f).unlink(missing_ok=True)
    p = Popen(['./myexe','7783'],stdin=PIPE,stdout=PIPE,stderr=PIPE,text=True)
    url = 'http://localhost:7783/'
    # send request --------------------------------------------------
    a={'url' : '/ccc'}
    result = requests.post(url + 'addHandler',json=a)
    assert result.ok
    assert result.content == b'/ccc is added to the url too'
    result = requests.post(url + 'removeHandler',json=a)
    assert result.ok
    assert result.content == b'/ccc is removed from the url'

    # stop server--------------------------------------------------
    o,e = p.communicate('\n')         # send a \n and wait for completion
    print(f'OUT--------------------------------------------------')
    print(o)
    print(f'ERR--------------------------------------------------')
    print(e)

    assert p.returncode == 0
