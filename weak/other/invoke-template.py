import json
args = None
with open('args.json', 'r') as f:
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

with open('result.json', 'w') as f:
    json.dump(r, f)
