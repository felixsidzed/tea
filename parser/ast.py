import pathlib

import lark
from parser.nodes import *

MODULE_LOOKUP = [
	"stdlib",
	"."
]

def resolveImport(path: pathlib.Path):
	if path.suffix != ".tea":
		path = path.with_suffix(".tea")
	for directory in MODULE_LOOKUP:
		fullPath = pathlib.Path(directory) / path
		if fullPath.exists():
			return fullPath
	return None


class AST(lark.Transformer):
	def __init__(self, filename: str | None = None):
		self.filename = filename or "[module]"
		self._functions = {}
		self._macros = {}

	def module(self, children):
		return ModuleNode(1, 1, children, self.filename)

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
		return self._macros.get(token.value, token)

	def CHAR(self, token: lark.Token):
		return lark.Token("CHAR", ord(token.value[1:-1].encode("utf-8").decode("unicode_escape")), line=token.line, column=token.column)

	def function(self, items: list[lark.Token | lark.Tree | Node]):
		storageType = items[0].value
		name = items[1]
		args = items[2].children
		returnType = items[3]
		body = items[4:]

		vararg = False
		fixedArgs = {}
		for arg in args:
			if arg.children[1].type == "VARARG":
				vararg = True
			else:
				fixedArgs[arg.children[1].value] = Type.get(arg.children[0].value)

		node = FunctionNode(
			STORAGE2I[storageType],
			"__cdecl",
			name.value,
			Type.get(returnType) if returnType is not None else returnType,
			fixedArgs,
			vararg,
			body,
			name.line,
			name.column
		)
		self._functions[name] = node
		return node

	def function2(self, items: list[lark.Token | lark.Tree | Node]):
		node = self.function([items[0], items[2], items[3], items[4]] + items[5:])
		node.conv = items[1].value
		return node

	def function_import(self, items: list[lark.Token | lark.Tree | Node]):
		conv = items[0].value
		name = items[1]
		args = items[2].children
		returnType = items[3]

		vararg = False
		fixedArgs = {}
		for arg in args:
			if arg.children[0].type == "VARARG":
				vararg = True
			else:
				fixedArgs[arg.children[1].value] = Type.get(arg.children[0].value)

		node = FunctionImportNode(
			conv,
			name.value,
			Type.get(returnType),
			fixedArgs,
			vararg,
			name.line,
			name.column
		)
		return node

	def function3(self, items: list[lark.Token | lark.Tree | Node]):
		return self.function([items[0], items[1], items[2], None] + items[3:])

	def function4(self, items: list[lark.Token | lark.Tree | Node]):
		node = self.function3([items[0], items[2], items[3]] + items[4:])
		node.conv = items[1].value
		return node

	def using(self, items: list[lark.Token | lark.Tree | Node]):
		path = resolveImport(pathlib.Path(items[0].value))
		if path is None:
			raise Exception(f"Failed to resolve module '{items[0].value}'")

		return UsingNode(
			path.name.split(".")[0] if path is not None else items[0].value,
			path.resolve() if path is not None else path,
			items[0].line,
			items[0].column,
		)

	def direct_call(self, items: list[lark.Token | lark.Tree | Node]):
		callee = items[0]
		args = items[1] if len(items) > 1 else []

		return CallNode(
			callee,
			[arg.children[0] for arg in args.children],
			[],
			self._functions.get(callee, None),
			callee.line,
			callee.column
		)

	def scoped_call(self, items: list[lark.Token | lark.Tree | Node]):
		*scope, name, args = items
		scope = [tok.value for tok in scope]

		return CallNode(
			name.value,
			[arg.children[0] for arg in args.children],
			scope,
			None,
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
			return ReturnNode(None, Type.void_, value.line, value.column)
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

	def variable2(self, items: list[lark.Token | lark.Tree | Node]):
		return VariableNode(
			items[0].value,
			None,
			items[1],
			items[0].line,
			items[0].column
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

	def reference(self, items: list[lark.Token | lark.Tree | Node]):
		return lark.Token("REF", items[0], line=items[0].line, column=items[0].column)

	def dereference(self, items: list[lark.Token | lark.Tree | Node]):
		return lark.Token("DEREF", items[0], line=items[0].line, column=items[0].column)

	def break_(self, items: list[lark.Token | lark.Tree | Node]):
		return BreakNode(items[0].line, items[0].column)

	def continue_(self, items: list[lark.Token | lark.Tree | Node]):
		return ContinueNode(items[0].line, items[0].column)

	def cast(self, items: list[lark.Token | lark.Tree | Node]):
		return CastNode(Type.get(items[0]), items[1], items[0].line, items[0].column)

	def macro(self, items: list[lark.Token | lark.Tree | Node]):
		self._macros[items[0].value] = items[1]
		return lark.Discard

	def array(self, items: list[lark.Token | lark.Tree | Node]):
		return ArrayNode(items[1:], items[0].line, items[0].column)

	def index(self, items: list[lark.Token | lark.Tree | Node]):
		return IndexNode(0, items[0], items[1], items[0].line, items[0].column)

	def object_field(self, items: list[lark.Token | lark.Tree | Node]):
		return FieldNode(
			STORAGE2I[items[0].value],
			items[1],
			Type.get(items[2].value),
			items[0].line,
			items[0].column,
		)

	def object_method(self, items: list[lark.Token | lark.Tree | Node]):
		if type(items[0]) == FunctionNode:
			return items[0]
		else:
			args = items[1].children

			vararg = False
			fixedArgs = {}
			for arg in args:
				if arg.children[1].type == "VARARG":
					vararg = True
				else:
					fixedArgs[arg.children[1].value] = Type.get(arg.children[0].value)
			return FunctionNode(
				STORAGE2I["public"],
				"__cdecl",
				".ctor" if items[0].type == "CTOR" else ".dtor",
				(TYPE2LLVM[Type.void_], False),
				fixedArgs,
				vararg,
				items[2:],
				items[0].line,
				items[0].column
			)

	def object_(self, items: list[lark.Token | lark.Tree | Node]):
		node = ObjectNode(
			items[0].value,
			items[1:],
			items[0].line,
			items[0].column,
		)
		struct = ir.global_context.get_identified_type(node.name)
		Type.register(node.name, struct.as_pointer())
		return node
	
	def object_new(self, items: list[lark.Token | lark.Tree | Node]):
		return NewNode(
			items[0].value,
			[arg.children[0] for arg in items[1].children],
			items[0].line,
			items[0].column,
		)
	
	def object_index(self, items: list[lark.Token | lark.Tree | Node]):
		return IndexNode(1, items[0], items[1], items[0].line, items[0].column)
	
	def method_call(self, items: list[lark.Token | lark.Tree | Node]):
		return MethodCallNode(
			items[0],
			items[1],
			[arg.children[0] for arg in items[2].children],
			items[0].line,
			items[1].column
		)
	
	def arrow_index(self, items: list[lark.Token | lark.Tree | Node]):
		return IndexNode(1, lark.Token("DEREF", items[0]), items[1], items[0].line, items[0].column)
	
	def arrow_call(self, items: list[lark.Token | lark.Tree | Node]):
		return MethodCallNode(
			lark.Token("DEREF", items[0]),
			items[1],
			[arg.children[0] for arg in items[2].children],
			items[0].line,
			items[1].column
		)
	
	def object_method_import(self, items: list[lark.Token | lark.Tree | Node]):
		if items[0].type == "STORAGE_TYPE":
			args = items[2].children

			vararg = False
			fixedArgs = {}
			for arg in args:
				if arg.children[0].type == "VARARG":
					vararg = True
				else:
					fixedArgs[arg.children[1].value] = Type.get(arg.children[0].value)
			return MethodImportNode(
				STORAGE2I[items[0].value],
				"__cdecl",
				items[1],
				Type.get(items[3].value),
				fixedArgs,
				vararg,
				items[0].line,
				items[0].column,
			)
		else:
			args = items[1].children

			vararg = False
			fixedArgs = {}
			for arg in args:
				if arg.children[1].type == "VARARG":
					vararg = True
				else:
					fixedArgs[arg.children[1].value] = Type.get(arg.children[0].value)
			return MethodImportNode(
				STORAGE2I["public"],
				"__cdecl",
				".ctor",
				(TYPE2LLVM[Type.void_], False),
				fixedArgs,
				vararg,
				items[0].line,
				items[0].column
			)
	
	def object_import(self, items: list[lark.Token | lark.Tree | Node]):
		node = ObjectImportNode(
			items[0].value,
			items[1:],
			items[0].line,
			items[0].column,
		)
		struct = ir.global_context.get_identified_type(node.name)
		Type.register(node.name, struct.as_pointer())
		return node
	
	def object_define(self, items: list[lark.Token | lark.Tree | Node]):
		if not hasattr(Type, f"{items[1].value}_"):
			struct = ir.global_context.get_identified_type(items[1].value)
			Type.register(items[1].value, struct.as_pointer())
		return lark.Discard
