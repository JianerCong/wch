from helpers import *

port = 7777

def stop_and_test(p,s : str ='Primary'):
    # send a get node status?
    o1,e1 = p.communicate('\n')         # send a \n and wait for completion
    print(f'OUT FOR {s}--------------------------------------------------')
    print(o1)
    print(f'ERR FOR {s}--------------------------------------------------')
    print(e1)
    assert p.returncode == 0
    return o1

def test_serv_open_close(): #9
    # Does the program run ?
    # this wait for PIPEs  and port binding/unbinding
    global port
    p = Popen([wc, '--port', str(port),'--light-exe','no'],stdin=PIPE,stdout=PIPE,stderr=PIPE,text=True)
    port += 1
    o1 = stop_and_test(p)
    assert '`normal exe`' in o1


def test_serv_send_add_txs():   # 32
    # Does the program run ?
    # this wait for PIPEs  and port binding/unbinding
    global port
    url = f'http://localhost:{port}/'
    p = Popen([wc, '--port', str(port),'--light-exe','no' ],stdin=PIPE,stdout=PIPE,stderr=PIPE,text=True)
    port += 1

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

    try:
        result = requests.post(url + 'add_txs',json=txs)
        assert result.ok
        # 🦜 : Let's check the result.
        #
        # 🐢 : The result should be a json array which contains the pair of hashes
        o = json.loads(result.content)
        print(f'Got result: {S.GREEN} {o} {S.NOR}')
        assert len(o) == 2  # the receipts of two txs

        assert 'hash' in o[0]  # tx1 is a CALL

        assert 'hash' in o[1]  # tx2 is a CREATE
        assert 'deployed_address' in o[1]
    finally:
        # send a get node status?
        stop_and_test(p)
        # o,e = p.communicate('\n')         # send a \n and wait for completion
        # print(f'OUT--------------------------------------------------')
        # print(o)
        # print(f'ERR--------------------------------------------------')
        # print(e)
        # assert p.returncode == 0

def test_serv_send_add_txs_and_check_pool():   # 32
    # Does the program run ?
    # this wait for PIPEs  and port binding/unbinding
    global port
    url = f'http://localhost:{port}/'
    p = Popen([wc, '--port', str(port), '--light-exe','no'],stdin=PIPE,stdout=PIPE,stderr=PIPE,text=True)
    port += 1

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
         'nonce' : 122
         },
    ]

    try:
        result = requests.post(url + 'add_txs',json=txs)
        assert result.ok
        time.sleep(1)                   #  wait until is up
        result = requests.get(url + 'get_pool_status')  # the pool status
        assert result.ok
        o = json.loads(result.content)
        print(f'Got result: {S.GREEN} {o} {S.NOR}')
        assert len(o['txs_in_pool']) == 2
        assert o['txs_in_pool_count'] == 2

        # 🦜 : Nope, you cannot do that, we used unordered map in there
        # assert o['txs_in_pool'][0]['nonce'] == 123
        # assert o['txs_in_pool'][1]['nonce'] == 122
    finally:
        # send a get node status?
        stop_and_test(p)
        # o,e = p.communicate('\n')         # send a \n and wait for completion
        # print(f'OUT--------------------------------------------------')
        # print(o)
        # print(f'ERR--------------------------------------------------')
        # print(e)
        # assert p.returncode == 0

def test_serv_two_nodes_add_txs():   # 44
    # Does the program run ?
    # this wait for PIPEs  and port binding/unbinding
    global port
    ports = [str(p) for p in [port + 1, port + 2]]
    port += 2

    url = [f'http://localhost:{p}/' for p in ports]

    p1 = Popen([wc, '--port', ports[0], '--light-exe','no'],stdin=PIPE,stdout=PIPE,stderr=PIPE,text=True)
    time.sleep(2)                   #  wait until is up

    p2 = Popen([wc, '--port', ports[1], '--consensus', 'Solo', '--Solo.node-to-connect','localhost:' + ports[0],
                '--light-exe','no'
                ],
               stdin=PIPE,stdout=PIPE,stderr=PIPE,text=True)
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
        try:
            o1 = stop_and_test(p1)
            # o1,e1 = p1.communicate('\n')         # send a \n and wait for completion
            # print(f'OUT FOR Primary--------------------------------------------------')
            # print(o1)
            # print(f'ERR FOR Primary--------------------------------------------------')
            # print(e1)
            # assert p1.returncode == 0
            # 🦜 : o1 should have 'Exec: t' ⇒ This means the node has executed 'AddTx'
            assert 'Adding parsed txs:' in o1
        finally:
            o2 = stop_and_test(p2,'sub')
            assert 'Adding parsed txs:' in o2
