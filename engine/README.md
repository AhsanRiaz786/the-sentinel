# Sentinel Engine

The C-based execution engine that compiles and sandboxes user code.

## Build

```bash
make        # Builds ./sentinel binary
make clean  # Removes binary and temp files
```

## Usage

```bash
./sentinel <file1.c> [file2.c ...]
```

Outputs JSON per job to stdout.

