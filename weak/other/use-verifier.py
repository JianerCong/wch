from verifier import verify_and_parse_func
import json
with open("hi.py") as f:
    # r = None
    # try:
    #     r = verify_and_parse_func(f, parse_it=True)
    # except Exception as e:
    #     r = 'Error verifying and parsing contract: ' + str(e)
    # with open("verifier-result.json", "w") as f1:
    #     json.dump(r, f1)
    r = verify_and_parse_func(f, parse_it=True)
    # ^ 🦜 : Let it throw
    with open("verifier-result.json", "w") as f1:
        json.dump(r, f1)
