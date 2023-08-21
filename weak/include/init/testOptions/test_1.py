import subprocess
def test_run():
    c = subprocess.run(['./myexe'],check=True)
    # c is the CompletedProcess class
    assert c.returncode == 0
    # 🦜 : Setting check=True will let it throw error is it got non-zero returncode.

def test_basic_output():
    c = subprocess.run(['./myexe'],capture_output=True, text=True, check=True)
    l = c.stdout.splitlines()   # split('\n') and remove the trailing \n
    assert l == [
        'config_file : ',
        'consensus_name : Solo',
        'port : 7777',
        'data_dir : ',
        'Solo_node_to_connect : ',
    ]


def test_give_some_param():
    c = subprocess.run(['./myexe','--port','7778','--consensus','Rbft',
                        '--Bft.node-list','a1','a2','a3'
                        ],capture_output=True, text=True, check=True)
    l = c.stdout.splitlines()   # split('\n') and remove the trailing \n
    assert l == [
        'config_file : ',
        'consensus_name : Rbft',
        'port : 7778',
        'data_dir : ',
        'Solo_node_to_connect : ',
        'Bft_node_list : a1,a2,a3'
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

    c = subprocess.run(['./myexe','--config',f],capture_output=True, text=True, check=True)
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

    c = subprocess.run(['./myexe','--config',f],capture_output=True, text=True, check=True)
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
    c = subprocess.run(['./myexe','--config',f,'--port','2222'],capture_output=True, text=True, check=True)
    l = c.stdout.splitlines()   # split('\n') and remove the trailing \n

    assert l == [
        'config_file : m.conf',
        'consensus_name : Solo',
        'port : 2222',
        'data_dir : abc',
        'Solo_node_to_connect : ',
    ]

def test_config_file_section():
    f = 'm.conf'
    Path(f).unlink(missing_ok=True)
    o = open(f,'w')

    o.writelines(
        [l + '\n' for l in
         [
             'port=1111',
             'data-dir=abc',
             '[Bft]',
             'node-list=a1 a2 a3'
         ]
         ]
    )
    # 🦜: writelines() doesn't add \n
    o.flush()

    # 🦜 : it looks like cmdline overwrites config file
    # 🐢 : Make sence.
    c = subprocess.run(['./myexe','--config',f,'--port','2222'],capture_output=True, text=True, check=True)

    l = c.stdout.splitlines()   # split('\n') and remove the trailing \n

    assert l == [
        'config_file : m.conf',
        'consensus_name : Solo',
        'port : 2222',
        'data_dir : abc',
        'Solo_node_to_connect : ',
        'Bft_node_list : a1,a2,a3'
    ]
