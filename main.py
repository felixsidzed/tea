from parser import Parser
from codegen import CodeGen

#LIB = {
#	"kernel32": "\"C:\\Program Files (x86)\\Windows Kits\\10\\Lib\\10.0.22621.0\\um\\x64\\kernel32.lib\"",
#	"user32": "\"C:\\Program Files (x86)\\Windows Kits\\10\\Lib\\10.0.22621.0\\um\\x64\\user32.lib\""
#}

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
	
	codegen: CodeGen = CodeGen(verbose=True, is64Bit=True, panic=panic)

	coff = codegen.generate(module)
	if len(errors) > 0:
		print(f"{len(errors)} error(s):")
		for i, error in enumerate(errors, 1):
			print(f"({i}) {error}")
		exit(1)
	
	print(f"=== [module 'test'] ({len(coff)} bytes) ===")
	printBytes(coff)

	with open("build/test.o", "wb") as f:
		f.write(coff)
		f.close()

if __name__ == "__main__":
	main()
