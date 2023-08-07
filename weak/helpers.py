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
