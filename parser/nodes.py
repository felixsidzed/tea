import re

from llvmlite import ir


class Type:
	int_ = 0
	float_ = 1
	double_ = 2
	char_ = 3
	string_ = 4
	void_ = 5
	bool_ = 6
	long_ = 7

	@staticmethod
	def get(name: str) -> tuple[ir.Type, bool]:
		const = "const" in name
		name = name.replace("const", "").strip()

		nptr = name.count("*")
		name = name.replace("*", "").strip()

		base = re.match(r"([a-zA-Z_]+)(\s*(\[[^\]]*\])*)?", name)

		basename = base.group(1)
		array = base.group(2)

		enum = getattr(Type, basename + "_")
		if enum == Type.void_ and nptr > 0:
			enum = Type.char_

		llvm = TYPE2LLVM[enum]

		if array:
			dims = re.findall(r"\[(\d*)\]", array)
			for dim in reversed(dims):
				if dim == "":
					raise ValueError("array size not specified")
				llvm = ir.ArrayType(llvm, int(dim))

		for _ in range(nptr):
			llvm = llvm.as_pointer()

		return (llvm, const)

	@staticmethod
	def register(name: str, value: ir.Type):
		TYPE2LLVM.append(value)
		setattr(Type, name + "_", len(TYPE2LLVM) - 1)


TYPE2LLVM = [ir.IntType(32), ir.FloatType(), ir.DoubleType(), ir.IntType(8), ir.IntType(8).as_pointer(), ir.VoidType(), ir.IntType(1), ir.IntType(64)]
STORAGE2I = {
	"public": 1,
	"private": 0
}


class Node:
	def __init__(self, line: int, column: int):
		self.line: int = line
		self.column: int = column


class UsingNode(Node):
	def __init__(self, name: str, path: str, line: int, column: int):
		self.name = name
		self.path = path
		super().__init__(line, column)


class FunctionNode(Node):
	def __init__(self, storage: int, conv: str, name: str, returnType: tuple[ir.Type, bool], args: dict[str: tuple[ir.Type, bool]], vararg: bool, body: list[Node], line: int, column: int):
		self.storage = storage
		self.conv = conv
		self.name = name
		self.returnType = returnType
		self.args = args
		self.vararg = vararg
		self.body = body
		super().__init__(line, column)


class CallNode(Node):
	def __init__(self, name: str, args: list, scope: list[str] | None, function: FunctionNode | None, line: int, column: int):
		self.name = name
		self.args = args
		self.scope = scope or []
		self.function = function
		super().__init__(line, column)


class ReturnNode(Node):
	def __init__(self, value, _type: list[int, bool, bool, bool], line: int, column: int):
		self.value = value
		self._type = _type
		super().__init__(line, column)


class ModuleNode(Node):
	def __init__(self, line: int, column: int, body, name: str):
		super().__init__(line, column)
		self.body = body
		self.name = name


class ExpressionNode(Node):
	def __init__(self, line: int, column: int, value=None):
		self.value = value
		super().__init__(line, column)


class AddNode(ExpressionNode):
	def __init__(self, line: int, column: int, left: ExpressionNode, right: ExpressionNode):
		super().__init__(line, column)
		self.left = left
		self.right = right


class SubNode(ExpressionNode):
	def __init__(self, line: int, column: int, left: ExpressionNode, right: ExpressionNode):
		super().__init__(line, column)
		self.left = left
		self.right = right


class MulNode(ExpressionNode):
	def __init__(self, line: int, column: int, left: ExpressionNode, right: ExpressionNode):
		super().__init__(line, column)
		self.left = left
		self.right = right


class DivNode(ExpressionNode):
	def __init__(self, line: int, column: int, left: ExpressionNode, right: ExpressionNode):
		super().__init__(line, column)
		self.left = left
		self.right = right


class VariableNode(Node):
	def __init__(self, name: str, dataType: int, value, line: int, column: int):
		self.name = name
		self.dataType = dataType
		self.value = value
		super().__init__(line, column)


class FunctionImportNode(Node):
	def __init__(self, conv: str, name: str, returnType: list[int, bool, bool, bool], args: dict[str: int], vararg: bool, line: int, column: int):
		self.conv = conv
		self.name = name
		self.returnType = returnType
		self.args = args
		self.vararg = vararg
		super().__init__(line, column)


class ElseNode(Node):
	def __init__(self, body: list[Node], line: int, column: int):
		self.body = body
		super().__init__(line, column)


class ElseIfNode(Node):
	def __init__(self, condition: Node, body: list[Node], line: int, column: int):
		self.body = body
		self.condition = condition
		super().__init__(line, column)


