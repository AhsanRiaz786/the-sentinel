# The Sentinel

Cyber-sandbox for compiling and running untrusted C code with OS-level controls, a Flask bridge, and a React (Vite) UI.

## Why we built it
- Showcase core OS concepts: `fork/exec`, `pipe`, `setrlimit`, signals, and watchdog timers.
- Provide a safe “online judge”-style sandbox for student code.
- Demonstrate concurrency (thread pool), isolation, and a full stack bridge/UI.

## High-level architecture
1) **C engine (`sentinel`)**  
   - Compiles submitted C code to a temp binary.  
   - Runs it in a sandboxed child process with `setrlimit` (CPU 2s, ~256MB AS), file size limits, and a wall-clock watchdog (SIGKILL on timeout).  
   - Captures stdout/stderr via pipes, reports exit code/signal, time, and max RSS.  
   - Banned-token scan (e.g., `system(`) before compile.  
   - Thread pool + bounded queue to handle multiple submissions.
2) **Flask bridge (`api/app.py`)**  
   - `POST /run` accepts JSON `{ code, filename? }`, writes to temp file, shells out to `sentinel`, returns the JSON result.  
   - CORS enabled for the front-end.
3) **React UI (`ui/`)**  
   - Monaco editor for code entry.  
   - Output panel with status badges and compiler log.  
   - Stat cards (CPU time, max RSS, exit code, signal).  
   - Tailwind-based “cyber” theme.

## Repository layout
- `sentinel.c` — C engine (queue, compile, sandbox run, JSON out)
- `Makefile` — build helper for sentinel
- `api/app.py` — Flask API bridge (`/run`, `/health`)
- `api/requirements.txt` — Python deps
- `api/test_run.py` — sample POST to the API
- `ui/` — Vite/React/Tailwind/Monaco front-end
- `.gitignore` — ignores build, venv, node_modules, etc.

## Building and running
### 1) Build the C engine
```bash
cd /Users/ahsanriaz/Developer/the-sentinel
make        # outputs ./sentinel
```

### 2) Run the Flask API
```bash
cd api
python -m venv .venv
source .venv/bin/activate
pip install -r requirements.txt
export SENTINEL_BIN="../sentinel"   # optional if not in default location
export FLASK_APP=app.py
export FLASK_RUN_PORT=8000          # UI default points to 8000
flask run --host 0.0.0.0 --port 8000
```

### 3) Run the React UI
```bash
cd ../ui
npm install
# Point UI to API (defaults to 8000)
export VITE_API_BASE="http://localhost:8000"
npm run dev
# open http://localhost:5173
```

### 4) Smoke test the API (without UI)
```bash
cd api
source .venv/bin/activate
python test_run.py
```

## API contract
- `POST /run`
  - Request JSON: `{ "code": "<c source string>", "filename": "optional.c" }`
  - Response JSON (from sentinel):  
    - `status`: `Success | CompileError | RuntimeError | TimeLimitExceeded | Banned`
    - `output`: combined stdout/stderr
    - `compile_log`: compiler stderr if any
    - `time_ms`, `max_rss_kb`, `exit_code`, `signal`, `timed_out`, `banned`
- `GET /health` → `{ "status": "ok" }`

## C engine details (sentinel.c)
- Thread pool: 3 workers, bounded queue (64 jobs).
- Compile path: mkstemp temp binary, `gcc -std=c11 -O2 -pipe`, logs to `/tmp/sentinel-compile.log`.
- Security: banned tokens (`system(`, `fork(`, `exec`, etc.) abort before compile.
- Sandboxing: `setrlimit` CPU, AS, file size; wall-clock watchdog kills after 2s.
- Output: JSON per job, printed to stdout (one line).

## Front-end notes
- Monaco editor for a VS Code-like feel.
- Status pill shows Success/CompileError/TimeLimitExceeded/etc.
- Output panel shows program stdout and compiler log (if present).
- Stat cards show CPU time, max RSS, exit code, and signal.

## Common issues
- CORS / port mismatch: ensure API runs on the port set in `VITE_API_BASE` (default 8000) and that Flask CORS is enabled (already added).
- Sentinel binary path: set `SENTINEL_BIN` if `./sentinel` isn’t found.
- Infinite loops: will be killed at ~2s and reported as `TimeLimitExceeded`.

## Demo ideas
- Normal run: small printf loop → Success.
- Compile error: missing semicolon → CompileError with compiler log.
- Infinite loop: while(1) → TimeLimitExceeded.
- Banned token: `system("rm -rf /")` → Banned.

## Future improvements
- Separate stdout/stderr fields in the JSON.
- Configurable limits via CLI/ENV.
- Persist submission history (SQLite) and display in UI.
- Auth and per-user quotas.


