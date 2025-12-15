type Props = {
  result: any;
  error: string | null;
};

export default function OutputPanel({ result, error }: Props) {
  if (error) {
    return (
      <div className="px-4 py-3 text-sm text-rose-300">
        <p className="font-semibold">Request failed</p>
        <pre className="mt-2 whitespace-pre-wrap text-rose-200">{error}</pre>
      </div>
    );
  }

  if (!result) {
    return (
      <div className="px-4 py-3 text-sm text-slate-300">
        Send code to run and view output here.
      </div>
    );
  }

  const showCompile = result.compile_log && result.compile_log.trim().length > 0;

  return (
    <div className="space-y-3 px-4 py-3 text-sm text-slate-100">
      <div>
        <p className="text-xs uppercase tracking-wide text-slate-400">Program Output</p>
        <pre className="mt-1 whitespace-pre-wrap rounded-lg bg-surface-700 px-3 py-2 text-slate-100">
          {result.output || "<empty>"}
        </pre>
      </div>
      {showCompile && (
        <div>
          <p className="text-xs uppercase tracking-wide text-amber-300">Compiler Log</p>
          <pre className="mt-1 whitespace-pre-wrap rounded-lg bg-surface-700 px-3 py-2 text-amber-100">
            {result.compile_log}
          </pre>
        </div>
      )}
    </div>
  );
}

