import json
import os
import subprocess
import tempfile
from flask import Flask, request, jsonify
from flask_cors import CORS

app = Flask(__name__)
CORS(app, resources={r"/*": {"origins": "*"}})

# Path to the compiled C engine
SENTINEL_BIN = os.environ.get("SENTINEL_BIN", os.path.join(os.path.dirname(__file__), "..", "sentinel"))

# Basic guardrails
MAX_CODE_BYTES = 200_000  # ~200 KB


@app.route("/run", methods=["POST"])
def run_code():
    if not os.path.exists(SENTINEL_BIN):
        return jsonify({"error": f"sentinel binary not found at {SENTINEL_BIN}"}), 500

    data = request.get_json(silent=True)
    if not data or "code" not in data:
        return jsonify({"error": "expected JSON with 'code'"}), 400

    code = data["code"]
    if not isinstance(code, str):
        return jsonify({"error": "'code' must be a string"}), 400
    if len(code.encode("utf-8")) > MAX_CODE_BYTES:
        return jsonify({"error": "code too large"}), 400

    filename = data.get("filename") or "submission.c"

    # Write code to a temp file
    with tempfile.NamedTemporaryFile(mode="w", suffix=".c", delete=False) as tmp:
        tmp.write(code)
        src_path = tmp.name

    try:
        # Call sentinel; it prints one JSON line per job
        completed = subprocess.run(
            [SENTINEL_BIN, src_path],
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            timeout=10,
            text=True,
        )

        if completed.returncode != 0:
            return jsonify(
                {
                    "error": "sentinel failed",
                    "returncode": completed.returncode,
                    "stderr": completed.stderr,
                    "stdout": completed.stdout,
                }
            ), 500

        # Sentinel outputs JSON per job; grab the last non-empty line
        lines = [ln for ln in completed.stdout.splitlines() if ln.strip()]
        line = lines[-1] if lines else ""
        try:
            result = json.loads(line)
        except json.JSONDecodeError:
            return jsonify({"error": "invalid JSON from sentinel", "raw": line}), 500

        result["source_file"] = os.path.basename(src_path if filename == "submission.c" else filename)
        return jsonify(result), 200

    finally:
        try:
            os.remove(src_path)
        except OSError:
            pass


@app.route("/health", methods=["GET"])
def health():
    return {"status": "ok"}


if __name__ == "__main__":
    app.run(host="0.0.0.0", port=8000, debug=True)


