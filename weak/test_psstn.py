from helpers import *

port = 7777

# 🦜 : Here we try to start a chain. add a Tx, restart it to check whether the state is persisted
import pytest

import shutil
@pytest.fixture()
def tmp_dirs():
    # make vars
    t = this_dir / 'my_tmp_folder'
    dirs = [t /'d1', t/'d2']

    # make dirs
    print(f'⚙️ Preparing folders for DBs')
    t.mkdir()
    for p in dirs:
        print(f'Making folder {S.CYAN} {p} {S.NOR}')
        p.mkdir()

    yield [str(p.absolute()) for p in dirs]
    # remove the root
    print(f'🚮️ Tearing down and removing folder:{S.CYAN} {str(t.absolute())} {S.NOR}')
    shutil.rmtree(str(t.absolute()))


def get_latest_Blk_and_check_receipt(o, url_to_ask
                                     ,expected_blk_number):
    # 🦜 :
    """
    o: the json-loaded object such that o[0]['hash'] is a valid hash to get receipt
    """
    txh = o[0]['hash']

    # 🦜 the Blk should have been increased from non-to 0
    result = requests.get(url_to_ask + 'get_latest_Blk')
    assert result.ok
    b = json.loads(result.content)  # the latest Blk
    print(f'Got result: {S.GREEN} {b} {S.NOR}')

    assert b[0]['number'] == expected_blk_number  # blk number

    # 🦜 : get the Tx-receipt
    result = requests.get(url_to_ask + f'get_receipt?hash={txh}')
    assert result.ok
    txr = json.loads(result.content)  # the receipt
    assert txr['ok']                  # deployed successfully

    return txr

def test_one(tmp_dirs):
    # start the server

    global port
    ports = [port + 1]
    port += 1

    p = Popen([wc, '--port', str(port),'--light-exe', '--data-dir', tmp_dirs[0],'--verbose','no'],stdin=PIPE,stdout=PIPE,stderr=PIPE,text=True)
    # p = Popen([wc, '--port', str(port),'--light-exe', '--data-dir', tmp_dirs[0]],stdin=PIPE,stdout=PIPE,stderr=PIPE)
    # [2024-02-04] 🦜 : After we switched to protobuf serialization, the
    # [debug] log is not valid utf8 string any more, so using `text=True` will cause exception...
    url = [f'http://localhost:{p}/' for p in ports]

    # send one
    try:
        print(f'Send one Tx')
        txs = [
            {'from' : '01',
             'to' : '',
             'data' : '',
             'nonce' : 123
             }
        ]
        time.sleep(2)                   #  wait until is up
        result = requests.post(url[0] + 'add_txs',json=txs)
        assert result.ok
        # 🦜 : It looks like we can also deploy empty contract


        # 🐢 : The result should be a json array which contains the pair of hashes
        o = json.loads(result.content)
        print(f'Got result: {S.GREEN} {o} {S.NOR}')
        assert len(o) == 1  # the receipts of one txs
        deployed_addr = o[0]['deployed_address']
        assert len(deployed_addr) == 40
        get_latest_Blk_and_check_receipt(o,url_to_ask=url[0],expected_blk_number=0)
    finally:
        # close the server
        stop_and_test(p)


    time.sleep(1)                   #  wait until is up
    # 🐢 : server closed. and two DBs should have been created
    p_chain = Path(tmp_dirs[0]) / 'chainDB'
    assert p_chain.exists()
    p_state = Path(tmp_dirs[0]) / 'stateDB'
    assert p_state.exists()

    # 🐢 : Now let's start it again. Remember to update the port.

    ports = [port + 1]
    port += 1
    p = Popen([wc, '--port', str(port),'--light-exe', '--data-dir', tmp_dirs[0],'--verbose','no'],stdin=PIPE,stdout=PIPE,stderr=PIPE,text=True)
    url = [f'http://localhost:{p}/' for p in ports]

    # 🐢 the latest Blk should still be there.
    try:
        print(f'Send another Tx')
        txs = [
            {'from' : '01',
             'to' : '',
             'data' : '',
             'nonce' : 124
             }
        ]
        time.sleep(2)                   #  wait until is up
        result = requests.post(url[0] + 'add_txs',json=txs)
        assert result.ok

        o = json.loads(result.content)
        # print(f'Got result: {S.GREEN} {o} {S.NOR}')
        # assert len(o) == 1  # the receipts of one txs
        # deployed_addr = o[0]['deployed_address']
        # assert len(deployed_addr) == 40

        # 🐢 the chain should continue
        get_latest_Blk_and_check_receipt(o,url_to_ask=url[0],expected_blk_number=1)
    finally:
        # close the server
        stop_and_test(p)

