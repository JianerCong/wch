import json
args = None
with open('args.json', 'r') as f:
    args = json.load(f)
r = METHOD(**args)
with open('result.json', 'w') as f:
    json.dump(r, f)
