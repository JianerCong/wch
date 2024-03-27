from helpers import *
from pathlib import Path
def test_single_primary_set_get_123():
    port = 7777
    p = Popen([wc, '--port', str(port),'--light-exe'],stdin=PIPE,stdout=PIPE,stderr=PIPE)
    time.sleep(2)                   #  wait until is up

    url = f'http://localhost:{port}/'

    # 0. prepare the py-contract
    f = Path('hi_contract.py')
    f0 = Path('invoke.json')
    s = '''
from typing import Any
def hi(_storage: dict[str, Any], y : int) -> int:
    _storage["x"] = _storage.get("x", 1) + y
    return _storage["x"]
    '''
    try:
        with f.open('w') as w:
            w.write(s)

        # 1. set the contract
        txs = [{
            'from' : '01',
            'to' : '',
            'data' : '@' + str(f),
            'nonce' : 123,
            'type' : 'python'
        }]
        r = requests.post(url + 'add_txs', json=txs)
        assert r.ok
        # get the deployed addr
        o = json.loads(r.content)
        print(f'Got result of deployment: {S.GREEN} {o} {S.NOR}')

        deployed_addr = o[0]['deployed_address']
        assert len(deployed_addr) == 40

        # 2. call the contract
        # 2.1 prepare the incoke json
        s = {'method' : 'hi', 'args' : {'y': 10}}
        with f0.open('w') as w:
            w.write(json.dumps(s))

        # 2.2 call the contract
        txs = [{
            'from' : '01',
            'to' : deployed_addr,
            'data' : '@' + str(f0),
            'nonce' : 124,
            'type' : 'python'
        }]
        r = requests.post(url + 'add_txs', json=txs)
        assert r.ok

        o = json.loads(r.content)
        print(f'Got result of invoke: {S.GREEN} {o} {S.NOR}')

        # get the result from txReceipt
        h = o[0]['hash']
        r = requests.get(url + 'get_receipt?hash=' + h)
        assert r.ok
        o = json.loads(r.content)
        print(f'Got result of get_receipt: {S.GREEN} {o} {S.NOR}')
        assert o['result'] == 11
    finally:
        print(f'🚮️ cleaning up')
        # clean the files
        if f.exists():
            f.unlink()
        if f0.exists():
            f0.unlink()
        stop_and_test(p)
