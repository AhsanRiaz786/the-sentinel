export type SentinelResponse = {
  job_id: number;
  status: string;
  output: string;
  compile_log: string;
  time_ms: number;
  max_rss_kb: number;
  exit_code: number;
  signal: number;
  timed_out: boolean;
  banned: boolean;
};

const API_BASE = import.meta.env.VITE_API_BASE || "http://localhost:8000";

export async function runCode(code: string): Promise<SentinelResponse> {
  const res = await fetch(`${API_BASE}/run`, {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify({ code }),
  });

  if (!res.ok) {
    const msg = await res.text();
    throw new Error(`HTTP ${res.status}: ${msg}`);
  }
  return res.json();
}

