from codegen.util import I32, I32_0, VOID, PI8
from parser.ast import ObjectNode, FunctionNode, ir

# TODO: clean
def emit(self, node: ObjectNode):
	struct = ir.global_context.get_identified_type(node.name)
	fields = {}
	body_ = [ I32 ]
	for field in node.fields:
		fields[field.name] = (field.storage, field.dataType)
		body_.append(field.dataType[0])
	struct.set_body(*body_)
	del body_

	pstruct = struct.as_pointer()
	sizeof = struct.get_abi_size(self._machine.target_data)

	self._log("Creating object '%s' (%d bytes, %d method(s), %d field(s))", node.name, sizeof - I32.get_abi_size(self._machine.target_data), len(node.methods), len(node.fields))

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

	ctor = ir.Function(self._module, ir.FunctionType(
		pstruct,
		[]
	), f"_{node.name}_new")
	self._block = ir.IRBuilder(ctor.append_basic_block("entry"))
	allocated = self._block.call(self._allocator, [ir.Constant(I32, sizeof)])
	this = self._block.bitcast(allocated, pstruct)
	self._block.store(ir.Constant(I32, 1), self._block.gep(this, [I32_0, I32_0]))
	self._ctors[node.name] = ctor
	del self._block

	dtor = ir.Function(self._module, ir.FunctionType(
		VOID,
		[pstruct]
	), f"_{node.name}_deconstruct")
	dtor.args[0].name = "this"
	self._block = ir.IRBuilder(dtor.append_basic_block("entry"))
	prefcount = self._block.gep(dtor.args[0], [I32_0, I32_0], True)
	refcount = self._block.sub(self._block.load(prefcount), I32(1))
	self._block.store(refcount, prefcount)
	with self._block.if_then(self._block.icmp_signed("<=", refcount, I32_0)) as then:
		self._block.call(self._deallocator, [self._block.bitcast(dtor.args[0], PI8)])
	self._block.ret_void()
	self._dtors[node.name] = dtor
	del self._block

	methods = {}
	for method in node.methods:
		if method.name == ".ctor":
			ctor.ftype.args = tuple(T for T, _ in method.args.values())
			ctor.ftype.vararg = method.vararg
			ctor.args = [ir.Argument(ctor, T, name) for name, (T, _) in method.args.items()]
			self._block = ir.IRBuilder(ctor.entry_basic_block)

			createMethodContext(ctor, method, this)
			self._emitBlock(method, method.name)
			if self._block.block.is_terminated:
				self.panic("constructor can not return")
				continue
			self._block.ret(this)
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
			f = ir.Function(self._module, ir.FunctionType(
				method.returnType[0],
				[pstruct] + [T for T, _ in method.args.values()],
				method.vararg
			), f"_{node.name}_{method.name}")
			f.args[0].name = "this"
			createMethodContext(f, method, f.args[0])
			self._emitBlock(method, method.name)
			delMethodContext()
			methods[method.name] = (method.storage, f)

	self._objects[node.name] = (pstruct, this, fields, methods)

__all__ = ["emit"]