class IfNode(Node):
	def __init__(self, condition: Node, body: list[Node], line: int, column: int, else_: ElseNode = None, elseIf: list[ElseIfNode] = None):
		self.condition = condition
		self.body = body
		self.else_ = else_
		self.elseIf = elseIf or []
		super().__init__(line, column)


class EqNode(ExpressionNode):
	def __init__(self, left: ExpressionNode, right: ExpressionNode, line: int, column: int):
		self.left = left
		self.right = right
		super().__init__(line, column)


class NeqNode(ExpressionNode):
	def __init__(self, left: ExpressionNode, right: ExpressionNode, line: int, column: int):
		self.left = left
		self.right = right
		super().__init__(line, column)


class LtNode(ExpressionNode):
	def __init__(self, left: ExpressionNode, right: ExpressionNode, line: int, column: int):
		self.left = left
		self.right = right
		super().__init__(line, column)


class LeNode(ExpressionNode):
	def __init__(self, left: ExpressionNode, right: ExpressionNode, line: int, column: int):
		self.left = left
		self.right = right
		super().__init__(line, column)


class GtNode(ExpressionNode):
	def __init__(self, left: ExpressionNode, right: ExpressionNode, line: int, column: int):
		self.left = left
		self.right = right
		super().__init__(line, column)


class GeNode(ExpressionNode):
	def __init__(self, left: ExpressionNode, right: ExpressionNode, line: int, column: int):
		self.left = left
		self.right = right
		super().__init__(line, column)


class AndNode(ExpressionNode):
	def __init__(self, left: ExpressionNode, right: ExpressionNode, line: int, column: int):
		self.left = left
		self.right = right
		super().__init__(line, column)


class OrNode(ExpressionNode):
	def __init__(self, left: ExpressionNode, right: ExpressionNode, line: int, column: int):
		self.left = left
		self.right = right
		super().__init__(line, column)


class NotNode(ExpressionNode):
	def __init__(self, operand: ExpressionNode, line: int, column: int):
		super().__init__(line, column, operand)


class AssignNode(Node):
	def __init__(self, lhs: Node, rhs: Node, line: int, column: int, extra: str | None = None):
		self.lhs = lhs
		self.rhs = rhs
		self.extra = extra
		super().__init__(line, column)


class WhileLoopNode(Node):
	def __init__(self, condition: Node, body: list[Node], line: int, column: int):
		self.condition = condition
		self.body = body
		super().__init__(line, column)


class ForLoopNode(Node):
	def __init__(self, variables: list[VariableNode], condition: Node, steps: list[Node], body: list[Node], line: int, column: int):
		self.variables = variables
		self.condition = condition
		self.steps = steps
		self.body = body
		super().__init__(line, column)


class GlobalVariableNode(Node):
	def __init__(self, storage: int, name: str, dataType: int, value, line: int, column: int):
		self.storage = storage
		self.name = name
		self.dataType = dataType
		self.value = value
		super().__init__(line, column)


class BreakNode(Node):
	def __init__(self, line: int, column: int):
		super().__init__(line, column)


class ContinueNode(Node):
	def __init__(self, line: int, column: int):
		super().__init__(line, column)


class CastNode(ExpressionNode):
	def __init__(self, type_, value: ExpressionNode, line: int, column: int):
		self.type_ = type_
		super().__init__(line, column, value)


class IndexNode(ExpressionNode):
	def __init__(self, kind: int, arr: ExpressionNode, value: int, line: int, column: int):
		self.kind = kind
		self.arr = arr
		super().__init__(line, column, value)


class ArrayNode(ExpressionNode):
	def __init__(self, elements: list[ExpressionNode], line: int, column: int):
		super().__init__(line, column, elements)


class FieldNode(Node):
	def __init__(self, storage: int, name: str, dataType: tuple[ir.Type, bool], line: int, column: int):
		self.storage = storage
		self.name = name
		self.dataType = dataType
		super().__init__(line, column)


class ObjectNode(Node):
	def __init__(self, name: str, body: list[FunctionNode | FieldNode], line: int, column: int):
		self.name = name
		self.fields = tuple(filter(lambda x: type(x) == FieldNode, body))
		self.methods = tuple(filter(lambda x: type(x) == FunctionNode, body))
		super().__init__(line, column)


class NewNode(ExpressionNode):
	def __init__(self, name: str, args: list[ExpressionNode], line: int, column: int):
		self.args = args
		super().__init__(line, column, name)


class MethodCallNode(Node):
	def __init__(self, value: str, name: str, args: list, line: int, column: int):
		self.value = value
		self.name = name
		self.args = args
		super().__init__(line, column)
