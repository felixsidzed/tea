from codegen.util import I32, I32_0, VOID, PI8, I8, mangle
from parser.ast import ObjectNode, FunctionNode, ir

# TODO: clean
def emit(self, node: ObjectNode):
	struct = ir.global_context.get_identified_type(node.name)
	pstruct = struct.as_pointer()

	vtable_t = ir.global_context.get_identified_type(mangle("vtable", node.name))
	vtable_t.set_body(ir.FunctionType(VOID, [pstruct]).as_pointer(), *[
		ir.FunctionType(method.returnType[0], [pstruct] + [T for T, _ in method.args.values()], method.vararg).as_pointer()
		for method in node.methods if method.name[0] != "."
	])
	pvtable = vtable_t.as_pointer()

	vtable = ir.GlobalVariable(self._module, vtable_t, vtable_t.name)
	vtable.linkage = "internal"

	fields = {field.name: (field.storage, field.dataType) for field in node.fields}
	struct.set_body(pvtable, I32, *[field.dataType[0] for field in node.fields])

	sizeof = struct.get_abi_size(self._machine.target_data)
	sizeofVtable = vtable_t.get_abi_size(self._machine.target_data)

	self._log("Creating object '%s' (%d bytes. VTable: %d bytes). %d method(s), %d field(s)", node.name, sizeof - I32.get_abi_size(self._machine.target_data), sizeofVtable, len(node.methods), len(node.fields))

	def createMethodContext(f: ir.Function, method: FunctionNode, this, ctor = False):
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
		if not ctor:
			self._args = ("@",) + self._args
			self._argsTi = ((),) + self._argsTi
		self._this = this
		
	def delMethodContext():
		del self._func, self._funcNode, self._block, self._args, self._argsTi, self._locals, self._this

	sig = vtable_t.elements[0].pointee
	dtor = ir.Function(self._module, sig, mangle("dtor", node.name, sig))

	sig = ir.FunctionType(pstruct, [])
	ctor = ir.Function(self._module, sig, mangle("ctor", node.name, sig))
	self._block = ir.IRBuilder(ctor.append_basic_block("entry"))
	this = self._block.bitcast(self._block.call(self._allocator, [I32(sizeof)]), pstruct)
	self._block.store(I32_0, self._block.gep(this, [I32_0, I32(1)], True))
	self._block.store(dtor, self._block.gep(vtable, [I32_0, I32_0], True))
	self._block.store(vtable, self._block.gep(this, [I32_0, I32_0], True))
	self._ctors[node.name] = ctor
	del self._block

	dtor.args[0].name = "this"
	self._block = ir.IRBuilder(dtor.append_basic_block("entry"))
	prefcount = self._block.gep(dtor.args[0], [I32_0, I32(1)], True)
	refcount = self._block.sub(self._block.load(prefcount), I32(1))
	self._block.store(refcount, prefcount)
	with self._block.if_then(self._block.icmp_signed("<=", refcount, I32_0)) as then:
		dtor.blocks.insert(1, ir.Block(parent=dtor, name="usercode"))
		self._block.call(self._deallocator, [self._block.bitcast(dtor.args[0], PI8)])
	self._block.ret_void()
	del self._block
	dtor.blocks[0].terminator.operands[1] = dtor.blocks[1]

	methods = {}
	vtableIdx = 1
	
	self._objects[node.name] = (pstruct, fields, methods)
	
	for method in node.methods:
		if method.name == ".ctor":
			ctor.ftype.args = tuple(T for T, _ in method.args.values())
			ctor.ftype.vararg = method.vararg
			ctor.args = [ir.Argument(ctor, T, name) for name, (T, _) in method.args.items()]
			self._block = ir.IRBuilder(ctor.blocks[-1])

			createMethodContext(ctor, method, this, True)
			self._emitBlock(method, "entry")
			if self._block.block.is_terminated:
				self.panic("constructor can not return")
				continue
			delMethodContext()

		elif method.name == ".dtor":
			self._block = ir.IRBuilder(dtor.blocks[1])

			createMethodContext(dtor, method, dtor.args[0])
			self._emitBlock(method, "entry")
			if self._block.block.is_terminated:
				self.panic("deconstructor can not return")
				continue
			self._block.branch(dtor.blocks[2])
			delMethodContext()

		else:
			sig = vtable_t.elements[vtableIdx].pointee
			f = ir.Function(self._module, sig, mangle("method", node.name, sig, method.name))
			f.args[0].name = "this"
			createMethodContext(f, method, f.args[0])
			self._emitBlock(method, "entry")
			delMethodContext()
			methods[method.name] = (method.storage, f.ftype, vtableIdx, f)
			vtableIdx += 1

	self._block = ir.IRBuilder(ctor.blocks[-1])
	for _, (_, sig, idx, f) in methods.items():
		self._block.store(f, self._block.gep(vtable, [I32_0, I32(idx)], True))
	self._block.ret(this)
	del self._block

__all__ = ["emit"]
