import os
import sys
import pathlib

import lark

from parser.nodes import *

MODULE_LOOKUP = [
	"stdlib",
	"."
]
_functioncache = {}


def resource(relative_path):
    try:
        base_path = sys._MEIPASS
    except AttributeError:
        base_path = os.path.abspath(".")
    return os.path.join(base_path, relative_path)


def resolvePath(path: pathlib.Path):
	for directory in MODULE_LOOKUP:
		path = pathlib.Path(directory) / (path.name + ".json")
		if path.exists():
			return path
	return None


class AST(lark.Transformer):
	def module(self, children):
		return ModuleNode(1, 1, children)

	def STRING(self, token: lark.Token):
		return lark.Token("STRING", token.value[1:-1].encode("utf-8").decode("unicode_escape"), line=token.line, column=token.column)

	def INT(self, token: lark.Token):
		return lark.Token("INT", int(token.value, 0), line=token.line, column=token.column)

	def FLOAT(self, token: lark.Token):
		return lark.Token("FLOAT", float(token.value[:-1]), line=token.line, column=token.column)

	def DOUBLE(self, token: lark.Token):
		return lark.Token("DOUBLE", float(token.value), line=token.line, column=token.column)

	def IDENTF(self, token: lark.Token):
		if token.value in ("true", "false"):
			return lark.Token("BOOL", True if token.value == "true" else (False if token.value == "false" else token), line=token.line, column=token.column)
		return token

	def CHAR(self, token: lark.Token):
		return lark.Token("CHAR", ord(token.value[1:-1].encode("utf-8").decode("unicode_escape")), line=token.line, column=token.column)

	def function(self, items: list[lark.Token | lark.Tree | Node]):
		storageType = items[0].value
		name = items[1]
		args = items[2].children
		returnType = items[3]
		body = items[4:]

		node = FunctionNode(
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

	def function2(self, items: list[lark.Token | lark.Tree | Node]):
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

	def function_declaration(self, items: list[lark.Token | lark.Tree | Node]):
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

	def using(self, items: list[lark.Token | lark.Tree | Node]):
		path = resolvePath(pathlib.Path(items[0].value))

		return UsingNode(
			path.name.split(".")[0] if path is not None else items[0].value,
			path.resolve() if path is not None else path,
			items[0].line,
			items[0].column,
		)

	def direct_call(self, items: list[lark.Token | lark.Tree | Node]):
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

	def scoped_call(self, items: list[lark.Token | lark.Tree | Node]):
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

	def add(self, items: list[lark.Token | lark.Tree | Node]):
		left, right = items
		return AddNode(left.line, left.column, left, right)

	def sub(self, items: list[lark.Token | lark.Tree | Node]):
		left, right = items
		return SubNode(left.line, left.column, left, right)

	def mul(self, items: list[lark.Token | lark.Tree | Node]):
		left, right = items
		return MulNode(left.line, left.column, left, right)

	def div(self, items: list[lark.Token | lark.Tree | Node]):
		left, right = items
		return DivNode(left.line, left.column, left, right)

	def rreturn(self, items: list[lark.Token | lark.Tree | Node]):
		value = items[0]
		try:
			value = items[1]
			return ReturnNode(value, Type.get(value.type)[0], value.line, value.column)
		except IndexError:
			return ReturnNode(None, Type.VOID, value.line, value.column)
		except AttributeError:
			return ReturnNode(value, None, value.line, value.column)

	def variable(self, items: list[lark.Token | lark.Tree | Node]):
		name, dataType, *value = items
		return VariableNode(
			name.value,
			Type.get(dataType.value),
			value[0] if len(value) > 0 else None,
			name.line,
			name.column
		)

	def if_block(self, items: list[lark.Token | lark.Tree | Node]):
		return IfNode(
			items[0],
			items[1:],
			items[0].line,
			items[0].column,
		)

	def ifelse_block(self, items: list[lark.Token | lark.Tree | Node]):
		condition = items[0]
		body = []
		elseifBlocks = []
		elseBlock = None

		i = 1
		while i < len(items):
			item = items[i]
			if type(item) == ElseNode:
				elseBlock = item
				break
			elif type(item) == ElseIfNode:
				elseifBlocks.append(item)
			else:
				body.append(item)
			i += 1

		return IfNode(
			condition,
			body,
			condition.line,
			condition.column,
			elseBlock,
			elseifBlocks,
		)

	def else_block(self, items: list[lark.Token | lark.Tree | Node]):
		return ElseNode(
			items[1:],
			items[0].line,
			items[0].line
		)

	def elseif_block(self, items: list[lark.Token | lark.Tree | Node]):
		return ElseIfNode(
			items[0],
			items[1:],
			items[0].line,
			items[0].column
		)

	def eq(self, items: list[lark.Token | lark.Tree | Node]):
		left, right = items
		return EqNode(left, right, left.line, left.column)

	def neq(self, items: list[lark.Token | lark.Tree | Node]):
		left, right = items
		return NeqNode(left, right, left.line, left.column)

	def lt(self, items: list[lark.Token | lark.Tree | Node]):
		left, right = items
		return LtNode(left, right, left.line, left.column)

	def le(self, items: list[lark.Token | lark.Tree | Node]):
		left, right = items
		return LeNode(left, right, left.line, left.column)

	def gt(self, items: list[lark.Token | lark.Tree | Node]):
		left, right = items
		return GtNode(left, right, left.line, left.column)

	def ge(self, items: list[lark.Token | lark.Tree | Node]):
		left, right = items
		return GeNode(left, right, left.line, left.column)

	def band(self, items: list[lark.Token | lark.Tree | Node]):
		left, right = items
		return AndNode(left, right, left.line, left.column)

	def bor(self, items: list[lark.Token | lark.Tree | Node]):
		left, right = items
		return OrNode(left, right, left.line, left.column)

	def bnot(self, items: list[lark.Token | lark.Tree | Node]):
		operand = items[0]
		return NotNode(operand, operand.line, operand.column)

	def assign(self, items: list[lark.Token | lark.Tree | Node]):
		return AssignNode(
			items[0],
			items[1],
			items[0].line,
			items[0].column
		)
	
	def assign_add(self, items: list[lark.Token | lark.Tree | Node]):
		return AssignNode(
			items[0],
			items[1],
			items[0].line,
			items[0].column,
			"+"
		)
	
	def assign_sub(self, items: list[lark.Token | lark.Tree | Node]):
		return AssignNode(
			items[0],
			items[1],
			items[0].line,
			items[0].column,
			"-"
		)
	
	def assign_mul(self, items: list[lark.Token | lark.Tree | Node]):
		return AssignNode(
			items[0],
			items[1],
			items[0].line,
			items[0].column,
			"*"
		)
	
	def assign_div(self, items: list[lark.Token | lark.Tree | Node]):
		return AssignNode(
			items[0],
			items[1],
			items[0].line,
			items[0].column,
			"/"
		)
	
	def while_loop(self, items: list[lark.Token | lark.Tree | Node]):
		return WhileLoopNode(
			items[0],
			items[1:],
			items[0].line,
			items[0].column
		)
	

	def for_loop(self, items: list[lark.Token | lark.Tree | Node]):
		return ForLoopNode(
			[items[0]],
			items[1],
			[items[2]],
			items[3:],
			items[0].line,
			items[0].column
		)
	

	def global_var(self, items: list[lark.Token | lark.Tree | Node]):
		scope, name, dataType, *value = items
		return GlobalVariableNode(
			STORAGE2I[scope.value],
			name.value,
			Type.get(dataType.value),
			value[0] if len(value) > 0 else None,
			scope.line,
			scope.column
		)


class Parser:
	def __init__(self) -> None:
		grammar: str = ""
		with open(resource("parser/grammar.lark"), "r") as f:
			grammar = f.read()
			f.close()
		self.lark = lark.Lark(grammar, propagate_positions=True, start="module")

	def parse(self, src: str) -> ModuleNode:
		return AST().transform(self.lark.parse(src))
