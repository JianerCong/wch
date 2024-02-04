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
