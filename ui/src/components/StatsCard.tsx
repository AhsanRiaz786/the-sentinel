type Props = {
  label: string;
  value: string;
};

export default function StatsCard({ label, value }: Props) {
  return (
    <div className="rounded-xl border border-slate-800 bg-surface-800 px-3 py-3 shadow shadow-slate-900/30">
      <p className="text-xs uppercase tracking-wide text-slate-400">{label}</p>
      <p className="text-lg font-semibold text-slate-100">{value}</p>
    </div>
  );
}

