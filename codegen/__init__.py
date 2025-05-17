import re
import sys
import traceback

from lark import Token as LarkToken
from llvmlite import ir, binding as llvm

from parser import *

STORAGE_PRIVATE = STORAGE2I["private"]
STORAGE_PUBLIC = STORAGE2I["public"]

I32  = TYPE2LLVM[0]
F32  = TYPE2LLVM[1]
F64  = TYPE2LLVM[2]
I8	 = TYPE2LLVM[3]
PI8	 = TYPE2LLVM[4]
VOID = TYPE2LLVM[5]
I1	 = TYPE2LLVM[6]
I64	 = TYPE2LLVM[7]

CCONV = {
	"__cdecl": "ccc",
	"__fastcall": "fastcc",
	"__stdcall": "stdcc",
}

NNAME2CMPOP = {
	"Eq":	"==",
	"Neq":	"!=",
	"Lt":	"<",
	"Le":	"<=",
	"Gt":	">",
	"Ge":	">=",
}


def defaultPanic(fmt: str, *args):
	print("[ERR]", fmt % args)
	exit(1)


def checki(i1: ir.Type, i2: ir.Type):
	try:
		return int(str(i1)[1:]) <= int(str(i2)[1:])
	except:
		return False
	

def cast(block: ir.IRBuilder, expected: ir.Type, value):
	got: ir.Type = value.type
	if expected == got:
		return value, True

	if expected.is_pointer and got.is_pointer:
		return block.bitcast(value, expected), True
	
	if expected == I1:
		if got.is_pointer:
			return block.icmp_unsigned("==", value, ir.Constant(got, None)), True
		else:
			return block.icmp_signed("==", value, ir.Constant(got, 0)), True
	else:
		if checki(expected, got):
			return (ir.Constant(expected, value.constant) if type(value) == ir.Constant else block.trunc(value, expected)), True
		elif checki(got, expected):
			return (ir.Constant(expected, value.constant) if type(value) == ir.Constant else block.zext(value, expected)), True
		
	if expected.is_pointer and type(got) == ir.IntType and got.width >= 32:
		return block.inttoptr(value, expected), True
	
	return value, False

defined = set()
def str2name(s: str, maxLen: int = 16) -> str:
	global defined
	basename = "aEmpty"
	s = list("".join(c for c in s if 32 <= ord(c) < 127) \
		.replace("*", "Star ") \
		.replace(".", "Dot ") \
		.replace("%", "Percent ") \
		.replace("/", "Slash ") \
		.replace("\n", "Newline ") \
		.replace("\\", "Backslash ") \
		.replace("+", "Plus ") \
		.replace(";", "Semicolon ") \
		.replace(":", "Colon ") \
		.replace("!", "Exclamation ") \
		.replace("?", "Question ") \
		.replace("-", "Minus ")
	)
	if len(s) > 0:
		s[0] = s[0].upper()
		for i in range(0, len(s)):
			if s[i] == " " and i+1 < len(s): s[i+1] = s[i+1].upper()
		s = "".join(s)
		match = re.match(r"([a-zA-Z0-9]+)", s.replace(" ", ""))
		if match:
			basename = "a" + match.group(1)[:maxLen]
		else:
			basename = "string"
		
	name = basename
	count = 1
	while name in defined:
		name = f"{basename}.{count}"
		count += 1
	defined.add(name)
	return name


