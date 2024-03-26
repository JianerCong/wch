from helpers import *


port = 7777

def stop_and_test(p,s : str ='Primary'):
    # send a get node status?
    o1,e1 = p.communicate(b'\n')         # send a \n and wait for completion
    if p.returncode != 0:
        print(f'OUT FOR {s}--------------------------------------------------')
        print(o1)
        print(f'ERR FOR {s}--------------------------------------------------')
        print(e1)
    assert p.returncode == 0

def test_serv_open_close(): #9
    # Does the program run ?
    # this wait for PIPEs  and port binding/unbinding
    global port
    p = Popen([wc, '--port', str(port),'--light-exe'],stdin=PIPE,stdout=PIPE,stderr=PIPE)
    port += 1
    stop_and_test(p)

def test_single_primary_set_get_123(): #9

    global port
    ports = [port + 1]
    port += 1

    # Does the program run ?
    # this wait for PIPEs  and port binding/unbinding
    # p = Popen([wc, '--port', str(port),'--light-exe'],stdin=PIPE,stdout=PIPE,stderr=PIPE)
    p = Popen([wc, '--port', str(port),'--light-exe'],stdin=PIPE,stdout=PIPE,stderr=PIPE)

    url = [f'http://localhost:{p}/' for p in ports]
    try:
    # 1. deploy the contract --------------------------------------------------
        txs = [
            {'from' : '01',
             'to' : '',
             'data' : '608060405234801561001057600080fd5b50610150806100206000396000f3fe608060405234801561001057600080fd5b50600436106100365760003560e01c806360fe47b11461003b5780636d4ce63c14610057575b600080fd5b610055600480360381019061005091906100c3565b610075565b005b61005f61007f565b60405161006c91906100ff565b60405180910390f35b8060008190555050565b60008054905090565b600080fd5b6000819050919050565b6100a08161008d565b81146100ab57600080fd5b50565b6000813590506100bd81610097565b92915050565b6000602082840312156100d9576100d8610088565b5b60006100e7848285016100ae565b91505092915050565b6100f98161008d565b82525050565b600060208201905061011460008301846100f0565b9291505056fea2646970667358221220271e30d641d99bedebb5450b18efe8b67269cf688a15386162d4c2ff7072a8af64736f6c63430008130033',
             'nonce' : 123
             }
        ]
        time.sleep(2)                   #  wait until is up
        result = requests.post(url[0] + 'add_txs',json=txs)
        assert result.ok

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
            print(f'Got Block result: {S.GREEN} {result.content} {S.NOR}')
            b = json.loads(result.content)  # the latest Blk

            assert b[0]['number'] == expected_blk_number  # blk number

            # 🦜 : get the Tx-receipt
            result = requests.get(url_to_ask + f'get_receipt?hash={txh}')
            assert result.ok
            txr = json.loads(result.content)  # the receipt
            assert txr['ok']                  # deployed successfully

            return txr

        # 🐢 : The result should be a json array which contains the pair of hashes
        o = json.loads(result.content)
        print(f'Got result: {S.GREEN} {o} {S.NOR}')
        assert len(o) == 1  # the receipts of one txs
        deployed_addr = o[0]['deployed_address']
        assert len(deployed_addr) == 40
        get_latest_Blk_and_check_receipt(o,url_to_ask=url[0],expected_blk_number=0)

    # 2. set(123) --------------------------------------------------
        txs = [
            {'from' : '01',
             'to' : deployed_addr,
             'data' : '60fe47b1000000000000000000000000000000000000000000000000000000000000007b',
             'nonce' : 124
             }
        ]
        result = requests.post(url[0] + 'add_txs',json=txs)
        assert result.ok

        # 🐢 : The result should be a json array which contains the pair of hashes
        print(f'📗️ Got result: {S.GREEN} {o} {S.NOR}')
        o = json.loads(result.content)
        print(f'Got result: {S.GREEN} {o} {S.NOR}')
        assert len(o) == 1  # the receipts of two txs
        get_latest_Blk_and_check_receipt(o,url_to_ask=url[0],expected_blk_number=1)
    # 3. get(123)
        txs = [
            {'from' : '01',
             'to' : deployed_addr,
             'data' : '6d4ce63c',
             'nonce' : 125
             }
        ]
        result = requests.post(url[0] + 'add_txs',json=txs)
        assert result.ok

        # 🐢 : The result should be a json array which contains the pair of hashes
        o = json.loads(result.content)
        print(f'Got result: {S.GREEN} {o} {S.NOR}')
        assert len(o) == 1  # the receipts of two txs
        txr = get_latest_Blk_and_check_receipt(o,url_to_ask=url[0],expected_blk_number=2)

        # 🦜 : See what is got, should have got 123 (=0x7b)
        assert txr['result'] == '000000000000000000000000000000000000000000000000000000000000007b'

    finally:
        # close the server
        stop_and_test(p)



