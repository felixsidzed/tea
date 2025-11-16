# tea
# This rewrite is still in progress, so most of the content is not yet functional

Tea, pronounced /teə/, is a lightweight, general purpose programming language inspired by many. Its designed to be easy to understand while having low-level access, and compiling down to a native executable file.

---

## Features

- Readable Lua-like syntax
- Compiles to a native executable file
- Easy to integrate with C/C++ code
- Rich standard library (`teastd`)

---

## Example Usage

To compile a Tea program into a native executable file, you can use the following command:

```bat
tea test.tea -o test.exe
```

---

## Example Directory Structure

```
.
├── tea.exe        # Tea CLI
├── test.tea       # Tea source file
├── test.exe       # Compiled executable file
├── lib/
│   └── teastd.lib # Tea standard library
```

---

## Requirements

- Microsoft Visual Studio Build Tools

---

## License

MIT License. See `LICENSE` file for details.
