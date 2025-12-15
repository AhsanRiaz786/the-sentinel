import { useMemo, useState } from "react";
import { PlayIcon, ArrowPathIcon } from "@heroicons/react/24/outline";
import Editor from "./components/Editor";
import OutputPanel from "./components/OutputPanel";
import StatsCard from "./components/StatsCard";
import { runCode } from "./api";

type RunResult = {
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

const sampleCode = `#include <stdio.h>
int main() {
    for(int i=0;i<3;i++){
        printf("hello %d\\n", i);
    }
    return 0;
}`;

function App() {
  const [code, setCode] = useState(sampleCode);
  const [loading, setLoading] = useState(false);
  const [result, setResult] = useState<RunResult | null>(null);
  const [error, setError] = useState<string | null>(null);

  const statusColor = useMemo(() => {
    if (!result) return "bg-slate-700 text-slate-100";
    if (result.status === "Success") return "bg-emerald-500 text-emerald-950";
    if (result.status === "TimeLimitExceeded") return "bg-amber-400 text-amber-950";
    if (result.status === "CompileError" || result.status === "Banned") return "bg-rose-500 text-rose-50";
    return "bg-slate-500 text-slate-50";
  }, [result]);

  const handleRun = async () => {
    setLoading(true);
    setError(null);
    setResult(null);
    try {
      const res = await runCode(code);
      setResult(res);
    } catch (err: any) {
      setError(err?.message || "Request failed");
    } finally {
      setLoading(false);
    }
  };

  const handleReset = () => {
    setCode(sampleCode);
    setResult(null);
    setError(null);
  };

  return (
    <div className="min-h-screen bg-surface-900 text-slate-50">
      <header className="border-b border-slate-800 bg-surface-800/80 backdrop-blur">
        <div className="mx-auto flex max-w-6xl items-center justify-between px-4 py-4">
          <div className="flex items-center gap-3">
            <span className="text-lg font-semibold text-accent-300">The Sentinel</span>
            <span className="text-xs rounded-full bg-slate-800 px-3 py-1 text-slate-200">
              Cyber Sandbox
            </span>
          </div>
          <div className="flex gap-2">
            <button
              onClick={handleRun}
              disabled={loading}
              className="flex items-center gap-2 rounded-md bg-accent-500 px-4 py-2 text-sm font-semibold text-surface-900 shadow hover:brightness-110 disabled:opacity-60"
            >
              <PlayIcon className="h-4 w-4" />
              {loading ? "Running..." : "Run"}
            </button>
            <button
              onClick={handleReset}
              className="flex items-center gap-2 rounded-md border border-slate-700 px-4 py-2 text-sm text-slate-200 hover:border-slate-500"
            >
              <ArrowPathIcon className="h-4 w-4" />
              Reset
            </button>
          </div>
        </div>
      </header>

      <main className="mx-auto grid max-w-6xl gap-4 px-4 py-4 lg:grid-cols-[1.3fr,0.7fr]">
        <section className="flex flex-col gap-3">
          <div className="rounded-xl border border-slate-800 bg-surface-800 shadow-lg shadow-slate-900/40">
            <div className="flex items-center justify-between border-b border-slate-800 px-4 py-3">
              <div>
                <p className="text-sm font-semibold text-slate-100">Code</p>
                <p className="text-xs text-slate-400">C (compiled and sandboxed)</p>
              </div>
              {result && (
                <span className={`rounded-full px-3 py-1 text-xs font-semibold ${statusColor}`}>
                  {result.status}
                </span>
              )}
            </div>
            <Editor code={code} onChange={setCode} />
          </div>
        </section>

        <section className="flex flex-col gap-3">
          <div className="rounded-xl border border-slate-800 bg-surface-800 shadow-lg shadow-slate-900/40">
            <div className="border-b border-slate-800 px-4 py-3">
              <p className="text-sm font-semibold text-slate-100">Output</p>
            </div>
            <OutputPanel result={result} error={error} />
          </div>

          <div className="grid grid-cols-2 gap-3">
            <StatsCard label="CPU Time" value={result ? `${result.time_ms} ms` : "—"} />
            <StatsCard label="Max RSS" value={result ? `${result.max_rss_kb} KB` : "—"} />
            <StatsCard label="Exit Code" value={result ? `${result.exit_code}` : "—"} />
            <StatsCard label="Signal" value={result ? `${result.signal}` : "—"} />
          </div>
        </section>
      </main>
    </div>
  );
}

export default App;

