from parser import Parser

def getModuleInclude(src: str, filename: str) -> dict:
	parser = Parser()
	module = parser.parse(src, filename=filename)

	return module
