import subprocess
def test_run():
    c = subprocess.run(['./myexe'])
    # c is the CompletedProcess class
    assert c.returncode == 0

def test_basic_output():
    c = subprocess.run(['./myexe'],capture_output=True, text=True)
    assert c.returncode == 0
    l = c.stdout.splitlines()   # split('\n') and remove the trailing \n
    assert l == [
        'config_file : ',
        'consensus_name : Solo',
        'port : 7777',
        'data_dir : ',
        'Solo_node_to_connect : ',
    ]
import pathlib
from pathlib import Path

def test_basic_config_file():
    f = 'm.conf'
    Path(f).unlink(missing_ok=True)
    o = open(f,'w')

    o.writelines([
        'port=1234'
    ])
    o.flush()

    c = subprocess.run(['./myexe','--config',f],capture_output=True, text=True)
    assert c.returncode == 0
    l = c.stdout.splitlines()   # split('\n') and remove the trailing \n
    assert l == [
        'config_file : m.conf',
        'consensus_name : Solo',
        'port : 1234',
        'data_dir : ',
        'Solo_node_to_connect : ',
    ]

def test_config_file_more_options():
    f = 'm.conf'
    Path(f).unlink(missing_ok=True)
    o = open(f,'w')

    o.writelines(
        [l + '\n' for l in
         [
             'port=1234',
             'consensus=aaa',
             'data-dir=abc'
         ]
         ]
    )
    # 🦜: writelines() doesn't add \n
    o.flush()

    c = subprocess.run(['./myexe','--config',f],capture_output=True, text=True)
    assert c.returncode == 0
    l = c.stdout.splitlines()   # split('\n') and remove the trailing \n

    assert l == [
        'config_file : m.conf',
        'consensus_name : aaa',
        'port : 1234',
        'data_dir : abc',
        'Solo_node_to_connect : ',
    ]


def test_config_file_whose_boss():
    f = 'm.conf'
    Path(f).unlink(missing_ok=True)
    o = open(f,'w')

    o.writelines(
        [l + '\n' for l in
         [
             'port=1111',
             'data-dir=abc'
         ]
         ]
    )
    # 🦜: writelines() doesn't add \n
    o.flush()

    # 🦜 : it looks like cmdline overwrites config file
    # 🐢 : Make sence.
    c = subprocess.run(['./myexe','--config',f,'--port','2222'],capture_output=True, text=True)
    assert c.returncode == 0
    l = c.stdout.splitlines()   # split('\n') and remove the trailing \n

    assert l == [
        'config_file : m.conf',
        'consensus_name : Solo',
        'port : 2222',
        'data_dir : abc',
        'Solo_node_to_connect : ',
    ]