def stop_and_test(p,s : str ='Primary'):
    # send a get node status?
    o1,e1 = p.communicate('\n')         # send a \n and wait for completion
    if p.returncode != 0:
        print(f'OUT FOR {s}--------------------------------------------------')
        print(o1)
        print(f'ERR FOR {s}--------------------------------------------------')
        print(e1)
    assert p.returncode == 0
    return o1


def test_two(tmp_dirs):
    time.sleep(1)                   #  wait until is up
    # start the server
    global port
    ports = [str(p) for p in [port + 1, port + 2]]
    port += 2

    url = [f'http://localhost:{p}/' for p in ports]
    # Does the program run ?
    # this wait for PIPEs  and port binding/unbinding
    p1 = Popen([wc, '--light-exe','--port', ports[0], '--data-dir', tmp_dirs[0],'--verbose','no'],
               stdin=PIPE,stdout=PIPE,stderr=PIPE, text=True)
    time.sleep(2)                   #  wait until is up

    p2 = Popen([wc, '--light-exe','--port', ports[1], '--consensus',
                'Solo', '--Solo.node-to-connect','localhost:' + ports[0],
               '--data-dir', tmp_dirs[1],'--verbose','no'
                ],
               stdin=PIPE,stdout=PIPE,stderr=PIPE, text=True)
    time.sleep(2)                   #  wait until is up


    # 🦜 send one to N1, get from N2
    try:
        print('Send one Tx')
        txs = [
            {'from' : '01',
             'to' : '',
             'data' : '',
             'nonce' : 123
             }
        ]
        time.sleep(2)                   #  wait until is up
        result = requests.post(url[0] + 'add_txs',json=txs)
        assert result.ok
        # 🦜 : It looks like we can also deploy empty contract


        # 🐢 : The result should be a json array which contains the pair of hashes
        o = json.loads(result.content)
        print(f'Got result: {S.GREEN} {o} {S.NOR}')
        assert len(o) == 1  # the receipts of one txs
        deployed_addr = o[0]['deployed_address']
        assert len(deployed_addr) == 40
        get_latest_Blk_and_check_receipt(o,url_to_ask=url[1],expected_blk_number=0)
    finally:
        # close the server
        print('Closing the two')
        stop_and_test(p2)
        stop_and_test(p1)


    time.sleep(1)                   #  wait until is up
    # 🐢 : server closed. and two DBs should have been created for each nodes
    p_chain = Path(tmp_dirs[0]) / 'chainDB'
    assert p_chain.exists()
    p_state = Path(tmp_dirs[0]) / 'stateDB'
    assert p_state.exists()
    p_chain = Path(tmp_dirs[1]) / 'chainDB'
    assert p_chain.exists()
    p_state = Path(tmp_dirs[1]) / 'stateDB'
    assert p_state.exists()

    # 🐢  : Remember to update the port
    ports = [str(p) for p in [port + 1, port + 2]]
    url = [f'http://localhost:{p}/' for p in ports]
    port += 2
    # 🐢 : Now let's start them again.
    p1 = Popen([wc, '--light-exe','--port', ports[0], '--data-dir', tmp_dirs[0],'--verbose','no'],
               stdin=PIPE,stdout=PIPE,stderr=PIPE,text=True)
    time.sleep(2)                   #  wait until is up

    p2 = Popen([wc, '--light-exe','--port', ports[1], '--consensus',
                'Solo', '--Solo.node-to-connect','localhost:' + ports[0],
               '--data-dir', tmp_dirs[1],'--verbose','no'],
               stdin=PIPE,stdout=PIPE,stderr=PIPE,text=True)

    time.sleep(2)                   #  wait until is up

    # 🐢 the latest Blk should still be there.
    try:
        print(f'Send another Tx to N1')
        txs = [
            {'from' : '01',
             'to' : '',
             'data' : '',
             'nonce' : 124
             }
        ]
        time.sleep(2)                   #  wait until is up
        result = requests.post(url[1] + 'add_txs',json=txs)
        assert result.ok

        o = json.loads(result.content)
        # print(f'Got result: {S.GREEN} {o} {S.NOR}')
        # assert len(o) == 1  # the receipts of one txs
        # deployed_addr = o[0]['deployed_address']
        # assert len(deployed_addr) == 40

        # 🐢 the chain should continue
        get_latest_Blk_and_check_receipt(o,url_to_ask=url[0],expected_blk_number=1)
    finally:
        # close the server
        stop_and_test(p1)
        stop_and_test(p2)
