# Test Files

Sample C programs for testing the Sentinel engine:

- `user_code.c` - Basic working example (prints hello)
- `bad.c` - Compilation error (undeclared identifier)
- `banned.c` - Contains banned token (`system()`)
- `loop.c` - Infinite loop (triggers timeout)

## Testing

From the root directory:
```bash
cd engine
./sentinel ../tests/user_code.c
./sentinel ../tests/bad.c
./sentinel ../tests/banned.c
./sentinel ../tests/loop.c
```