def test_two_nodes_set_get_123(): #9

    global port
    ports = [str(p) for p in [port + 1, port + 2]]
    port += 2
    url = [f'http://localhost:{p}/' for p in ports]
    # Does the program run ?
    # this wait for PIPEs  and port binding/unbinding
    p1 = Popen([wc, '--light-exe','--port', ports[0]],stdin=PIPE,stdout=PIPE,stderr=PIPE)
    time.sleep(2)                   #  wait until is up

    p2 = Popen([wc, '--light-exe','--port', ports[1], '--consensus',
                'Solo', '--Solo.node-to-connect','localhost:' + ports[0]],stdin=PIPE,stdout=PIPE,stderr=PIPE)
    time.sleep(2)                   #  wait until is up

    try:
    # 1. deploy the contract to N0 --------------------------------------------------
        txs = [
            {'from' : '01',
             'to' : '',
             'data' : '608060405234801561001057600080fd5b50610150806100206000396000f3fe608060405234801561001057600080fd5b50600436106100365760003560e01c806360fe47b11461003b5780636d4ce63c14610057575b600080fd5b610055600480360381019061005091906100c3565b610075565b005b61005f61007f565b60405161006c91906100ff565b60405180910390f35b8060008190555050565b60008054905090565b600080fd5b6000819050919050565b6100a08161008d565b81146100ab57600080fd5b50565b6000813590506100bd81610097565b92915050565b6000602082840312156100d9576100d8610088565b5b60006100e7848285016100ae565b91505092915050565b6100f98161008d565b82525050565b600060208201905061011460008301846100f0565b9291505056fea2646970667358221220271e30d641d99bedebb5450b18efe8b67269cf688a15386162d4c2ff7072a8af64736f6c63430008130033',
             'nonce' : 123
             }
        ]
        time.sleep(2)                   #  wait until is up
        result = requests.post(url[0] + 'add_txs',json=txs)
        assert result.ok

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
            print(f'📗️ Got block result: {S.GREEN} {result.content} {S.NOR}')
            b = json.loads(result.content)  # the latest Blk
            print(f'Got result: {S.GREEN} {b} {S.NOR}')

            assert b[0]['number'] == expected_blk_number  # blk number

            # 🦜 : get the Tx-receipt
            result = requests.get(url_to_ask + f'get_receipt?hash={txh}')
            assert result.ok
            txr = json.loads(result.content)  # the receipt
            assert txr['ok']                  # deployed successfully

            return txr

        # 🐢 : The result should be a json array which contains the pair of hashes
        o = json.loads(result.content)
        print(f'Got result: {S.GREEN} {o} {S.NOR}')
        assert len(o) == 1  # the return values of one txs
        deployed_addr = o[0]['deployed_address']
        assert len(deployed_addr) == 40
        # 🦜 : Check the state on N1(We sent to N0)
        get_latest_Blk_and_check_receipt(o,url_to_ask=url[1],expected_blk_number=0)

    # 2. set(123) --------------------------------------------------
        txs = [
            {'from' : '01',
             'to' : deployed_addr,
             'data' : '60fe47b1000000000000000000000000000000000000000000000000000000000000007b',
             'nonce' : 124
             }
        ]
        # 🦜 Send to N1
        result = requests.post(url[1] + 'add_txs',json=txs)
        assert result.ok

        # 🐢 : The result should be a json array which contains the pair of hashes
        o = json.loads(result.content)
        print(f'Got result: {S.GREEN} {o} {S.NOR}')
        assert len(o) == 1  # the receipts of two txs
        # 🦜 : We sent to N1 and this time, we check the state of N0
        get_latest_Blk_and_check_receipt(o,url_to_ask=url[0],expected_blk_number=1)
    # 3. get(123)
        # 🦜 : Here we send to N1 and check with N1
        txs = [
            {'from' : '01',
             'to' : deployed_addr,
             'data' : '6d4ce63c',
             'nonce' : 125
             }
        ]
        result = requests.post(url[1] + 'add_txs',json=txs)
        assert result.ok

        # 🐢 : The result should be a json array which contains the pair of hashes
        o = json.loads(result.content)
        print(f'Got result: {S.GREEN} {o} {S.NOR}')
        assert len(o) == 1  # the receipts of two txs
        txr = get_latest_Blk_and_check_receipt(o,url_to_ask=url[1],expected_blk_number=2)

        # 🦜 : See what is got, should have got 123 (=0x7b)
        assert txr['result'] == '000000000000000000000000000000000000000000000000000000000000007b'

    finally:
        # close the server
        stop_and_test(p1)
        stop_and_test(p2)

