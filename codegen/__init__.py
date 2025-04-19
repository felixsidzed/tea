import re
import sys
import json

from lark import Token as LarkToken

from parser.nodes import *
import codegen.pygen as pygen

ENDIAN					= sys.byteorder
STORAGE_PRIVATE			= STORAGE2I["private"]
STORAGE_PUBLIC			= STORAGE2I["public"]
FASTCALL_ARGSSET		= [
	b"\x48\x89\xC1",		# mov rcx, rax
	b"\x48\x89\xC2",		# mov rdx, rax
	b"\x49\x89\xC0",		# mov r8, rax
	b"\x49\x89\xC1",		# mov r9, rax
]
FASTCALL_ARGSGET		= [
	b"\x48\x8B\xC1",		# mov rax, rcx
	b"\x48\x89\xD0",		# mov rax, rdx
	b"\x49\x89\xC1",		# mov rax, r8
	b"\x49\x89\xC1",		# mov rax, r9

	b"\x48\x89\xCB",		# mov rbx, rcx
	b"\x48\x89\xD3",		# mov rbx, rdx
	b"\x4C\x89\xC3",		# mov rbx, r8
	b"\x4C\x89\xCB",		# mov rbx, r9
]
FASTCALL_ARGSPRESERVE	= [
	b"\x51",				# push rcx
	b"\x52",				# push rdx
	b"\x41\x50",			# push r8
	b"\x41\x51",			# push r9
]
FASTCALL_ARGSRESTORE	= [
	b"\x59",				# pop rcx
	b"\x5A",				# pop rdx
	b"\x41\x58",			# pop r8
	b"\x41\x59",			# pop r9
]


def defaultPanic(fmt: str, *args):
	print(fmt % args)
	exit(1)

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
	for i in range(0, len(s)):
		if s[i] == " ": s[i+1] = s[i+1].upper()
	s = "".join(s)
	match = re.match(r"([a-zA-Z0-9]+)", s.replace(" ", ""))
	if match:
		return "a" + match.group(1)[:max_length]
	else:
		return "string"

