import json
import requests

# Simple smoke test against the running Flask server

payload = {
    "code": r'#include <stdio.h>\nint main(){ printf("hi\\n"); return 0; }',
}

r = requests.post("http://localhost:5000/run", json=payload, timeout=5)
print("status", r.status_code)
print(json.dumps(r.json(), indent=2))


