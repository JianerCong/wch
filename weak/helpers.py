import subprocess
from subprocess import Popen, PIPE
import pathlib
from pathlib import Path
import re
import time

import pytest

import requests #pip install requests
import json
import pathlib

this_dir = pathlib.Path(__file__).parent.resolve()  # where this file stays
wc =  this_dir / 'wch'

class S:
    HEADER = '\033[95m'
    BLUE = '\033[94m'
    CYAN = '\033[96m'
    GREEN = '\033[92m'
    MAG = '\033[93m'
    RED = '\033[91m'
    NOR = '\033[0m'
    BOLD = '\033[1m'
    UNDERLINE = '\033[4m'

def stop_and_test(p,s : str ='Primary'):
    # send a get node status?
    o1,e1 = p.communicate(b'\n')         # send a \n and wait for completion
    if p.returncode != 0:
        print(f'OUT FOR {s}--------------------------------------------------')
        print(o1)
        print(f'ERR FOR {s}--------------------------------------------------')
        print(e1)
    assert p.returncode == 0
