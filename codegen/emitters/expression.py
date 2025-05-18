from parser.nodes import *
from codegen.util import *
from lark import Token as LarkToken

def emit(self, node: ExpressionNode, const: bool = False):
	# we use some tricks to minimize the amount of duplicate code
	if type(node) == LarkToken:
		if node.type not in ("IDENTF", "STRING", "REF", "DEREF"):
			T = Type.get(node.type)[0]
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
						return (var.type, self._block.load(var))
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
				type_ = ir.ArrayType(ir.IntType(8), len(value))
				
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
				ptr = self._block.bitcast(arg, ir.PointerType(arg.type))
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

	elif type(node) == CallNode:
		if const:
			self.panic("'CALL' can not be a constant expression")

		return self._emitCall(node)

	elif isinstance(node, ExpressionNode):
		if type(node) in (AddNode, MulNode, SubNode, DivNode):
			lhs = self._emitExpression(node.left, const)
			rhs = self._emitExpression(node.right, const)
			return (lhs[0], getattr(self._block, type(node).__name__.lower()[:-4].replace("div", "sdiv"))(lhs[1], rhs[1]))
		
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
		
		elif type(node) == AndNode:
			lhs = self._emitExpression(node.left, const)
			rhs = self._emitExpression(node.right, const)
			return (I1, self._block.and_(lhs[1], rhs[1]))
		
		elif type(node) == OrNode:
			lhs = self._emitExpression(node.left, const)
			rhs = self._emitExpression(node.right, const)
			return (I1, self._block.or_(lhs[1], rhs[1]))
		
		elif type(node) == CastNode:
			type_, value = self._emitExpression(node.value)
			value, success = cast(self._block, node.type_[0], value)
			if not success:
				self.panic("unable to cast '%s' to '%s'. line %d, column %d", type_, node.type_[0], node.line, node.column)
			return (node.type_[0], value)
		
		elif type(node) == IndexNode:
			if node.kind == 0: # array index
				itype, index = self._emitExpression(node.value)
				type_, arr = self._emitExpression(LarkToken("REF", node.arr, node.line, node.column))
				if str(itype)[0] != "i":
					self.panic("'%s' is not a valid index", itype)
				if type(type_.pointee) != ir.ArrayType:
					self.panic("'%s' is not an array type. line %d, column %d", type_, node.line, node.column)
				if node.value.value >= type_.pointee.count:
					self.panic("index out of bounds. line %d, column %d", node.line, node.column)
				return (type_.pointee.element, self._block.load(self._block.gep(arr, [I32_0, index])))
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
					return (field[1][1], self._block.load(self._block.gep(this, [I32_0, ir.Constant(I32, idx + 1)])))
				return self.panic("'%s' is not a valid member of object '%s'. line %d, column %d", node.value, this.pointee, node.line, node.column)
		
		elif type(node) == ArrayNode:
			if const:
				self.panic("array creation can't be a constant expression. line %d, column %d", node.line, node.column)

			elems = [self._emitExpression(elem, const=False)[1] for elem in node.value]

			# ts should never happen
			if not elems:
				self.panic("can't create an empty array")

			elementType = elems[0].type
			for elem in elems:
				if elem.type != elementType:
					self.panic("invalid element type: expected '%s', got '%s'. line %d, column %d", elementType, elem.type, node.line, node. column)

			allocated = self._block.alloca(ir.ArrayType(elementType, len(elems)))
			for i, val in enumerate(elems):
				self._block.store(val, self._block.gep(allocated, [I32_0, ir.Constant(I32, i)]))

			return (allocated.allocated_type, allocated)

		elif type(node) == NewNode:
			obj = self._objects.get(node.value)
			if obj is None:
				return self.panic("reference to undefined object '%s' in expression. line %d, column %d", node.value, node.line, node.column)

			# TODO: type check args
			args = [self._emitExpression(arg)[1] for arg in node.args]
			return (obj[0], self._block.call(self._ctors[node.value], args))

		else:
			self.panic("whoopsies")

	else:
		self.panic("invalid statement '%s' in expression", type(node).__name__[:-4])

__all__ = ["emit"]
