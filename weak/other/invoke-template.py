import json
import os
args = None

wd = os.environ["PWD"]
# print(f'🦜 Verifyer started: PWD = {S.CYAN}{wd}{S.NOR}')
p = os.path.join(wd, 'args.json')
with open(p, 'r') as f:
    args = json.load(f)

out = {}
if '_storage' in args:
    # take the _storage out
    _storage = args['_storage']
    del args['_storage']
    out['result'] = METHOD(**args, _storage=_storage)
    out['storage'] = _storage   # modified storage
else:
    out['result'] = METHOD(**args)

p = os.path.join(wd, 'result.json')
with open(p, 'w') as f:
    json.dump(out, f)
