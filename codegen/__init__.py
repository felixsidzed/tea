import re
import sys
import traceback

from lark import Token as LarkToken
from llvmlite import ir, binding as llvm

from parser.nodes import *
from codegen.teaparse import *

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


# TODO: clean
def str2name(s: str, max_length: int = 16) -> str:
	s = list("".join(c for c in s if 32 <= ord(c) < 127) \
		.replace("*", "Star ") \
		.replace(".", "Dot ") \
		.replace("%", "Percent ") \
		.replace("/", "Slash ") \
		.replace("\\", "Backslash ") \
		.replace("+", "Plus ") \
		.replace(";", "Semicolon ") \
		.replace(":", "Colon ") \
		.replace("!", "Exclamation ") \
		.replace("?", "Question ") \
		.replace("-", "Minus "))
	if len(s) > 0:
		s[0] = s[0].upper()
		for i in range(0, len(s)):
			if s[i] == " " and i+1 < len(s): s[i+1] = s[i+1].upper()
		s = "".join(s)
		match = re.match(r"([a-zA-Z0-9]+)", s.replace(" ", ""))
		if match:
			return "a" + match.group(1)[:max_length]
		else:
			return "string"
	else:
		return "string"


def fixTypes(module: dict):
	for f in module["functions"].values():
		f["return"] = Type.get(f["return"])[0]
		for i, arg in enumerate(f["args"]):
			f["args"][i] = Type.get(arg)[0]


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
		self._optimizer = opt

	def _log(self, fmt, *args):
		if self.verbose:
			print(" -", fmt % args)

	def generate(self, root: ModuleNode) -> bytes:
		llvm.initialize()
		llvm.initialize_native_target()
		llvm.initialize_native_asmprinter()

		# hard coded triple :(
		target = llvm.Target.from_triple(
			"x86_64-pc-windows-msvc" if self.is64Bit else "i386-pc-windows-msvc"
		)
		self._machine = target.create_target_machine(codemodel="default")
		self._module = ir.Module("[module]")
		self._module.triple = self._machine.triple
		self._module.data_layout = self._machine.target_data
		self._log("Target machine: '%s'", str(target))

		#ir.GlobalVariable(self._module, I8.as_pointer(), "nullptr").linkage = "internal"

		try:
			self._emitCode(root)

			if self.verbose: print(self._module)

			split = str(self._module).split("\n", 1)
			ref = llvm.parse_assembly(f"{split[0]}\nsource_filename = \"{self._module.name}\"\n{split[1]}")
			ref.name = self._module.name
			ref.verify()

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

			elif type(node) == FunctionDeclarationNode:
				ir.Function(
					self._module,
					ir.FunctionType(node.returnType[0], (T for T, _ in node.args.values())
				), node.name).calling_convention = CCONV[node.conv]

			elif type(node) == UsingNode:
				with open(node.path) as f:
					parsed = parseJSON(f.read())
					f.close()
				fixTypes(parsed)
				self._modules[node.name] = parsed
			
			elif type(node) == GlobalVariableNode:
				expected, const = node.dataType
				type_, value = self._emitExpression(node.value)
				if type_ != expected:
					if checki(type_, expected):
						value = ir.Constant(expected, value.constant) if type(value) == ir.Constant else self._block.zext(value, expected)
					else:
						self.panic("constant (%s) is incompatible with function return value type (%s). line %d, column %d", str(type_), str(expected), node.line, node.column)
						continue
				var = ir.GlobalVariable(self._module, type_, node.name)
				var.initializer = value
				var.linkage = "public" if node.storage == STORAGE_PUBLIC else "private"
				var.global_constant = const

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
		self._func = ir.Function(self._module, ir.FunctionType(root.returnType[0], (T for T, _ in root.args.values())), root.name)
		self._func.calling_convention = CCONV[root.conv]
		if root.storage == STORAGE_PRIVATE: self._func.linkage = "private"
		self._args = tuple(root.args.keys())
		self._argsTi = tuple(root.args.values())
		self._returned = False

		self._emitBlock(root, "entry", self._func)
		
		if not self._returned:
			if root.returnType[0] == VOID:
				b = ir.IRBuilder(self._func.blocks[-1])
				b.ret_void()
				del b
			else:
				self.panic("missing return statement in non-void function")

		del self._func

	def _emitBlock(self, root: Node, name: str, parent=None, preserve: bool = True) -> None:
		if parent is not None:
			self._block = ir.IRBuilder(parent.append_basic_block(name))
		self._locals = getattr(self, "_locals", {})

		for node in root.body:
			if type(node) == ReturnNode:
				self._returned = True
				expected: ir.Type = self._block.block.function.return_value.type
				if node.value is not None:
					type_, value = self._emitExpression(node.value)
					if type_ != expected:
						if (type_.is_pointer or type(type_) == ir.FunctionType) and expected.is_pointer:
							value = self._block.bitcast(value, expected)
						elif checki(type_, expected):
							value = ir.Constant(expected, value.constant) if type(value) == ir.Constant else self._block.zext(value, expected)
						else:
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
				
				if len(node.elseIf) == 0 and node.else_ is None:
					with self._block.if_then(firstCond):
						self._emitBlock(node, "then", preserve=True)
				
				else:
					with self._block.if_else(firstCond) as (then, otherwise):
						with then:
							self._emitBlock(node, "then", preserve=True)
						
						if len(node.elseIf) > 0:
							for i, elseIf in enumerate(node.elseIf):
								with otherwise:
									_, elifCond = self._emitExpression(elseIf.condition)
									with self._block.if_else(elifCond) as (then, otherwise):
										with then:
											self._emitBlock(elseIf, f"elseIf_{i}", preserve=True)

										if node.else_ is not None:
											with otherwise:
												self._emitBlock(node.else_, "else", preserve=True)

						elif node.else_ is not None:
							with otherwise:
								self._emitBlock(node.else_, "else", preserve=True)
			
			elif type(node) == AssignNode:
				self._emitAssignment(node)
			
			elif type(node) == WhileLoopNode:
				condBlock: ir.Block  = self._block.append_basic_block("loop.cond")
				bodyBlock: ir.Block  = self._block.append_basic_block("loop.body")
				mergeBlock: ir.Block = self._block.append_basic_block("loop.merge")

				self._block.branch(condBlock)
				self._block = ir.IRBuilder(condBlock)
				_, cond = self._emitExpression(node.condition)
				self._block.cbranch(cond, bodyBlock, mergeBlock)

				self._block = ir.IRBuilder(bodyBlock)
				self._emitBlock(node, "loop.body", preserve=True)
				if not self._block.block.is_terminated:
					self._block.branch(condBlock)

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
				self._block.cbranch(cond, bodyBlock, mergeBlock)

				self._block = ir.IRBuilder(bodyBlock)
				self._emitBlock(node, "loop.body", preserve=True)
				if not self._block.block.is_terminated:
					for step in node.steps:
						self._emitAssignment(step)
						
					self._block.branch(condBlock)

				self._block = ir.IRBuilder(mergeBlock)

			else:
				self.panic("invalid statement '%s' in block", type(node).__name__[:-4])

		self._log("Leaving block '%s:%s' (%d locals)", getattr(root, "name", "<unnamed>"), name, len(self._locals))
		if not preserve:
			del self._block, self._locals

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
					self.panic("cannot dereference non-pointer type '%s'" % str(type_))

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
					self.panic("invalid lhs type '%s' in expression", lhs[0])
			
			elif type(node) == NotNode:
				type_, value = self._emitExpression(node.value, const)
				return (type_, self._block.not_(value))
			
			elif type(node) == AndNode:
				lhs = self._emitExpression(node.left, const)
				rhs = self._emitExpression(node.right, const)
				return (I1, self._block.and_(lhs[1], rhs[1]))
			
			elif type(node) == OrNode:
				lhs = self._emitExpression(node.left, const)
				rhs = self._emitExpression(node.right, const)
				return (I1, self._block.or_(lhs[1], rhs[1]))

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
				fqn += module["namespace"]
			fqn += callee

			if fqn in self._module.globals:
				func: ir.Function = self._module.get_global(fqn)
				vararg = func.function_type.var_arg
			else:
				finfo = module["functions"].get(callee)
				if finfo is None:
					self.panic("reference to undefined function '%s'. line %d, column %d", fqn, node.line, node.column)
					return
				ftype = ir.FunctionType(finfo["return"], finfo["args"], finfo["vararg"])
				func: ir.Function = ir.Function(self._module, ftype, fqn)
				vararg = finfo["vararg"]

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
				if (got.is_pointer or type(got) == ir.FunctionType) and expected.is_pointer:
					args[i][1] = self._block.bitcast(args[i][1], expected)
				elif checki(got, expected):
					args[i][1] = ir.Constant(expected, args[i][1].constant) if type(args[i][1]) == ir.Constant else self._block.zext(args[i][1], expected)
				else:
					self.panic(
						"argument %d: expected type %s, got %s. line %d, column %d",
						i, expected, got, node.line, node.column
					)
					continue

		return (func.function_type.return_type, self._block.call(func, (value for _, value in args)))
	
	def _emitVariable(self, node: VariableNode):
		self._log("Emitting local %s '%s' (initialized = %s)", node.dataType[0], node.name, node.value is not None)

		expected = node.dataType[0]
		alloca = self._block.alloca(expected, name=node.name)

		if node.value is not None:
			type_, value = self._emitExpression(node.value)
			if type_ != expected:
				if checki(type_, expected):
					value = ir.Constant(expected, value.constant) if isinstance(value, ir.Constant) else self._block.zext(value, expected)
				else:
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