class CodeGen:
	def __init__(self, verbose: bool = False, is64Bit: bool = True, panic=defaultPanic) -> None:
		self.verbose = verbose
		self.is64Bit = is64Bit
		self.panic = panic

		opt = llvm.create_module_pass_manager()
		opt.add_gvn_pass()
		#opt.add_loop_unroll_pass() # this sucks
		opt.add_instruction_combining_pass()
		opt.add_dead_code_elimination_pass()
		opt.add_reassociate_expressions_pass()
		opt.add_global_dce_pass()
		self._optimizer = opt

	def _log(self, fmt: str, *args):
		if self.verbose:
			print(" -", fmt % args)

	def generate(self, root: ModuleNode, optimize: bool = True) -> bytes:
		llvm.initialize()
		llvm.initialize_native_target()
		llvm.initialize_native_asmprinter()

		triple = llvm.get_default_triple()
		if not self.is64Bit:
			split = triple.split("-")
			split[0] = "i386"
			triple = "-".join(split)
			del split
		target = llvm.Target.from_triple(triple)
		self._machine = target.create_target_machine(codemodel="default")
		module = ir.Module(root.name)
		self._module = module
		module.triple = self._machine.triple
		module.data_layout = self._machine.target_data
		self._log("Target machine: '%s'", str(target))

		try:
			fltused = ir.GlobalVariable(module, I32, name="_fltused")
			fltused.initializer = ir.Constant(I32, 1)
			fltused.global_constant = False

			null = ir.GlobalVariable(module, PI8, name="null")
			null.initializer = ir.Constant(PI8, None)
			null.global_constant = True

			self._emitCode(root)

			if self.verbose: print(module)

			split = str(module).split("\n", 1)
			ref = llvm.parse_assembly(f"{split[0]}\nsource_filename = \"{module.name}\"\n{split[1]}")
			ref.name = module.name
			ref.verify()

			if optimize:
				self._optimizer.run(ref)
			
			obj = self._machine.emit_object(ref)
			del self._module, self._machine
			return obj
		except Exception as e:
			if self.verbose:
				_, _, tb = sys.exc_info()
				fr = traceback.extract_tb(tb)[-1]
				self.panic("code generation failed due to a fatal error (%s : %d): %s", fr.filename, fr.lineno, e)
			else:
				self.panic("code generation failed due to a fatal error")
			del self._module, self._machine
			return b""
	
	def _emitCode(self, root: ModuleNode) -> None:
		self._modules = {}

		for node in root.body:
			if type(node) == FunctionNode:
				self._emitFunction(node)

			elif type(node) == FunctionImportNode:
				self._emitImport(node)

			elif type(node) == UsingNode:
				try:
					with open(node.path) as f:
						parser = Parser()
						imported = parser.parse(f.read(), node.name)
						del parser
						f.close()
				except Exception as e:
					self.panic(f"failed to import module '{node.path}': {e}")
					continue

				included = {}
				for _node in imported.body:
					if type(_node) == FunctionImportNode:
						origName = _node.name
						_node.name = f"_{node.name}__{_node.name}"
						included[origName] = self._emitImport(_node)
						del origName
					else:
						self.panic("module '%s' is not a valid module: '%s' is not a valid top-level entity in this context", node.name, type(_node).__name__)
						continue

				self._modules[node.name] = included
			
			elif type(node) == GlobalVariableNode:
				expected, const = node.dataType
				type_, value = self._emitExpression(node.value)
				value, success = cast(self._block, expected, value)
				if not success:
					self.panic("constant (%s) is incompatible with function return value type (%s). line %d, column %d", str(type_), str(expected), node.line, node.column)
					continue
				var = ir.GlobalVariable(self._module, type_, node.name)
				var.initializer = value
				var.linkage = "public" if node.storage == STORAGE_PUBLIC else "private"
				var.global_constant = const

			elif type(node) == ObjectNode:
				self._emitObject(node)

			else:
				self.panic("invalid root statement '%s'", type(node).__name__[:-4])

		del self._modules

	def _emitFunction(self, root: FunctionNode):
		self._log("Entering function '%s %s func %s(...)' (%d arguments)",
			"public" if root.storage == STORAGE_PUBLIC else "private",
			root.conv,
			root.name,
			len(root.args)
		)
		self._func = ir.Function(self._module, ir.FunctionType(
			root.returnType[0] if root.returnType is not None else VOID,
			(T for T, _ in root.args.values()),
			root.vararg
		), root.name)
		self._funcNode = root
		self._func.calling_convention = CCONV[root.conv]
		if root.storage == STORAGE_PRIVATE: self._func.linkage = "private"
		self._args = tuple(root.args.keys())
		self._argsTi = tuple(root.args.values())
		self._returned = False
		self._block = ir.IRBuilder(self._func.append_basic_block("entry"))

		# TODO: find a better way to fix possible rsp overflow
		self._preallocated = {}
		def preallocate(root):
			for node in root.body:
				if type(node) == VariableNode and node.dataType is not None:
					self._preallocated[id(node)] = self._block.alloca(node.dataType[0], name=node.name)
				elif hasattr(node, "body"):
					preallocate(node)
		preallocate(root)

		self._emitBlock(root, "entry")
		del self._block, self._locals, self._preallocated
		
		if not self._returned:
			if root.returnType[0] == VOID:
				b = ir.IRBuilder(self._func.blocks[-1])
				b.ret_void()
				del b
			else:
				self.panic("missing return statement in non-void function")

		del self._func, self._funcNode

	def _emitBlock(self, root: Node, name: str) -> None:
		self._locals = getattr(self, "_locals", {})
		oldLocals = self._locals.copy()

		for node in root.body:
			if type(node) == ReturnNode:
				self._returned = True
				if self._funcNode.returnType is None:
					if self._funcNode.returnType is None and node.value is not None:
						type_, value = self._emitExpression(node.value)
						self._log("Guessed return type for function '%s': %s", self._func.name, type_)
						self._funcNode.returnType = type_

						del self._module.globals[self._func.name]
						self._module.scope._useset.remove(self._func.name)
						patched = ir.Function(self._func.module, ir.FunctionType(type_, self._func.ftype.args, self._func.ftype.var_arg), self._func.name)
						patched.calling_convention = CCONV[self._funcNode.conv]
						if self._funcNode.storage == STORAGE_PRIVATE: patched.linkage = "private"
						mapping = {}
						for old in self._func.blocks:
							block = ir.Block(patched, old.name)
							block.instructions = old.instructions
							block.terminator = old.terminator
							block.scope = old.scope
							patched.blocks.append(block)
							mapping[old] = block
						self._func = patched

						current = mapping.get(self._block.block)
						self._block = ir.IRBuilder(current)
						self._block.position_at_end(current)
						
						self._block.ret(value)
					else:
						self._block.ret_void()
				else:
					expected: ir.Type = self._block.block.function.return_value.type

					if node.value is not None:
						type_, value = self._emitExpression(node.value)
						value, success = cast(self._block, expected, value)
						if not success:
							self.panic("return value (%s) is incompatible with function return type (%s). line %d, column %d", str(type_), str(expected), node.line, node.column)
							continue
						self._block.ret(value)
					else:
						self._block.ret_void()

			elif type(node) == CallNode:
				self._emitCall(node)

			elif type(node) == VariableNode:
				self._emitVariable(node)

			elif type(node) == IfNode:
				_, firstCond = self._emitExpression(node.condition)
				firstCond, _ = cast(self._block, I1, firstCond)
				
				if len(node.elseIf) == 0 and node.else_ is None:
					with self._block.if_then(firstCond):
						self._emitBlock(node, "then")
				
				else:
					with self._block.if_else(firstCond) as (then, otherwise):
						with then:
							self._emitBlock(node, "then")
						
						if len(node.elseIf) > 0:
							for i, elseIf in enumerate(node.elseIf):
								with otherwise:
									_, elifCond = self._emitExpression(elseIf.condition)
									elifCond, _ = cast(self._block, I1, elifCond)

									with self._block.if_else(elifCond) as (then, otherwise):
										with then:
											self._emitBlock(elseIf, f"elseIf_{i}")

										if node.else_ is not None:
											with otherwise:
												self._emitBlock(node.else_, "else")

						elif node.else_ is not None:
							with otherwise:
								self._emitBlock(node.else_, "else")
			
			elif type(node) == AssignNode:
				self._emitAssignment(node)
			
			elif type(node) == WhileLoopNode:
				condBlock: ir.Block  = self._block.append_basic_block("loop.cond")
				bodyBlock: ir.Block  = self._block.append_basic_block("loop.body")
				mergeBlock: ir.Block = self._block.append_basic_block("loop.merge")

				self._block.branch(condBlock)
				self._block = ir.IRBuilder(condBlock)
				_, cond = self._emitExpression(node.condition)
				cond, _ = cast(self._block, I1, cond)
				self._block.cbranch(cond, bodyBlock, mergeBlock)

				oldBreak = getattr(self, "_break", None)
				oldContinue = getattr(self, "_continue", None)
				self._break = mergeBlock
				self._continue = condBlock

				self._block = ir.IRBuilder(bodyBlock)
				self._emitBlock(node, "loop.body")
				if not self._block.block.is_terminated:
					self._block.branch(condBlock)

				if oldBreak is not None: self._break = oldBreak
				else: del self._break

				if oldContinue is not None: self._continue = oldContinue
				else: del self._continue

				self._block = ir.IRBuilder(mergeBlock)

			elif type(node) == ForLoopNode:
				condBlock: ir.Block  = self._block.append_basic_block("loop.cond")
				bodyBlock: ir.Block  = self._block.append_basic_block("loop.body")
				mergeBlock: ir.Block = self._block.append_basic_block("merge")

				for var in node.variables:
					self._emitVariable(var)

				self._block.branch(condBlock)
				self._block = ir.IRBuilder(condBlock)
				_, cond = self._emitExpression(node.condition)
				cond, _ = cast(self._block, I1, cond)
				self._block.cbranch(cond, bodyBlock, mergeBlock)

				oldBreak = getattr(self, "_break", None)
				oldContinue = getattr(self, "_continue", None)
				self._break = mergeBlock
				self._continue = condBlock

				self._block = ir.IRBuilder(bodyBlock)
				self._emitBlock(node, "loop.body")
				if not self._block.block.is_terminated:
					for step in node.steps:
						self._emitAssignment(step)
						
					self._block.branch(condBlock)
				
				if oldBreak is not None: self._break = oldBreak
				else: del self._break

				if oldContinue is not None: self._continue = oldContinue
				else: del self._continue

				self._block = ir.IRBuilder(mergeBlock)

			elif type(node) == BreakNode:
				self._block.branch(self._break)
				break

			elif type(node) == ContinueNode:
				self._block.branch(self._continue)
				break

			else:
				self.panic("invalid statement '%s' in block", type(node).__name__[:-4])

		self._log("Leaving block '%s:%s' (%d local(s))", getattr(root, "name", "<unnamed>"), name, len(self._locals) - len(oldLocals))
		self._locals = oldLocals

	def _emitExpression(self, node: ExpressionNode, const: bool = False):
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
				itype, index = self._emitExpression(node.value)
				type_, arr = self._emitExpression(LarkToken("REF", node.arr, node.line, node.column))
				if type(type_.pointee) != ir.ArrayType:
					self.panic("'%s' is not an array type. line %d, column %d", type_, node.line, node.column)
				if str(itype)[0] == "i" and node.value.value >= type_.pointee.count:
					self.panic("index out of bounds. line %d, column %d", node.line, node.column)
				return (type_.pointee.element, self._block.load(self._block.gep(arr, [ir.Constant(I32, 0), index])))
			
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
					self._block.store(val, self._block.gep(allocated, [ir.Constant(I32, 0), ir.Constant(I32, i)]))

				return (allocated.allocated_type, allocated)

			else:
				self.panic("whoopsies")

		else:
			self.panic("invalid statement '%s' in expression", type(node).__name__[:-4])

	def _emitCall(self, node: CallNode):
		callee = node.name
		args = [list(self._emitExpression(arg)) for arg in node.args]

		func = None
		vararg = False

		module = None
		if len(node.scope) == 0:
			if callee not in self._module.globals:
				self.panic("reference to undefined function '%s'. line %d, column %d", callee, node.line, node.column)
				return
			func: ir.Function = self._module.get_global(callee)
			vararg = func.function_type.var_arg
		else:
			module = self._modules
			fqn = ""

			for scope in node.scope:
				module = module.get(scope)
				if module is None:
					self.panic("'%s' does not name a valid scope. line %d, column %d", scope, node.line, node.column)
					return
				fqn += f"_{scope}__"
			fqn += callee

			if fqn in self._module.globals:
				func: ir.Function = self._module.get_global(fqn)
				vararg = func.function_type.var_arg
			else:
				func: ir.Function = module.get(callee)
				if func is None:
					self.panic("reference to undefined function '%s'. line %d, column %d", fqn, node.line, node.column)
					return
				vararg = func.function_type.var_arg

		expectedArgs = func.function_type.args
		if not vararg and len(args) != len(expectedArgs):
			self.panic("argument count mismatch. expected %d, got %d. line %d, column %d", len(expectedArgs), len(args), node.line, node.column)
			return
		elif vararg and len(args) < len(expectedArgs):
			self.panic("argument count mismatch. expected at least %d, got %d. line %d, column %d", len(expectedArgs), len(args), node.line, node.column)
			return

		for i, expected in enumerate(expectedArgs):
			got = args[i][0]
			if got != expected:
				value, success = cast(self._block, expected, args[i][1])
				args[i][1] = value
				if not success:
					self.panic(
						"argument %d: expected type %s, got %s. line %d, column %d",
						i, expected, got, node.line, node.column
					)
					continue

		return (func.function_type.return_type, self._block.call(func, (value for _, value in args)))
	
	def _emitVariable(self, node: VariableNode):
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
							self._block.load(self._block.gep(value, [I32(0), idx])),
							self._block.gep(alloca, [I32(0), idx])
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
	
	def _emitAssignment(self, node: AssignNode):
		def _emitLhs(val):
			if type(val) == LarkToken and val.type == "DEREF":
				old = val
				val = _emitLhs(val.value)
				if old in self._locals:
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

	def _emitImport(self, node: FunctionImportNode):
		if type(node) == FunctionImportNode:
			f = ir.Function(
				self._module,
				ir.FunctionType(node.returnType[0], (T for T, _ in node.args.values()), node.vararg),
				node.name
			)
			f.calling_convention = CCONV[node.conv]
			return f
		else:
			return self.panic("'%s' is not a valid import", type(node).__name__[:-4])

	def _emitObject(self, node: ObjectNode):
		pass
