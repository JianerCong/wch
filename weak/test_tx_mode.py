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
    o = json.loads(f0.read_text())
    # clean up
    f.unlink()
    f0.unlink()
    return o

def new_keypair(s_pk = 'acn-pk.pem', s_sk = 'acn-sk.pem'):
    # 0. prepare the contract
    # 0.1 generate key pair
    f_pk = Path(s_pk)
    f_sk = Path(s_sk)
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


def test_real_run_public_tx_mode():
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
        for f in [f_pk, f_sk]:
            if f.exists():
                f.unlink()
        stop_and_test(p)


def test_real_run_ca_tx_mode():
    # 0.1 generate ca key pair
    f_ca_pk, f_ca_sk = new_keypair('ca-pk.pem', 'ca-sk.pem')
    # 0.2 prepare user key pair
    f_pk, f_sk = new_keypair('user-pk.pem', 'user-sk.pem')

    # 0.3 sign the pk
    f_crt = Path('user-crt.pem')
    c = subprocess.run([wc, 'toolbox', 'sign-pk', str(f_ca_sk), str(f_pk), str(f_crt)], capture_output=True, text=True)
    assert c.returncode == 0
    print('\t\t🦜: sign-pk output:\n\n' + c.stdout)

    # 0.4 prepare the tx
    f = Path('tx-pre.json')
    f0 = Path('tx-post.json')
    txs = [{'type' : 'data', 'to' : '', 'data' : 'hi', 'nonce': 123}]
    f.write_text(json.dumps(txs))

    # 0.5 sign the tx
    c = subprocess.run([wc, 'toolbox', 'tx-sign', str(f), str(f_sk), str(f_crt), str(f0)], capture_output=True, text=True)
    assert c.returncode == 0
    print('\t\t🦜: tx-sign output:\n\n' + c.stdout)
    txs1 = json.loads(f0.read_text())

    # 1. start the server
    port = 7778
    print('⚙️ starting the server')
    p = Popen([wc, '--port', str(port),'--light-exe', '--tx-mode-serious', 'ca@' + str(f_ca_pk)],stdin=PIPE,stdout=PIPE,stderr=PIPE, text=True)
    time.sleep(2)                   #  wait until is up
    url = f'http://localhost:{port}/'

    try:
        # 2. send the tx
        r = requests.post(url + 'add_txs', json=txs1)
        assert r.ok

        o = json.loads(r.content)
        print(f'Got result of invoke: {S.GREEN} {o} {S.NOR}')

        # 3. if we modify the tx, it should fail (i.e. we need to sign it again)
        txs1[0]['pk_crt'] = 'bbad'  # should be valid hex
        r = requests.post(url + 'add_txs', json=txs1)
        assert r.ok             # 🦜 : it's still passed, but no receipt for it
        o = json.loads(r.content)  # an empty list
        print(f'Got result of invoke: {S.GREEN} {o} {S.NOR}')
        assert len(o) == 0
    finally:
        print(f'🚮️ cleaning up')
        # clean the files
        for f in [f_ca_pk, f_ca_sk, f_pk, f_sk, f_crt]:
            if f.exists():
                f.unlink()
        stop_and_test(p)
