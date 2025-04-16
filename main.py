from parser import Parser
from codegen import CodeGen

def printBytes(data: bytearray):
	for i in range(0, len(data), 16):
		chunk = data[i:i+16]
		hex = ' '.join(f"{b:02x}" for b in chunk)
		print(f"{i:04x}: {hex}")

errors = []
def panic(fmt, *args):
	errors.append(fmt % args)

def main() -> None:
	src: str = ""
	with open("test.tea") as f:
		src = f.read()
		f.close()

	parser: Parser = Parser()
	module = parser.parse(src)
	
	codegen: CodeGen = CodeGen(panic=panic)

	if len(errors) > 0:
		for error in errors:
			print(error + "\n")
		exit(1)

	coff = codegen.generate(module)
	with open("build/test.o", "wb") as f:
		f.write(coff)
		f.close()

	print(f"=== [module 'test'] ({len(coff)} bytes) ===")
	printBytes(coff)

if __name__ == "__main__":
	main()
