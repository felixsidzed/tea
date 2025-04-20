import re

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
	return int(str(i1)[1:]) <= int(str(i2)[1:])


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
				self.panic("code generation failed due to a fatal error: %s", e)
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

		self._emitBlock(root, "entry", self._func)

		del self._func

	def _emitBlock(self, root: Node, name: str, parent=None) -> None:
		if parent is not None:
			self._block = ir.IRBuilder(parent.append_basic_block(name))
		self._locals = getattr(self, "_locals", {})

		for node in root.body:
			if type(node) == ReturnNode:
				expected: ir.Type = self._block.block.function.return_value.type
				type_, value = self._emitExpression(node.value)
				if type_ != expected:
					if checki(type_, expected):
						value = ir.Constant(expected, value.constant) if type(value) == ir.Constant else self._block.zext(value, expected)
					else:
						self.panic("return value (%s) is incompatible with function return value type (%s). line %d, column %d", str(type_), str(expected), node.line, node.column)
						continue
				self._block.ret(value)

			elif type(node) == CallNode:
				self._emitCall(node)

			elif type(node) == VariableNode:
				self._emitVariable(node)

			elif type(node) == IfNode:
				_, cond = self._emitExpression(node.condition)

				thenBlock: ir.Block = self._block.append_basic_block("then")
				elseBlock: ir.Block = self._block.append_basic_block("else") if node.else_ else None
				elseIfBlocks = [(self._block.append_basic_block(f"elseif{i}"), 
								self._block.append_basic_block(f"elseif{i}.body")) for i in range(len(node.elseIf))]
				mergeBlock: ir.Block = self._block.append_basic_block("merge")

				firstFallback = elseIfBlocks[0][0] if elseIfBlocks else (elseBlock or mergeBlock)
				self._block.cbranch(cond, thenBlock, firstFallback)

				thenBuilder = ir.IRBuilder(thenBlock)
				self._block = thenBuilder
				oldLocals = self._locals
				self._emitBlock(node, "then")
				if not thenBlock.is_terminated:
					thenBuilder.branch(mergeBlock)

				for i, (condBlock, bodyBlock) in enumerate(elseIfBlocks):
					self._locals = oldLocals
					self._block = ir.IRBuilder(condBlock)
					_, elseifCond = self._emitExpression(node.elseIf[i].condition)

					nextFallback = elseIfBlocks[i+1][0] if i+1 < len(elseIfBlocks) else (elseBlock or mergeBlock)
					self._block.cbranch(elseifCond, bodyBlock, nextFallback)

					bodyBuilder = ir.IRBuilder(bodyBlock)
					self._block = bodyBuilder
					self._emitBlock(node.elseIf[i], f"elseif{i}.body")
					if not bodyBlock.is_terminated:
						bodyBuilder.branch(mergeBlock)

				if node.else_ is not None:
					elseBuilder = ir.IRBuilder(elseBlock)
					self._locals = oldLocals
					self._block = elseBuilder
					self._emitBlock(node.else_, "else")
					if not elseBlock.is_terminated:
						elseBuilder.branch(mergeBlock)

				self._block = ir.IRBuilder(mergeBlock)
				self._locals = oldLocals
			
			elif type(node) == AssignNode:
				self._emitAssignment(node)
			
			elif type(node) == WhileLoopNode:
				condBlock: ir.Block  = self._block.append_basic_block("loop.cond")
				bodyBlock: ir.Block  = self._block.append_basic_block("loop.body")
				mergeBlock: ir.Block = self._block.append_basic_block("merge")

				self._block.branch(condBlock)
				self._block = ir.IRBuilder(condBlock)
				_, cond = self._emitExpression(node.condition)
				self._block.cbranch(cond, bodyBlock, mergeBlock)

				oldLocals = self._locals
				bodyBuilder = ir.IRBuilder(bodyBlock)
				self._block = bodyBuilder
				self._emitBlock(node, "loop.body")
				self._locals = oldLocals
				if not bodyBlock.is_terminated:
					bodyBuilder.branch(condBlock)

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

				oldLocals = self._locals
				bodyBuilder = ir.IRBuilder(bodyBlock)
				self._block = bodyBuilder
				self._emitBlock(node, "loop.body")
				self._locals = oldLocals
				if not bodyBlock.is_terminated:
					self._block = bodyBuilder
					for step in node.steps:
						self._emitAssignment(step)
						
					bodyBuilder.branch(condBlock)

				self._block = ir.IRBuilder(mergeBlock)

			else:
				self.panic("invalid statement '%s' in block", type(node).__name__[:-4])

		self._log("Leaving block '%s:%s' (%d locals)", getattr(root, "name", "<unnamed>"), name, len(self._locals))
		del self._block, self._locals

	def _emitExpression(self, node: ExpressionNode):
		# we use some tricks to minimize the amount of duplicate code
		if type(node) == LarkToken:
			if node.type not in ("IDENTF", "STRING"):
				T = Type.get(node.type)[0]
				return (T, ir.Constant(T, node.value))
			
			elif node.type == "IDENTF":
				if node.value in self._args:
					arg = self._func.args[self._args.index(node.value)]
					return (arg.type, arg)
				elif node.value in self._locals:
					local = self._locals[node.value]
					return (local[1][0], self._block.load(local[0]))
				else:
					self.panic("undefined constant '%s' in expression", node.name)

			elif node.type == "STRING":
				value = bytearray(node.value.encode("utf8")) + b"\00"
				type_ = ir.ArrayType(ir.IntType(8), len(value))
				
				g = ir.GlobalVariable(self._module, type_, str2name(node.value))
				g.linkage = "internal"
				g.global_constant = True
				g.initializer = ir.Constant(type_, value)
				
				return (PI8, self._block.bitcast(g, PI8))
			
			else:
				self.panic("invalid constant '%s' in expression", node.value)

		elif type(node) == CallNode:
			return self._emitCall(node)

		elif isinstance(node, ExpressionNode):
			if type(node) in (AddNode, MulNode, SubNode, DivNode):
				lhs = self._emitExpression(node.left)
				rhs = self._emitExpression(node.right)
				return (lhs[0], getattr(self._block, type(node).__name__.lower()[:-4].replace("div", "sdiv"))(lhs[1], rhs[1]))
			
			elif type(node) in (EqNode, NeqNode, LtNode, LeNode, GtNode, GeNode):
				lhs = self._emitExpression(node.left)
				rhs = self._emitExpression(node.right)
				cmpop = NNAME2CMPOP[type(node).__name__[:-4]]
				if lhs[0] in (F32, F64):
					return (I1, self._block.fcmp_ordered(cmpop, lhs[1], rhs[1]))
				elif lhs[0] in (I32, I8):
					return (I1, self._block.icmp_signed(cmpop, lhs[1], rhs[1]))
				else:
					self.panic("invalid lhs type '%s' in expression", lhs[0])
			
			elif type(node) == NotNode:
				type_, value = self._emitExpression(node.value)
				return (type_, self._block.not_(value))

		else:
			self.panic("invalid statement '%s' in expression", type(node).__name__[:-4])

	def _emitCall(self, node: CallNode):
		if len(node.scope) == 0 and node.name not in self._module.globals:
			self.panic("call to undefined function '%s'. line %d, column %d", node.name, node.line, node.column)
			return

		args = []
		for arg in node.args:
			args.append(self._emitExpression(arg))

		if len(node.scope) == 0:
			func: ir.Function = self._module.get_global(node.name)
			vararg = func.function_type.var_arg
		else:
			module = self._modules.get(node.scope[0], None)
			if (module["namespace"] + node.name) in self._module.globals:
				func: ir.Function = self._module.get_global(module["namespace"] + node.name)
				vararg = func.function_type.var_arg
			else:
				if module is None:
					self.panic("'%s' does not name any valid scope. line %d, column %d", node.scope[0], node.line, node.column)
					return
				funcDef = module["functions"][node.name]
				vararg = funcDef["vararg"]
				func: ir.Function = ir.Function(self._module, ir.FunctionType(funcDef["return"], funcDef["args"], vararg), module["namespace"] + node.name)
		
		if not vararg:
			if len(func.function_type.args) != len(args):
				self.panic("argument count mismatch. line %d, column %d", node.line, node.column)
				return
		else:
			if len(args) < len(func.function_type.args):
				self.panic("argument count mismatch. line %d, column %d", node.line, node.column)
				return

		for i, expected in enumerate(func.function_type.args):
			got = args[i][0]
			if expected != got:
				self.panic("argument %d: argument type (%s) does not match function argument type (%s). line %d, column %d",
					i,
					expected, got,
					node.line, node.column
				)
				return

		return (func.function_type.return_type, self._block.call(func, (value for _, value in args)))
	
	def _emitVariable(self, node: VariableNode):
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
		if node.lhs not in self._locals:
			self.panic("invalid lhs operator '%s' in assignment. line %d, column %d", node.lhs, node.line, node.column)
			return
		local = self._locals[node.lhs]
		if local[1][1]:
			self.panic("assignment to a constant variable '%s'. line %d, column %d", node.lhs, node.line, node.column)
			return
		rhs = list(self._emitExpression(node.rhs))
		if rhs[0] != local[1][0]:
			self.panic("value type (%s) does not match variable type (%s). line %d, column %d", rhs[0], local[1][0], node.line, node.column)
			return
		
		if node.extra:
			ptr = self._block.load(local[0])
		if node.extra == "+":
			rhs[1] = self._block.add(ptr, rhs[1])
		elif node.extra == "-":
			rhs[1] = self._block.sub(ptr, rhs[1])
		elif node.extra == "*":
			rhs[1] = self._block.mul(ptr, rhs[1])
		elif node.extra == "/":
			rhs[1] = self._block.sdiv(ptr, rhs[1])
		elif node.extra is None: pass
		else:
			self.panic("invalid operator '%s' in assignment. line %d, column %d", node.extra, node.line, node.column)
			return

		self._block.store(rhs[1], local[0])
