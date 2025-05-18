import copy

from lark import Token as LarkToken

from parser.nodes import *
from codegen.util import *

def emit(self, node: ExpressionNode, const: bool = False):
	# * = we use some tricks here to minimize the amount of duplicate code
	if type(node) == LarkToken:
		# *
		if node.type not in ("IDENTF", "STRING", "REF", "DEREF"):
			T = Type.get(node.type.lower())[0]
			return (T, ir.Constant(T, node.value))
		
		elif node.type == "IDENTF":
			if const:
				self.panic("'IDENTF' can not be a constant expression")

			if node.value in self._args:
				arg = self._func.args[self._args.index(node.value)]
				return (arg.type, arg)
			elif node.value in self._locals:
				local = self._locals[node.value]
				return (local[1][0], self._block.load(local[0]))
			else:
				try:
					var = self._module.get_global(node.value)
					if type(var) == ir.GlobalVariable:
						return (var.value_type, self._block.load(var))
					elif isinstance(var, ir.Function):
						return (var.function_type, var)
					else:
						raise KeyError
				except KeyError:
					self.panic("undefined reference to '%s' in expression", node.value)

		elif node.type == "STRING":
			name = str2name(node.value)
			g = self._module.globals.get(name)
			if g is None:
				value = bytearray(node.value.encode("utf8")) + b"\00"
				type_ = ir.ArrayType(I8, len(value))
				
				g = ir.GlobalVariable(self._module, type_, name)
				g.linkage = "internal"
				g.global_constant = True
				g.initializer = ir.Constant(type_, value)
			
			return (PI8, self._block.bitcast(g, PI8))
		
		elif node.type == "REF":
			if const:
				self.panic("'REFERENCE' can not be a constant expression")

			if node.value in self._args:
				arg = self._func.args[self._args.index(node.value)]
				ptr = self._block.bitcast(arg, arg.type.as_pointer())
				return (ptr.type, ptr)
			
			elif node.value in self._locals:
				local = self._locals[node.value]
				return (local[0].type, local[0])
			
			else:
				try:
					var = self._module.get_global(node.value)
					if isinstance(var, ir.GlobalVariable):
						return (var.type, var)
					elif isinstance(var, ir.Function):
						return (var.function_type, var)
					else:
						raise KeyError
				except KeyError:
					self.panic("undefined reference to '%s' in expression", node.value)

		elif node.type == "DEREF":
			if const:
				self.panic("'DEREFERENCE' can not be a constant expression")

			type_, val = self._emitExpression(node.value, const=False)

			if not type_.is_pointer:
				self.panic("cannot dereference non-pointer type '%s'", type_)

			val = self._block.load(val)
			return (type_.pointee, val)
		
		else:
			self.panic("invalid constant '%s' in expression", node)

	elif type(node) == CallNode or type(node) == MethodCallNode:
		if const:
			self.panic("'CALL' can not be a constant expression")

		return self._emitCall(node)

	elif isinstance(node, ExpressionNode):
		# *
		if type(node) in (AddNode, MulNode, SubNode, DivNode):
			lhs = self._emitExpression(node.left, const)
			rhs = self._emitExpression(node.right, const)
			
			if type(node) in (AddNode, SubNode):
				if lhs[0].is_pointer and type(rhs[0]) == I32:
					scaled = self._block.mul(rhs[1], I32(lhs[0].pointee.get_abi_size(self._machine.target_data)))

					if type(node) == AddNode:
						return (lhs[0], self._block.gep(lhs[1], [scaled]))
					else:
						return (lhs[0], self._block.gep(lhs[1], [self._block.neg(scaled)]))

				elif rhs[0].is_pointer and type(lhs[0]) == I32:
					scaled = self._block.mul(rhs[1], I32(lhs[0].pointee.get_abi_size(self._machine.target_data)))

					if type(node) == AddNode:
						return (rhs[0], self._block.gep(rhs[1], [scaled]))
					else:
						self.panic("invalid operation. line %d, column %d", node.line, node.column)

			return (lhs[0], getattr(self._block, type(node).__name__.lower()[:-4].replace("div", "sdiv"))(lhs[1], rhs[1]))
		
		# *
		elif type(node) in (EqNode, NeqNode, LtNode, LeNode, GtNode, GeNode):
			lhs = self._emitExpression(node.left, const)
			rhs = self._emitExpression(node.right, const)
			cmpop = NNAME2CMPOP[type(node).__name__[:-4]]
			if lhs[0] in (F32, F64):
				return (I1, self._block.fcmp_ordered(cmpop, lhs[1], rhs[1]))
			elif lhs[0] in (I32, I8, I64):
				return (I1, self._block.icmp_signed(cmpop, lhs[1], rhs[1]))
			else:
				self.panic("invalid lhs type '%s' in expression. line %d, column %d", lhs[0], node.line, node.column)
		
		elif type(node) == NotNode:
			type_, value = self._emitExpression(node.value)
			value, _ = cast(self._block, I1, value)
			return (type_, self._block.not_(value))
		
		elif type(node) in (AndNode, OrNode):
			lhs = self._emitExpression(node.left, const)
			rhs = self._emitExpression(node.right, const)
			return (I1, getattr(self._block, type(node).__name__.lower()[:-4] + "_")(lhs[1], rhs[1]))
		
		elif type(node) == CastNode:
			type_, value = self._emitExpression(node.value)
			value, success = cast(self._block, node.type_[0], value)
			if not success:
				self.panic("unable to cast '%s' to '%s'. line %d, column %d", type_, node.type_[0], node.line, node.column)
			return (node.type_[0], value)
		
		elif type(node) == IndexNode:
			if node.kind == 0: # array index
				itype, index = self._emitExpression(node.value)
				oldBlock = copy.copy(self._block)
				type_, arr = self._emitExpression(node.arr)

				if type(itype) != ir.IntType:
					self.panic("'%s' is not a valid index", itype)

				if type(type_) == ir.ArrayType:
					element = type_.element
					self._block = oldBlock
					type_, arr = self._emitExpression(LarkToken("REF", node.arr, line=node.line, column=node.column))
					return (element, self._block.load(self._block.gep(arr, [I32_0, index])))
				else:
					element = getElementType(type_)
					if element is None:
						self.panic("'%s' is not an indexable type", type_)
					return (element, self._block.load(self._block.gep(arr, [index])))

			elif node.kind == 1: # object index
				type_, this = self._emitExpression(node.arr)
				for objName, obj in self._objects.items():
					if obj[1].type == this.type: break
				else:
					return self.panic("reference to undefined object '%s' in expression. line %d, column %d", node.value, node.line, node.column)
				
				field = obj[2].get(node.value.value)
				if field:
					if field[0] == STORAGE_PRIVATE:
						return self.panic("storage type violation. line %d, column %d", node.line, node.column)
					idx = tuple(obj[2].keys()).index(node.value.value)
					return (field[1][0], self._block.load(self._block.gep(this, [I32_0, ir.Constant(I32, idx + 1)])))
				return self.panic("'%s' is not a valid member of object '%s'. line %d, column %d", node.value, this.pointee, node.line, node.column)
		
		elif type(node) == ArrayNode:
			elems = [self._emitExpression(elem, const)[1] for elem in node.value]

			if not elems:
				self.panic("can't create an empty array")

			elementType = elems[0].type
			for elem in elems:
				if elem.type != elementType:
					self.panic(
						"invalid element type: expected '%s', got '%s'. line %d, column %d",
						elementType, elem.type, node.line, node.column
					)

			arrType = ir.ArrayType(elementType, len(elems))
			constArray = ir.Constant(arrType, elems)

			if const:
				return (arrType, constArray)
			
			allocated = self._block.alloca(arrType)
			self._block.store(constArray, allocated)

			return (allocated.allocated_type, allocated)

		elif type(node) == NewNode:
			obj = self._objects.get(node.value)
			if obj is None:
				return self.panic("reference to undefined object '%s' in expression. line %d, column %d", node.value, node.line, node.column)

			# TODO: type check args
			args = [self._emitExpression(arg)[1] for arg in node.args]
			return (obj[0], self._block.call(self._ctors[node.value], args))

		else:
			self.panic("invalid statement '%s' in expression", type(node).__name__[:-4])

	else:
		self.panic("invalid statement '%s' in expression", type(node).__name__[:-4])

__all__ = ["emit"]
