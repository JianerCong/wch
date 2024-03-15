# remove everything between "\x1b[33m" (S_YELLOW) and "\x1b[0m" (S_NOR) [inclusive]
# usage example: python process-log.py o.log

import sys

o : bytes = b''
S_YELLOW : bytes = b'\x1b[33m'
S_NOR : bytes = b'\x1b[0m'
with open(sys.argv[1], 'rb') as f:
    text: bytes = f.read()
    while True:
        # 1. find the next S_YELLOW
        i_yellow = text.find(S_YELLOW)
        # if not found, push everything and break
        if i_yellow == -1:
            o += text
            break
        # print(f'🦜 : Found S_YELLOW at {i_yellow}')
        # push everything before here, and eat till the end of the file
        o += text[:i_yellow]
        text = text[i_yellow:]
        # 2. find the next S_NOR
        i_nor = text.find(S_NOR)
        # if not found, push everything and break
        if i_nor == -1:
            o += text
            break
        # print(f'🦜 : Found S_NOR at {i_nor}')
        # eat everything including S_NOR
        text = text[i_nor+len(S_NOR):]

# print raw byte to stdout
sys.stdout.buffer.write(o)
    # 1. 🦜 : keep putting bytes into a buffer until we find S_YELLOW
    # 2. 🦜 : keep putting bytes into a buffer until we find S_NOR

