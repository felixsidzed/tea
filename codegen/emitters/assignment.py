from lark import Token as LarkToken
from parser.ast import AssignNode, ir, IndexNode
from codegen.util import I32, I32_0, STORAGE_PRIVATE

def emit(self, node: AssignNode):
	def _emitLhs(val):
		if type(val) == LarkToken and val.type == "DEREF":
			old = val
			val = _emitLhs(val.value)
			if type(old.value) == IndexNode:
				return (
					self._block.load(val[0]),
					(val[0].type.pointee.pointee, val[1][1])
				)
			elif old in self._locals:
				return (
					self._block.load(val[0]),
					(val[0].allocated_type.pointee, val[1][1])
				)
			else:
				return (
					val[0],
					(val[0].type.pointee, val[1][1])
				)
			
		elif val in self._locals:
			return self._locals[val]
		
		elif val in self._module.globals:
			val: ir.GlobalVariable = self._module.get_global(val)
			return (val, (val.type.pointee, val.global_constant))
		
		elif val in self._args:
			idx = self._args.index(val)
			arg = self._func.args[idx]
			ti = self._argsTi[idx]
			return (arg, ti)
		
		elif type(val) == IndexNode and val.kind == 1:
			type_, this = self._emitExpression(val.arr)
			for objName, obj in self._objects.items():
				if obj[1].type == type_: break
			else:
				return self.panic("'%s' is not an object. line %d, column %d", val.value, val.line, val.column)
			
			field = obj[2].get(val.value.value)
			if field:
				if field[0] == STORAGE_PRIVATE:
					self.panic("storage type violation. line %d, column %d", val.line, val.column)
				else:
					idx = tuple(obj[2].keys()).index(val.value.value)
					return (self._block.gep(this, [I32_0, I32(idx + 2)]), field[1])
			self.panic("'%s' is not a valid member of object '%s'. line %d, column %d", val.value, objName, val.line, val.column)
		
		else:
			self.panic("invalid lhs operator '%s' in assignment. line %d, column %d", node.lhs, node.line, node.column)
	lhs = _emitLhs(node.lhs)
	
	if lhs[1][1]:
		self.panic("assignment to a constant variable '%s'. line %d, column %d", node.lhs, node.line, node.column)
		return
	rhs = list(self._emitExpression(node.rhs))
	if rhs[0] != lhs[1][0]:
		self.panic("value type (%s) does not match variable type (%s). line %d, column %d", rhs[0], lhs[1][0], node.line, node.column)
		return
	
	if node.extra: val = self._block.load(lhs[0])
	if node.extra == "+": rhs[1] = self._block.add(val, rhs[1])
	elif node.extra == "-": rhs[1] = self._block.sub(val, rhs[1])
	elif node.extra == "*": rhs[1] = self._block.mul(val, rhs[1])
	elif node.extra == "/": rhs[1] = self._block.sdiv(val, rhs[1])
	elif node.extra is not None:
		self.panic("invalid operator '%s' in assignment. line %d, column %d", node.extra, node.line, node.column)
		return

	self._block.store(rhs[1], lhs[0])

__all__ = ["emit"]
