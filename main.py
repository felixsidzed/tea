import argparse
import platform
from parser import Parser
from codegen import CodeGen

errors = []

def panic(fmt, *args):
	errors.append(fmt % args)

def main() -> None:
	parser = argparse.ArgumentParser(description="Compile a Tea source file into an object file.")
	parser.add_argument("source", help="Path to the Tea source file")
	parser.add_argument("-o", "--output", default=None, help="Path to the output object ")
	parser.add_argument("-v", "--verbose", action="store_true", help="Enable verbose(debug) output")
	parser.add_argument("-64", "--force64bit", action="store_true", help="Force 64-bit code generation")
	parser.add_argument("-32", "--force32bit", action="store_true", help="Force 32-bit code generation")

	args = parser.parse_args()

	if args.force32bit:
		args.force64bit = False
	elif not args.force64bit:
		args.force64bit = platform.architecture()[0] == "64bit"
	if args.verbose:
		print(f" Using {64 if args.force64bit else 32}-bit code generation")

	if args.output is None:
		args.output = ".".join(args.source.split(".")[:-1]) + ".o"

	try:
		with open(args.source, "r") as f:
			src = f.read()
			f.close()
	except FileNotFoundError:
		print(f"1 error(s):\n(1) error: {args.source}: file not found")
		exit(1)
	except Exception as e:
		print(f"1 error(s):\n(1) error: could not open '{args.source}' file: {e}")
		exit(1)

	parser = Parser()
	module = parser.parse(src)

	codegen = CodeGen(verbose=args.verbose, is64Bit=args.force64bit, panic=panic)

	coff = codegen.generate(module)

	if len(errors) > 0:
		print(f"{len(errors)} error(s):")
		for i, error in enumerate(errors, 1):
			print(f"({i}) {error}")
		exit(1)

	try:
		with open(args.output, "wb") as f:
			f.write(coff)
	except Exception as e:
		print(f"error: failed to write to output file '{args.output}': {e}")
		exit(1)

if __name__ == "__main__":
	main()
