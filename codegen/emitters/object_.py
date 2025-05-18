from codegen.util import I32, I32_0, VOID, PI8, mangle
from parser.ast import ObjectNode, FunctionNode, ir, Type

# TODO: clean
def emit(self, node: ObjectNode):
	struct = ir.global_context.get_identified_type(node.name)
	pstruct = struct.as_pointer()

	_vtable = ir.global_context.get_identified_type(mangle("vtable", node.name))
	_vtable.set_body(*[
		ir.FunctionType(method.returnType[0], [pstruct] + [T for T, _ in method.args.values()], method.vararg).as_pointer()
		for method in node.methods if method.name[0] != "."
	])
	pvtable = _vtable.as_pointer()

	vtable = ir.GlobalVariable(self._module, _vtable, _vtable.name)
	vtable.linkage = "internal"

	fields = {field.name: (field.storage, field.dataType) for field in node.fields}
	struct.set_body(pvtable, I32, *[field.dataType[0] for field in node.fields])

	sizeof = struct.get_abi_size(self._machine.target_data)
	sizeofVtable = _vtable.get_abi_size(self._machine.target_data)

	self._log("Creating object '%s' (%d bytes. VTable: %d bytes). %d method(s), %d field(s))", node.name, sizeof - I32.get_abi_size(self._machine.target_data), sizeofVtable, len(node.methods), len(node.fields))

	def createMethodContext(f: ir.Function, method: FunctionNode, this):
		block = None
		if hasattr(self, "_block"):
			block = self._block.block
		else:
			block = f.append_basic_block("entry")
			self._block = ir.IRBuilder(block)
			
		self._func = f
		self._funcNode = method
		self._args = tuple(method.args.keys())
		self._argsTi = tuple(method.args.values())
		self._this = this

		classLocals = {}

		if this:
			i = 1
			for name, (_, T) in fields.items():
				classLocals[name] = (self._block.gep(this, [I32_0, ir.Constant(I32, i)]), T)
				i += 1

		self._locals = classLocals
		
	def delMethodContext():
		del self._func, self._funcNode, self._block, self._args, self._argsTi, self._locals, self._this

	sig = ir.FunctionType(pstruct, [])
	ctor = ir.Function(self._module, sig, mangle("ctor", node.name, sig))
	self._block = ir.IRBuilder(ctor.append_basic_block("entry"))
	this = self._block.bitcast(self._block.call(self._allocator, [I32(sizeof)]), pstruct)
	self._block.store(I32(1), self._block.gep(this, [I32_0, I32(1)]))
	self._block.store(vtable, self._block.gep(this, [I32_0, I32_0]))
	self._ctors[node.name] = ctor
	del self._block

	sig = ir.FunctionType(VOID, [pstruct])
	dtor = ir.Function(self._module, sig, mangle("dtor", node.name, sig))
	dtor.args[0].name = "this"
	self._block = ir.IRBuilder(dtor.append_basic_block("entry"))
	prefcount = self._block.gep(dtor.args[0], [I32_0, I32(1)])
	refcount = self._block.sub(self._block.load(prefcount), I32(1))
	with self._block.if_then(self._block.icmp_signed("<=", refcount, I32_0)) as then:
		self._block.call(self._deallocator, [self._block.bitcast(dtor.args[0], PI8)])
	self._block.ret_void()
	self._dtors[node.name] = dtor
	del self._block

	methods = {}
	vtableIdx = 0
	for method in node.methods:
		if method.name == ".ctor":
			ctor.ftype.args = tuple(T for T, _ in method.args.values())
			ctor.ftype.vararg = method.vararg
			ctor.args = [ir.Argument(ctor, T, name) for name, (T, _) in method.args.items()]
			self._block = ir.IRBuilder(ctor.blocks[-1])

			createMethodContext(ctor, method, this)
			self._emitBlock(method, method.name)
			if self._block.block.is_terminated:
				self.panic("constructor can not return")
				continue
			delMethodContext()

		elif method.name == ".dtor":
			dtor.ftype.args = [pstruct] + list(T for T, _ in method.args.values())
			dtor.ftype.vararg = method.vararg
			dtor.args = [ir.Argument(dtor, T, name) for name, (T, _) in method.args.items()]
			self._block = ir.IRBuilder(dtor.blocks[-1])

			createMethodContext(dtor, method, this)
			self._emitBlock(method, method.name)
			if self._block.block.is_terminated:
				self.panic("deconstructor can not return")
				continue
			self._block.ret(this)
			delMethodContext()

		else:
			sig = _vtable.elements[vtableIdx].pointee
			f = ir.Function(self._module, sig, mangle("method", node.name, sig, method.name))
			f.args[0].name = "this"
			createMethodContext(f, method, f.args[0])
			self._emitBlock(method, method.name)
			delMethodContext()
			methods[method.name] = (method.storage, f.ftype, vtableIdx, f)
			vtableIdx += 1

	self._block = ir.IRBuilder(ctor.blocks[-1])
	vtable = self._block.load(self._block.gep(this, [I32_0, I32_0], True))
	for name, (_, sig, idx, f) in methods.items():
		self._block.store(f, self._block.gep(vtable, [I32_0, I32(idx)], True))
	self._block.ret(this)
	del self._block

	self._objects[node.name] = (pstruct, this, fields, methods)

__all__ = ["emit"]
