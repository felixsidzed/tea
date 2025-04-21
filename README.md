# Tea

**Tea**, pronounced /teə/, is a lightweight, general purpose scripting language inspired by Lua. Its designed to be easy to understand while having low-level access, and it compiles down to a native object file.

---

## Features

- Lua-like syntax
- Compiles to a native object file
- Easy to integrate with C/C++ code
- Rich standard library (`teastd`)

---

## Example Usage

To compile a Tea program into a native object file, you can use the following command:

```bat
tea test.tea -o build/test.o
```

---

## Example Directory Structure

```
.
├── tea.exe			# Tea CLI
├── test.tea			# Tea source file
├── build/
│   └── test.o			# Compiled object file
├── lib/
│   └── teastd.lib		# Tea standard library
```

---

## Requirements

- Python 3.11+
- Microsoft Visual Studio Build Tools (for `link.exe`)

---

## License

MIT License. See `LICENSE` file for details.
