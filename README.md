# tea

Tea, pronounced /teə/, is a multi-backend, general-purpose programming language designed to be easy to understand while providing access to low-level features.

## Features

- Syntax inspired by C++, Luau, and GDScript for intuitive learning
- Cross-platform compilation to native code using the LLVM backend
- Rich standard library & package index
- Support for compliation to Luau bytecode (experimental)

## Quick Start

1. Download the [latest release](https://github.com/felixsidzed/tea/releases) of tea
2. Take a [tour of tea](#tour)
3. Read the [language documentation](https://felixsidzed.github.io/tea-docs)
4. Write your first program

## Hello, World!

```tea
using "io";

public func main() -> int
	io::print("Hello, World!\n");
	return 0;
end
```

## Tour

### Variables

```tea
// Standard variable declaration
var a: string = "Hello, World!\n";

// Uninitialized variables
var b: int;

// Type inference
var c = 3.14f;
```

### Functions

```tea
public func tan(double x) -> double
	return sin(x) / cos(x);
end
```

### Control Flow

#### Conditionals

```tea
if (ok) do
	io::print("Success!");
else
	io::print("Fail!");
end
```

#### Loops

```tea
// For loop
for (var k = 1; k <= 128; k *= 2) do
	io::printf("k = %d\n", k);
end

// While loop
while (i < 5) do
	io::printf("i = %d\n", i);
	i += 1;
end
```

### Assignments & Function Calls

```tea
// Variable assignment
a = 67;
m[0] = 3.14;

// Function calls
x = tan(30.0 * 3.1415 / 180.0);
h.push(123);
h.pop();
```

### Method Calls

```tea
// Standard method call
str = str.sub(3, 4);

// Arrow method call (equivalent to `(*str).sub`)
*str = str->sub(3, 4);
```

### Pointers & Casting

```tea
var f: double = 42.42;
var pf: double* = &f;
var pi: int* = (int*)pf;

io::printf("double: %f\n", f);
io::printf("int: %d\n", *pi);
```

### Macros

```tea
macro PI 3.14159;
macro DEBUG_MODE true;

io::printf("PI = %f\n", PI);
```

## Documentation

For detailed documentation on syntax, types, and the standard library, see:
- [Language Reference](https://felixsidzed.github.io/tea-docs/language_reference.html)
- [Standard Library](https://felixsidzed.github.io/tea-docs/standard_library_reference.html)
- [Examples](https://github.com/felixsidzed/tea/tree/main/playground/examples)

## Project History

Tea has evolved through several iterations:

- **dia** - Initial version written in Python with an AST walker
- **lynx** - Transpiled to Python code and executed via `exec()`
- **llvm-test** - Used llvmlite's JIT compiler as a sort of proof of concept
- **tea v1** - First LLVM-based version in Python
- **tea v2** - Rewritten in C++ with improved architecture
- **tea v3** (current) — Complete rewrite with mid-level IR

Tea was designed as my vision of what a perfect programming language should be - balancing simplicity with power, and high-level expressiveness with low-level control.

## Contributing

Contributions are welcome! Feel free to submit issues and pull requests.

## License

MIT License. See [LICENSE](LICENSE) for details.

---

**Note:** Tea is under active development. Some features may be incomplete or subject to change.
