from helpers import *

port = 7777

def close_and_check(p):
    o,e = p.communicate(b'\n')         # send a \n and wait for completion
    print(f'OUT--------------------------------------------------')
    print(o)
    print(f'ERR--------------------------------------------------')
    print(e)
    assert p.returncode == 0

def test_serv_open_close(): #9
    # Does the program run ?
    # this wait for PIPEs  and port binding/unbinding
    global port
    p = Popen([wc, '--port', str(port)],stdin=PIPE,stdout=PIPE,stderr=PIPE)
    close_and_check(p)

# @pytest.mark.skip(​)
def test_serv_send_basic_node_status(): #19
    # Does the program run ?
    # this wait for PIPEs  and port binding/unbinding
    global port
    port += 1

    url = f'http://localhost:{port}/'

    p = Popen([wc, '--port', str(port)],stdin=PIPE,stdout=PIPE,stderr=PIPE)

    time.sleep(2)                   #  wait until is up
    result = requests.get(url + 'get_node_status')
    assert result.ok

    close_and_check(p)

# @pytest.mark.skip(​)
def test_serv_send_add_txs():   # 32
    global port
    port += 1
    # Does the program run ?
    # this wait for PIPEs  and port binding/unbinding

    url = f'http://localhost:{port}/'

    p = Popen([wc, '--port', str(port),'--mock-exe'],stdin=PIPE,stdout=PIPE,stderr=PIPE)

    time.sleep(2)                   #  wait until is up
    txs = [
        {'from' : '01',
         'to' : '02',
         'data' : 'ffff',
         'nonce' : 123
         },
        {'from' : '01',
         'to' : '',
         'data' : 'ffff',
         'nonce' : 123
         },
    ]
    result = requests.post(url + 'add_txs',json=txs)
    assert result.ok

    close_and_check(p)


def test_serv_two_nodes_add_txs():   # 44
    # Does the program run ?
    # this wait for PIPEs  and port binding/unbinding
    global port
    ports = [str(p) for p in [port + 1, port + 2]]
    port += 2

    url = [f'http://localhost:{p}/' for p in ports]

    p1 = Popen([wc, '--mock-exe','--port', ports[0]],stdin=PIPE,stdout=PIPE,stderr=PIPE)
    time.sleep(2)                   #  wait until is up

    p2 = Popen([wc, '--mock-exe','--port', ports[1], '--consensus',
                'Solo', '--Solo.node-to-connect','localhost:' + ports[0]],stdin=PIPE,stdout=PIPE,stderr=PIPE)
    time.sleep(2)                   #  wait until is up

    try:
        txs = [
            {'from' : '01',
             'to' : '02',
             'data' : 'ffff',
             'nonce' : 123
             },
            {'from' : '01',
             'to' : '',
             'data' : 'ffff',
             'nonce' : 123
             },
        ]
        result = requests.post(url[1] + 'add_txs',json=txs)
        assert result.ok
    finally:
        # send a get node status?
        stop_and_test(p1)
        stop_and_test(p2,'sub')

def test_serv_three_nodes_add_txs(): #45
    # Does the program run ?
    # this wait for PIPEs  and port binding/unbinding

    global port
    ports = [str(p) for p in [port + 1, port + 2,port+3]]
    port += 3

    url = [f'http://localhost:{p}/' for p in ports]

    # the primary
    p1 = Popen([wc,'--mock-exe', '--port', ports[0]],stdin=PIPE,stdout=PIPE,stderr=PIPE)
    time.sleep(2)                   #  wait until is up

    # the subs
    p2 = Popen([wc,'--mock-exe', '--port', ports[1],
                '--consensus', 'Solo', '--Solo.node-to-connect','localhost:' + ports[0]],stdin=PIPE,stdout=PIPE,stderr=PIPE)
    p3 = Popen([wc,'--mock-exe', '--port', ports[2],
                '--consensus', 'Solo', '--Solo.node-to-connect','localhost:' + ports[0]],stdin=PIPE,stdout=PIPE,stderr=PIPE)
    time.sleep(2)                   #  wait until is up

    try:
        txs = [
            {'from' : '01',
             'to' : '02',
             'data' : 'ffff',
             'nonce' : 123
             },
            {'from' : '01',
             'to' : '',
             'data' : 'ffff',
             'nonce' : 123
             },
        ]
        result = requests.post(url[1] + 'add_txs',json=txs)
        assert result.ok
    finally:
        stop_and_test(p1)
        stop_and_test(p2,'sub1')
        stop_and_test(p3,'sub2')

def stop_and_test(p,s : str ='Primary'):
    # send a get node status?
    o1,e1 = p.communicate(b'\n')         # send a \n and wait for completion
    print(f'OUT FOR {s}--------------------------------------------------')
    print(o1)
    print(f'ERR FOR {s}--------------------------------------------------')
    print(e1)
    assert p.returncode == 0
    # 🦜 : o1 should have 'Exec: t' ⇒ This means the node has executed
    # 'AddTx'. Note that this only exists for mocked exe.
    assert b'Exec: ' in o1