class CodeGen:
	def __init__(self, verbose: bool = False, is64Bit: bool = True, panic=defaultPanic) -> None:
		self.verbose = verbose
		self.is64Bit = is64Bit
		self.panic = panic

	def _log(self, fmt, *args):
		if self.verbose: print("", fmt % args)

	def generate(self, root: ModuleNode) -> bytes:
		gen: pygen.COFFGenerator = pygen.COFFGenerator(self.is64Bit)
		self.gen = gen

		self.data = bytearray()
		self.rdata = bytearray()

		self._stringcache = {}
		self._modules = {}

		text: pygen.COFFSection = self.gen.section(".text", 0x60000020)
		self.emit = text.emit
		self.emitInsn = (lambda x: text.emit(b"\x48" + x)) if self.is64Bit else text.emit
		self.emitImm = lambda x, y=None, z=False: self.emit(x.to_bytes((8 if self.is64Bit else 4) if y is None else y, ENDIAN, signed=z))
		self.text = text

		codeOffset = len(text.data)
		self._emitCode(root)
		codeSize = len(text.data) - codeOffset

		self._log("Code Size: 0x%X", codeSize)

		if len(self.rdata) > 0:
			rdata: pygen.COFFSection = self.gen.section(".rdata", 0x40000040)
			rdata.data = self.rdata
		if len(self.data) > 0:
			data: pygen.COFFSection = self.gen.section(".data", 0xC0000040)
			data.data = self.data
		
		self._log("Data Size: 0x%X", len(self.data))
		self._log("Read-only Data Size: 0x%X", len(self.rdata))
		
		del self.emit, self.emitImm, self.emitInsn, self.text, self.data, self.rdata, self._stringcache, self._modules
		return gen.emit()

	def _emitCode(self, root: ModuleNode) -> None:
		self._subroutines = {}

		for node in root.body:
			if type(node) == FunctionNode:
				self._emitFunction(node)

			elif type(node) == FunctionDeclarationNode:
				self._subroutines[node.name] = (-1, node)
			
			elif type(node) == UsingNode:
				module = {}
				if node.path is None:
					self.panic("[ERR] unresolved module %s", node.name)
					continue
				with open(node.path, "r") as f:
					module = json.load(f)
					f.close()
				self._log("Loading Module '%s' (format %d, %d function declarations)", node.name, module["format"], len(module["functions"]))
				formatted = {}
				for name, func in module["functions"].items():
					if type(func) == dict and module["format"] >= 2:
						formatted[name] = func
					elif type(func) == str and module["format"] >= 1:
						formatted[name] = {
							"name": func,
							"vararg": False,
							"params": -1
						}
					else:
						self.panic("[ERR] module '%s' has invalid format", node.name)
				self._modules[node.name] = formatted

			else:
				self.panic("[ERR] invalid root statement '%s'", type(node).__name__)

		del self._subroutines

	def _emitBlock(self, root: Node) -> None:
		self._curBlock		= root
		self._locals		= {}
		self._localOffset	= 0

		for node in root.body:
			if type(node) == ReturnNode:
				self._emitExpression(node.value)
				self.freea(self._localOffset)
				self.emit(b"\xC9\xC3")				# leave
													# ret
			
			elif type(node) == CallNode:
				self._emitCall(node)

			elif type(node) == IfNode:
				endJumps = []

				self._emitExpression(node.condition)
				self.emitInsn(b"\x85\xC0")			# test rax/eax, rax/eax
				jzList = [self.emit(b"\x74\x00")]	# jz [rip + offset]

				oldBlockData = (self._locals, self._localOffset, self._curBlock)
				self._emitBlock(node)
				self._locals, self._localOffset, self._curBlock = oldBlockData

				endJumps.append(self.emit(b"\xEB\x00"))

				for elseif in node.elseIf:
					self.text.data[jzList[-1] + 1] = len(self.text.data) - (jzList[-1] + 2)

					self._emitExpression(elseif.condition)
					self.emitInsn(b"\x85\xC0")
					jzList.append(self.emit(b"\x74\x00"))

					oldBlockData = (self._locals, self._localOffset, self._curBlock)
					self._emitBlock(elseif)
					self._locals, self._localOffset, self._curBlock = oldBlockData

					endJumps.append(self.emit(b"\xEB\x00"))

				if node.else_:
					self.text.data[jzList[-1] + 1] = len(self.text.data) - (jzList[-1] + 2)
					oldBlockData = (self._locals, self._localOffset, self._curBlock)
					self._emitBlock(node.else_)
					self._locals, self._localOffset, self._curBlock = oldBlockData
				else:
					self.text.data[jzList[-1] + 1] = len(self.text.data) - (jzList[-1] + 2)

				for jmp in endJumps:
					self.text.data[jmp + 1] = len(self.text.data) - (jmp + 2)

			elif type(node) == VariableNode:
				size: int = Type.size(node.dataType, self.is64Bit)
				aligned: int = self.alloca(size)
				self._localOffset += aligned
				self._locals[node.name] = self._localOffset

				if node.value is not None:
					self._emitExpression(node.value)
					self.emitInsn(b"\x89\x85")		# mov [rbp/ebp + imm32], rax/eax
					self.emitImm(-self._localOffset, 4, True)

			elif type(node) == AssignNode:
				if node.lhs not in self._locals:
					self.panic("[ERR] invalid left-hand operator in assignment")
					continue
				offset: int = -self._locals[node.lhs]
				
				if node.extra is None:
					self._emitExpression(node.rhs)
					self.emitInsn(b"\x89\x85")		# mov [rbp/ebp + imm32], rax/eax
					self.emitImm(offset, 4, True)

				elif node.extra == "+":
					self._emitExpression(node.rhs)
					self.emitInsn(b"\x01")			# add [rbp/ebp + imm32], rax/eax
					if -128 <= offset <= 127:
						self.emit(b"\x45")
						self.emitImm(offset, 1, True)
					else:
						self.emit(b"\x85")
						self.emitImm(offset, 4, True)

				elif node.extra == "-":
					self._emitExpression(node.rhs)
					self.emitInsn(b"\x29")			# sub [rbp/ebp + imm32], rax/eax
					if -128 <= offset <= 127:
						self.emit(b"\x45")
						self.emitImm(offset, 1, True)
					else:
						self.emit(b"\x85")
						self.emitImm(offset, 4, True)

				elif node.extra == "*":
					self._emitExpression(node.rhs)
					self.emitInsn(b"\x8B")			# mov rbx, [rbp/ebp + imm32]
					if -128 <= offset <= 127:
						self.emit(b"\x5D")
						self.emitImm(offset, 1, True)
					else:
						self.emit(b"\x9D")
						self.emitImm(offset, 4, True)

					self.emitInsn(b"\x0F\xAF\xD8")	# imul rbx, rax

					self.emitInsn(b"\x89")			# mov [rbp/ebp + imm32], rbx
					if -128 <= offset <= 127:
						self.emit(b"\x5D")
						self.emitImm(offset, 1, True)
					else:
						self.emit(b"\x9D")
						self.emitImm(offset, 4, True)

				elif node.extra == "/":
					self._emitExpression(node.rhs, 1)
					self.emitInsn(b"\x8B")			# mov rax, [rbp/ebp + imm32]
					if -128 <= offset <= 127:
						self.emit(b"\x45")
						self.emitImm(offset, 1, True)
					else:
						self.emit(b"\x85")
						self.emitImm(offset, 4, True)

					self.emitInsn(b"\x99")			# cqo/cdq
					self.emitInsn(b"\xF7\xFB")		# idiv rbx/ebx

					self.emitInsn(b"\x89")			# mov [rbp/ebp + imm32], rax
					if -128 <= offset <= 127:
						self.emit(b"\x45")
						self.emitImm(offset, 1, True)
					else:
						self.emit(b"\x85")
						self.emitImm(offset, 4, True)
				
				else:
					self.panic("[ERR] invalid assignment operator '%s='", node.extra)

			else:
				self.panic("[ERR] invalid statement '%s' in function body", type(node).__name__)

		self._log("Leaving block '%s' (%d locals)", getattr(root, "name", "<unnamed>"), len(self._locals))
		del self._locals, self._localOffset, self._curBlock

	def _emitFunction(self, root: FunctionNode):
		self._log("Entering function '%s %s func %s(...)' (%d arguments)",
				"public" if root.storage == STORAGE_PUBLIC else "private",
				root.conv,
				root.name,
				len(root.args)
			)

		self._subroutines[root.name] = (len(self.text.data), root)
		if root.storage == STORAGE_PUBLIC:
			self.gen.symbol(root.name, len(self.text.data), 1, 0x20)

		self._curFunc		= root
		self._arguments		= tuple(root.args.keys())

		self.emit(b"\x55")						# push rbp/ebp
		self.emitInsn(b"\x89\xE5")				# mov rbp, rsp

		self._emitBlock(root)
		
		del self._curFunc, self._arguments

	# TODO: add checks for consntant values for optimization
	# and overall optimize this heavily
	def _emitExpression(self, expr: ExpressionNode | LarkToken, target: int = 0, **kw):
		if isinstance(expr, ExpressionNode):
			if type(expr) == AddNode:
				self._emitExpression(expr.left, 0, **kw)
				self._emitExpression(expr.right, 1, **kw)
				self.emitInsn(b"\x01\xD8")		# add rax/eax, rbx/ebx

			elif type(expr) == SubNode:
				self._emitExpression(expr.left, 0, **kw)
				self._emitExpression(expr.right, 1, **kw)
				self.emitInsn(b"\x29\xD8")		# sub rax/eax, rbx/ebx

			elif type(expr) == MulNode:
				self._emitExpression(expr.left, 0, **kw)
				self._emitExpression(expr.right, 1, **kw)
				self.emitInsn(b"\x0F\xAF\xC3")	# imul rax/eax, rbx/ebx

			elif type(expr) == DivNode:
				self._emitExpression(expr.left, 0,**kw)
				self._emitExpression(expr.right, 1, **kw)
				self.emitInsn(b"\x99")			# cqo/cdq
				self.emitInsn(b"\xF7\xFB")		# idiv rbx/ebx
			
			elif type(expr) == EqNode:
				self._emitExpression(expr.left, 0, **kw)
				self._emitExpression(expr.right, 1, **kw)
				self.emitInsn(b"\x3B\xC3")		# cmp rax/eax, rbx/ebx
				self.emitInsn(b"\x0F\x94\xC0")	# setz al
			
			elif type(expr) == NeqNode:
				self._emitExpression(expr.left, 0, **kw)
				self._emitExpression(expr.right, 1, **kw)
				self.emitInsn(b"\x3B\xC3")		# cmp rax/eax, rbx/ebx
				self.emitInsn(b"\x0F\x95\xC0")	# setne al

			elif type(expr) == AndNode:
				self._emitExpression(expr.left, 0, **kw)
				self._emitExpression(expr.right, 1, **kw)
				self.emitInsn(b"\x21\xD8")		# and rax/eax, rbx/ebx

			elif type(expr) == OrNode:
				self._emitExpression(expr.left, 0, **kw)
				self._emitExpression(expr.right, 1, **kw)
				self.emitInsn(b"\x09\xD8")		# or rax/eax, rbx/ebx
			
			elif type(expr) == LtNode:
				self._emitExpression(expr.left, 0, **kw)
				self._emitExpression(expr.right, 1, **kw)
				self.emitInsn(b"\x3B\xC3")		# cmp rax/eax, rbx/ebx
				self.emit(b"\x0F\x9C\xC0")		# setl al

			elif type(expr) == LeNode:
				self._emitExpression(expr.left, 0, **kw)
				self._emitExpression(expr.right, 1, **kw)
				self.emitInsn(b"\x3B\xC3")		# cmp rax/eax, rbx/ebx
				self.emit(b"\x0F\x9E\xC0")		# setle al

			elif type(expr) == GtNode:
				self._emitExpression(expr.left, 0, **kw)
				self._emitExpression(expr.right, 1, **kw)
				self.emitInsn(b"\x3B\xC3")		# cmp rax/eax, rbx/ebx
				self.emit(b"\x0F\x9F\xC0")		# setg al

			elif type(expr) == GeNode:
				self._emitExpression(expr.left, 0, **kw)
				self._emitExpression(expr.right, 1, **kw)
				self.emitInsn(b"\x3B\xC3")		# cmp rax/eax, rbx/ebx
				self.emit(b"\x0F\x9D\xC0")		# setge al
			
			elif type(expr) == NotNode:
				self._emitExpression(expr.operand, 0, **kw)
				self.emitInsn(b"\xF7\xD0")	# not rax/eax

			else:
				self.panic("[ERR] unsupported expression node '%s'", type(expr).__name__)
		
		elif type(expr) == CallNode:
			self._emitCall(expr, **kw)
			if target == 1:
				self.emitInsn(b"\x89\xC3")
			# result is already in rax
		
		elif type(expr) == LarkToken:
			if expr.type == "INT":
				if target == 0:
					self.emitInsn(b"\xB8")				# mov rax, imm32/imm64
				elif target == 1:
					self.emitInsn(b"\xBB")				# mov rbx, imm32/imm64
				else:
					self.panic("INTRN_INVALID_EXPR_TARGET")
				self.emitImm(expr.value, None, expr.value < 0)

			elif expr.type == "IDENTF" and expr.value in self._arguments:
				if self._curFunc.conv == "__cdecl":
					index = (self._arguments.index(expr.value) + 2) * (8 if self.is64Bit else 4)
					if target == 0:
						self.emitInsn(b"\x8B\x45")		# mov rax, [rbp + imm8/imm32]
					elif target == 1:
						self.emitInsn(b"\x8B\x5D")		# mov rbx, [rbp + imm8/imm32]
					else:
						self.panic("INTRN_INVALID_EXPR_TARGET")
					self.emitImm(index, 1 if index <= 0x7F else 4)
				elif self._curFunc.conv == "__fastcall" and self.is64Bit:
					index = self._arguments.index(expr.value)
					if index < 4:
						self.emit(FASTCALL_ARGSGET[index + (target * 4)])
					else:
						index = ((index - 4) + 2) * 8
						if target == 0:
							self.emitInsn(b"\x8B\x45")	# mov rax, [rbp + imm8/imm32]
						elif target == 1:
							self.emitInsn(b"\x8B\x5D")	# mov rbx, [rbp + imm8/imm32]
						else:
							self.panic("INTRN_INVALID_EXPR_TARGET")
						self.emitImm(index, 1 if index <= 0x7F else 4)
				else:
					self.panic("[ERR] unsupported calling convention '%s'", self._curFunc.conv)

			elif expr.type == "IDENTF" and expr.value in self._locals:
				index = -self._locals[expr.value]
				
				self.emitInsn(b"\x8B")					# mov rax/eax, [rbp/ebp - imm8/imm32]
				if -128 <= index <= 127:
					self.emit(b"\x45")
					self.emitImm(index, 1, True)
				else:
					self.emit(b"\x85")
					self.emitImm(index, 4, True)
				
			elif expr.type == "STRING":
				sym = self._stringcache.get(expr.value, None)
				if sym is None:
					stringOffset = len(self.rdata)
					self.rdata.extend(expr.value.encode() + b"\x00")
					self.gen.symbol(str2name(expr.value), stringOffset, 2, 3)
					sym = self.gen.header.NumberOfSymbols - 1
					self._stringcache[expr.value] = sym
				
				if target == 0:
					self.emitInsn(b"\x8D\x05")			# lea rax/eax, [rip/eip + sym]
				elif target == 1:
					self.emitInsn(b"\x8D\x1D")			# lea rbx/ebx, [rip/eip + sym]
				offset = self.emitImm(0, 4)
				self.text.reloc(offset, sym, 4)

			else:
				self.panic("[ERR] undefined symbol '%s' in expression", expr.value)

		else:
			self.panic("[ERR] unsupported node '%s' in expression", type(expr).__name__)
		
	def _emitCall(self, node: CallNode, preserve: bool = False):
		cleanup = 0
		preserved = 0
		scopedName = "::".join(node.scope + [node.name])
		conv = node.function.conv if node.function is not None else "__fastcall"
		if len(node.scope) > 0:
			moduleFunc = self._modules.get(node.scope[0], {}).get(node.name, None)

		self._log("Call to '%s' (using '%s' calling convention)", scopedName, conv)

		if conv == "__cdecl":
			for arg in reversed(node.args):
				self._emitExpression(arg)
				self.emit(b"\x50")
				cleanup += 8 if self.is64Bit else 4
		elif conv == "__fastcall" and self.is64Bit:
			cleanup = self.alloca(0x20) # __fastcall needs 32 bytes of shadow space

			for i, arg in enumerate(node.args[:4]):
				if preserve:
					self.emit(FASTCALL_ARGSPRESERVE[i])
					preserved += 1
				self._emitExpression(arg, preserve=True)
				self.emit(FASTCALL_ARGSSET[i])
			
			for arg in node.args[4:]:
				self._emitExpression(arg)
				self.emit(b"\x50")
				cleanup += 8 if self.is64Bit else 4
		else:
			self.panic("[ERR] unsupported calling convention '%s'", conv)
		self._log("Stack cleanup: %d bytes", cleanup)

		self.emit(b"\xE8")		# call [rip/eip + imm32]
		info = self._subroutines.get(scopedName, (-1, None))

		if info[0] != -1 and type(info[1]) == FunctionNode and len(node.scope) == 0:
			self.emitImm(self._subroutines[node.name][0] - (len(self.text.data) + 4), 4, True)

		elif info[0] == -1 and type(info[1]) == FunctionDeclarationNode:
			offset = self.emitImm(0, 4)
			self.gen.symbol(node.name, 0, 0, 0x20)
			self.text.reloc(offset, self.gen.header.NumberOfSymbols - 1, 4)
		
		elif info[0] == -1 and moduleFunc is not None:
			offset = self.emitImm(0, 4)
			self.gen.symbol(moduleFunc["name"], 0, 0, 0x20)
			self.text.reloc(offset, self.gen.header.NumberOfSymbols - 1, 4)
		
		else:
			self.panic("[ERR] undefined symbol '%s' referenced in function '%s'", scopedName, self._curFunc.name)

		for i in range(preserved):
			self.emit(FASTCALL_ARGSRESTORE[i])
		self.freea(cleanup)
		
	def alloca(self, size: int) -> int:
		# TODO: it would be great to modify the closest previous alloca instead of emitting a new one
		if size > 0:
			align = (7 if self.is64Bit else 3)
			aligned = (size + align) & ~align

			self.emitInsn(b"\x83\xEC")	# sub rsp, imm8/imm3
			if aligned <= 0x10:
				self.emitImm(aligned, 1)
			else:
				self.emitImm(aligned, 1)

			return aligned
		return 0
	def freea(self, size: int) -> int:
		# TODO: it would be great to modify the closest previous freea instead of emitting a new one
		if size > 0:
			align = (7 if self.is64Bit else 3)
			aligned = (size + align) & ~align

			self.emitInsn(b"\x83\xC4")	# sub rsp, imm8/imm32
			if aligned <= 0x10:
				self.emitImm(aligned, 1)
			else:
				self.emitImm(aligned, 1)

			return aligned
		return 0
