import MonacoEditor from "@monaco-editor/react";

type Props = {
  code: string;
  onChange: (value: string) => void;
};

export default function Editor({ code, onChange }: Props) {
  return (
    <div className="overflow-hidden rounded-b-xl">
      <MonacoEditor
        height="480px"
        defaultLanguage="c"
        value={code}
        theme="vs-dark"
        options={{
          minimap: { enabled: false },
          fontSize: 14,
          fontFamily: "JetBrains Mono, ui-monospace, SFMono-Regular, Menlo, Monaco, Consolas, monospace",
          padding: { top: 12 },
          scrollbar: { verticalScrollbarSize: 8, horizontalScrollbarSize: 8 },
        }}
        onChange={(v) => onChange(v || "")}
      />
    </div>
  );
}

