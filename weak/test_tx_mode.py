from helpers import *
from pathlib import Path

import subprocess

def do_sign(f_sk, txs):
    f = Path('tx-pre.json')
    f0 = Path('tx-post.json')
    f.write_text(json.dumps(txs))

    # 0.3 sign the tx
    c = subprocess.run([wc, 'toolbox', 'tx-sign-no-crt', str(f), str(f_sk), str(f0)], capture_output=True, text=True)
    assert c.returncode == 0
    print('\t\t🦜: tx-sign-no-crt output:\n\n' + c.stdout)
    return json.loads(f0.read_text())

def new_keypair():
    # 0. prepare the contract
    # 0.1 generate key pair
    f_pk = Path('acn-pk.pem')
    f_sk = Path('acn-sk.pem')
    c = subprocess.run([wc, 'toolbox', 'new-keypair', str(f_pk), str(f_sk)], capture_output=True, text=True)
    assert c.returncode == 0
    print('\t\t🦜: new-keypair output:\n\n' + c.stdout)
    return f_pk, f_sk

def test_toolbox():
    f_pk, f_sk = new_keypair()

    # 0.2 prepare the tx
    txs = [{'type' : 'data', 'to' : '', 'data' : 'hi', 'nonce': 123}]
    do_sign(f_sk, txs)

    # clean up
    for f in [f_pk, f_sk]:
        if f.exists():
            f.unlink()


def test_real_run_public_mode():
    # 0.1 generate key pair
    f_pk, f_sk = new_keypair()

    # 0.2 prepare the tx
    txs = [{'type' : 'data', 'to' : '', 'data' : 'hi', 'nonce': 123}]
    txs1 = do_sign(f_sk, txs)

    # 1. start the server
    port = 7777
    print('⚙️ starting the server')
    p = Popen([wc, '--port', str(port),'--light-exe', '--tx-mode-serious', 'public'],stdin=PIPE,stdout=PIPE,stderr=PIPE, text=True)
    time.sleep(2)                   #  wait until is up
    url = f'http://localhost:{port}/'

    try:
        # 2. send the tx
        r = requests.post(url + 'add_txs', json=txs1)
        assert r.ok

        o = json.loads(r.content)
        print(f'Got result of invoke: {S.GREEN} {o} {S.NOR}')

        # 3. if we modify the tx, it should fail (i.e. we need to sign it again)
        txs1[0]['nonce'] = 124
        r = requests.post(url + 'add_txs', json=txs1)
        assert r.ok             # 🦜 : it's still passed, but no receipt for it
        o = json.loads(r.content)  # an empty list
        print(f'Got result of invoke: {S.GREEN} {o} {S.NOR}')
        assert len(o) == 0
    finally:
        print(f'🚮️ cleaning up')
        # clean the files
        stop_and_test(p)
        
