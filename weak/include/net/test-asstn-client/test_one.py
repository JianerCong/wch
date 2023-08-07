import subprocess
from subprocess import Popen, PIPE
import pathlib
from pathlib import Path
import re
import sys

import time
def test_serv_open_close():
    # this wait for PIPEs  and port binding/unbinding
    # Start the server --------------------------------------------------
    p = Popen([sys.executable, 'm.py'],stdin=PIPE,stdout=PIPE,stderr=PIPE)
    print('Start listening')
    assert p.returncode == None  # the server is up
    time.sleep(2)                   #  2sec

    # make the request --------------------------------------------------
    c = subprocess.run(['./myexe','--log_level=all'],capture_output=True, text=True)
    # c is the CompletedProcess class
    assert c.returncode == 0
    # assert 'No errors detected' in c.stderr
    # assert c.stdout == 'hi\n'
    print(f'stdout for request: {c.stdout}')

    # p.terminate()
    # close the server (send ctrl-c)
    p.kill()
