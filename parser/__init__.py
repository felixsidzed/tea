import lark
import pathlib

from parser.nodes import *

MODULE_LOOKUP = [
	"stdlib",
	"."
]

def resolvePath(path: pathlib.Path):
    for directory in MODULE_LOOKUP:
        path = pathlib.Path(directory) / (path.name + ".json")
        if path.exists():
            return path
    return None

_functioncache = {}
class AST(lark.Transformer):
	def module(self, children):
		return ModuleNode(1, 1, children)

	def STRING(self, token: lark.Token):
		return lark.Token("STRING", token.value[1:-1].encode("utf-8").decode("unicode_escape"), line=token.line, column=token.column)

	def INT(self, token: lark.Token):
		return lark.Token("INT", int(token.value, 0), line=token.line, column=token.column)

	def FLOAT(self, token: lark.Token):
		return lark.Token("FLOAT", float(token.value), line=token.line, column=token.column)

	def DOUBLE(self, token: lark.Token):
		return lark.Token("DOUBLE", float(token.value), line=token.line, column=token.column)

	def IDENTF(self, token: lark.Token):
		if token.value in ("true", "false"):
			return lark.Token("BOOL", True if token.value == "true" else (False if token.value == "false" else token),
					line=token.line, column=token.column)
		return token

	def CHAR(self, token: lark.Token):
		return lark.Token("CHAR", ord(token.value[1:-1].encode("utf-8").decode("unicode_escape")), line=token.line, column=token.column)

	def function(self, items: list[lark.Token | lark.Tree]):
		storageType = items[0].value
		name = items[1]
		args = items[2].children
		returnType = items[3]
		body = items[4:]

		node =  FunctionNode(
			STORAGE2I[storageType],
			"__cdecl",
			name.value,
			Type.get(returnType),
			{arg.children[1].value: Type.get(arg.children[0].value) for arg in args},
			body,
			name.line,
			name.column
		)
		_functioncache[name.value] = node
		return node
	
	def function2(self, items: list[lark.Token | lark.Tree]):
		storageType = items[0].value
		conv = items[1].value
		name = items[2]
		args = items[3].children
		returnType = items[4]
		body = items[5:]

		node = FunctionNode(
			STORAGE2I[storageType],
			conv,
			name.value,
			Type.get(returnType),
			{arg.children[1].value: Type.get(arg.children[0].value) for arg in args},
			body,
			name.line,
			name.column
		)
		_functioncache[name.value] = node
		return node

	def function_declaration(self, items: list[lark.Token | lark.Tree]):
		storageType = items[0].value
		conv = items[1].value
		name = items[2]
		args = items[3].children
		returnType = items[4]

		node = FunctionDeclarationNode(
			STORAGE2I[storageType],
			conv,
			name.value,
			Type.get(returnType),
			{arg.children[1].value: Type.get(arg.children[0].value) for arg in args},
			name.line,
			name.column
		)
		return node

	def using(self, items: list[lark.Token | lark.Tree]):
		path = resolvePath(pathlib.Path(items[0].value))

		return UsingNode(
			path.name.split(".")[0] if path is not None else path,
			path.resolve() if path is not None else path,
			items[0].line,
			items[0].column,
		)

	def direct_call(self, items: list[lark.Token | lark.Tree]):
		name = items[0]
		args = items[1] if len(items) > 1 else []

		return CallNode(
			name.value,
			[arg.children[0] for arg in args.children],
			[],
			_functioncache.get(name.value, None),
			name.line,
			name.column
		)

	def scoped_call(self, items: list[lark.Token | lark.Tree]):
		*scope, name, args = items
		scope = [tok.value for tok in scope]

		return CallNode(
			name.value,
			[arg.children[0] for arg in args.children],
			scope,
			_functioncache.get("::".join(scope + [name.value]), None),
			name.line,
			name.column
		)

	def add(self, items: list[lark.Token | lark.Tree]):
		left, right = items
		return AddNode(left.line, left.column, left, right)

	def sub(self, items: list[lark.Token | lark.Tree]):
		left, right = items
		return SubNode(left.line, left.column, left, right)

	def mul(self, items: list[lark.Token | lark.Tree]):
		left, right = items
		return MulNode(left.line, left.column, left, right)

	def div(self, items: list[lark.Token | lark.Tree]):
		left, right = items
		return DivNode(left.line, left.column, left, right)

	def rreturn(self, items: list[lark.Token | lark.Tree]):
		value = None
		try:
			value = items[0]
			return ReturnNode(value, Type.get(value.type), value.line, value.column)
		except IndexError:
			return ReturnNode(None, Type.VOID, value.line, value.column)
		except AttributeError:
			return ReturnNode(value, None, value.line, value.column)

	def variable(self, items: list[lark.Token | lark.Tree]):
		name, dataType, *value = items
		return VariableNode(
			name.value,
			Type.get(dataType.value),
			value[0] if len(value) > 0 else None,
			name.line,
			name.column
		)


class Parser:
	def __init__(self) -> None:
		grammar: str = ""
		with open("grammar.lark" if __name__ == "__main__" else "parser/grammar.lark", "r") as f:
			grammar = f.read()
			f.close()
		self.lark = lark.Lark(grammar, propagate_positions=True, start="module")

	def parse(self, src: str) -> ModuleNode:
		return AST().transform(self.lark.parse(src))
