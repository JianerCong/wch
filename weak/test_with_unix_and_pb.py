from helpers import *
from hi_pb2 import Txs

import requests_unixsocket
session = requests_unixsocket.Session()
port = 7777

def close_and_check(p):
    o,e = p.communicate('\n')         # send a \n and wait for completion
    if p.returncode != 0
        print(f'OUT--------------------------------------------------')
        print(o)
        print(f'ERR--------------------------------------------------')
        print(e)
    assert p.returncode == 0

def test_serv_open_close(): #9
    # Does the program run ?
    # this wait for PIPEs  and port binding/unbinding
    global port
    p = Popen([wc, '--port', str(port), '--mock-exe','--unix-socket', '/tmp/hi-weak.sock'],
              stdin=PIPE,stdout=PIPE,stderr=PIPE,text=True)
    close_and_check(p)

def test_serv_send_basic_node_status(): #19
    # Does the program run ?
    # this wait for PIPEs  and port binding/unbinding
    global port
    port += 1

    url = f'http+unix://%2Ftmp%2Fhi-weak.sock/'
    p = Popen([wc, '--port', str(port), '--mock-exe','--unix-socket', '/tmp/hi-weak.sock'],
              stdin=PIPE,stdout=PIPE,stderr=PIPE,text=True)

    time.sleep(2)                   #  wait until is up

    result = session.get(url + 'get_node_status')
    assert result.ok

    close_and_check(p)

