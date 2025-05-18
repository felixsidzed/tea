from parser.ast import VariableNode, ir
from codegen.util import I32, I32_0, cast

def emit(self, node: VariableNode):
	if self._locals.get(node):
		return self.panic("local '%s' is already defined", node.name)

	self._log("Emitting local '%s' of type '%s' (initialized = %s)", node.name, (node.dataType or ("Unknown",))[0], node.value is not None)

	alloca = None
	if node.dataType is not None:
		expected = node.dataType[0]
		# TODO: find a better way to fix possible rsp overflow
		alloca = self._preallocated.get(id(node))
		if alloca is None:
			alloca = self._block.alloca(expected, name=node.name)

	if node.value is not None:
		type_, value = self._emitExpression(node.value)

		if type(type_) == ir.ArrayType:
			if node.dataType is None or node.dataType[0] == type_:
				node.dataType = (type_, False)	
				self._log("Guessed type for variable '%s': %s", node.name, type_)
				self._locals[node.name] = (value, (type_, False))
				return
			
			if type(node.dataType[0]) == ir.ArrayType:
				alloca = self._block.alloca(node.dataType[0])
				for i in range(type_.count):
					idx = I32(i)
					self._block.store(
						self._block.load(self._block.gep(value, [I32_0, idx])),
						self._block.gep(alloca, [I32_0, idx])
					)
				self._locals[node.name] = (alloca, node.dataType)
				return
		
		if node.dataType is None:
			node.dataType = (type_, False)
			alloca = self._block.alloca(type_, name=node.name)
			self._log("Guessed type for variable '%s': %s", node.name, type_)
		else:
			value, success = cast(self._block, expected, value)
			if not success:
				self.panic("variable value type (%s) is incompatible with variable type (%s). line %d, column %d", str(expected), str(type_), node.line, node.column)
				return

		self._block.store(value, alloca)
	self._locals[node.name] = (alloca, node.dataType)

__all__ = ["emit"]
	