Syntax Reference
=============================

This section provides detailed documentation on the syntax and structure of the Tea language.

Basic Concepts
--------------

- **Lua-like syntax**: The language syntax is inspired by Lua, which is simple and concise.
- **Static Typing**: Tea uses statically typed variables and functions.

Functions
Function definitons in Tea follow a format similar to Lua but with static typing.

.. code-block:: tea

	public func main() -> int
		return 0;
	end

This example defines a function *main*, with a return type of *int*. The function has no arguments and returns an integer value.

Function Syntax
---------------
* Storage Class: Defines the visibility of the function (e.g., public or private).

* (*optional*) Calling Convention: The calling convention of the function

* Function Name: A valid identifier for the function.

* Arguments: Enclosed in parentheses, with each argument having a type and a name.

* Return Type: Specifies the type of value returned by the function.

* Body: The body of the function contains statements that define its behavior.

Function Definition Example

.. code-block:: tea

	public func add(int a, int b) -> int
		return a + b;
	end

This function add takes two integer arguments and returns an integer result.

Function Variants
-----------------
Tea supports various forms of function definitions:

1. Standard Function Definition:
	Declares a function with the system-default calling convention.

	.. code-block:: tea

		public func main() -> void
			// code here
		end

2. Function with Calling Convention:
	Specifies a calling convention, like __stdcall.

	.. code-block:: tea

		public __stdcall func calculate(int x) -> float
			return x * 3.14;
		end

3. Function Declaration (extern):
	Functions can be declared without a body (similar to forward declarations in C/C++).

	.. code-block:: tea

		public func compute(int a) -> int;

Arguments
---------
Arguments are declared in parentheses and separated by commas. Each argument is given a type.

Example:

.. code-block:: tea
	
	public func greet(string name) -> string
		return str::cat(3, "Hello, ", name, "!");
	end

Types
-----
Tea supports the following types:

Primitive Types: - int, float, double, char, string, bool, void

Pointer Types:
Use * to define a pointer type:

.. code-block:: tea

	var ptr: int*;


Statements
----------
Tea supports several types of statements that define the behavior of a program:

	* Return Statement
		A function can return a value with the return keyword.

		.. code-block:: tea

			return 42;

	* Variable Declaration
		Variables can be declared with a specified type and optionally initialized with a value.

		.. code-block:: tea

			var x: int = 5;
			var y: float;

	* Control Flow
		Tea includes control flow statements such as if, elseif, and else for conditional execution:

		.. code-block:: tea

			if (x > 0) do
				return "positive";
			else
				return "negative";
			end

	* Loops
		Tea supports while and for loops.

		.. code-block:: tea

			while (x > 0) do
				x -= 1;
			end

		.. code-block:: tea

			for (var i: int = 0; i < 10; i += 1) do
				io::printf("%d\n", i);
			end

	* Expressions
		Expressions in Tea are evaluated based on operator precedence. Operators are used to manipulate values.

		Arithmetic: +, -, \*, /

		Comparison: ==, !=, <, <=, >, >=

		Logical: && (and), || (or)

		Unary Operators: ! (not)

		Example of an expression:

		.. code-block:: tea

			var result: int = (x + y) * z;

	* Module Imports
		Tea supports the use of modules, which can import code from other files. Modules are included using the using keyword.

		.. code-block:: tea

			using "math";
