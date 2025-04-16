import ctypes


class Type:
	INT = 0
	FLOAT = 1
	DOUBLE = 2
	CHAR = 3
	STRING = 4
	VOID = 5
	BOOL = 6

	@staticmethod
	# typeIndex, isPointer, isConstant
	def get(name: str) -> list[int, bool, bool, bool]:
		return [
			getattr(Type, name.upper().replace("*", "").replace("const ", ""), None),
			name.find("*") != -1,
			name.find("const") != -1,
		]


index2C = [ctypes.c_int, ctypes.c_float, ctypes.c_double,
           ctypes.c_char, ctypes.c_char_p, None, ctypes.c_bool]


class Type2CConverter:
	def __getitem__(self, idx: int | tuple[int, bool, bool, bool]):
		if type(idx) == int:
			return index2C[idx]
		else:
			t = index2C[idx]
			if idx[1]:
				return ctypes.pointer(t)
			else:
				return t


TYPE2C = Type2CConverter()
STORAGE2I = {
	"public": 1,
	"private": 0
}


class Node:
	def __init__(self, line: int, column: int):
		self.line: int = line
		self.column: int = column


class UsingNode(Node):
	def __init__(self, storage: int, name: str, path: str, line: int, column: int):
		self.storage = storage
		self.name = name
		self.path = path
		super().__init__(line, column)


class FunctionNode(Node):
	def __init__(self, storage: int, name: str, returnType: list[int, bool, bool, bool], args: dict[str: int], body: list[Node], line: int, column: int):
		self.storage = storage
		self.name = name
		self.returnType = returnType
		self.args = args
		self.body = body
		super().__init__(line, column)


class CallNode(Node):
	def __init__(self, name: str, args: list, scope: list[str] | None, line: int, column: int):
		self.name = name
		self.args = args
		self.scope = scope or []
		super().__init__(line, column)


class ReturnNode(Node):
	def __init__(self, value, _type: list[int, bool, bool, bool], line: int, column: int):
		self.value = value
		self._type = _type
		super().__init__(line, column)


class ModuleNode(Node):
	def __init__(self, line: int, column: int, body):
		super().__init__(line, column)
		self.body = body


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
