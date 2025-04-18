import re
import sys
import json

from lark import Token as LarkToken

from parser.nodes import *
import codegen.pygen as pygen

ENDIAN			= sys.byteorder
STORAGE_PRIVATE	= STORAGE2I["private"]
STORAGE_PUBLIC	= STORAGE2I["public"]
FASTCALL_ARGSSET = [
	b"\x48\x89\xC1",		# mov rcx, rax
	b"\x48\x89\xC2",		# mov rdx, rax
	b"\x49\x89\xC0",		# mov r8, rax
	b"\x49\x89\xC1",		# mov r9, rax
]
FASTCALL_ARGSGET = [
	b"\x48\x8B\xC1",		# mov rax, rcx
	b"\x48\x89\xD0",		# mov rax, rdx
	b"\x49\x89\xC1",		# mov rax, r8
	b"\x49\x89\xC1",		# mov rax, r9

	b"\x48\x89\xCB",		# mov rbx, rcx
	b"\x48\x89\xD3",		# mov rbx, rdx
	b"\x4C\x89\xC3",		# mov rbx, r8
	b"\x4C\x89\xCB",		# mov rbx, r9
]


def defaultPanic(fmt: str, *args):
	print(fmt % args)
	exit(1)

def str2name(s: str, max_length: int = 8):
    s = "".join(c for c in s if 32 <= ord(c) < 127)
    s = re.sub(r"^[^a-zA-Z]+", "", s)
    match = re.match(r"([a-zA-Z0-9]+)", s)
    if match:
        word = match.group(1)
        return "a" + word[:max_length]
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

	def _emitFunction(self, root: FunctionNode):
		self._log("In function '%s %s func %s(...)' (%d arguments)",
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
		self._locals		= {}
		self._localOffset	= 0

		self.emit(b"\x55")						# push rbp/ebp
		self.emitInsn(b"\x89\xE5")				# mov rbp, rsp

		for node in root.body:
			if type(node) == ReturnNode:
				self._emitExpression(node.value)
				self.freea(self._localOffset)
				self.emit(b"\x5D\xC3")			# pop rbp
												# ret
			
			elif type(node) == CallNode:
				self._emitCall(node)

			elif type(node) == VariableNode:
				size: int = Type.size(node.dataType, self.is64Bit)
				aligned: int = self.alloca(size)
				self._localOffset += aligned
				self._locals[node.name] = self._localOffset

				if node.value is not None:
					self._emitExpression(node.value)
					self.emitInsn(b"\x89\x85")	# mov [rbp + imm32], rax
					self.emitImm(-self._localOffset, 4, True)

			else:
				self.panic("[ERR] unsupported node '%s' in function body", type(node).__name__)
		
		del self._curFunc, self._arguments, self._locals, self._localOffset

	def _emitExpression(self, expr: ExpressionNode | LarkToken, target: int = 0):
		if isinstance(expr, ExpressionNode):
			if isinstance(expr, AddNode):
				self._emitExpression(expr.left, 0)
				self._emitExpression(expr.right, 1)
				self.emitInsn(b"\x01\xD8")		# add rax/eax, rbx/ebx

			elif isinstance(expr, SubNode):
				self._emitExpression(expr.left, 0)
				self._emitExpression(expr.right, 1)
				self.emitInsn(b"\x29\xD8")		# sub rax/eax, rbx/ebx

			elif isinstance(expr, MulNode):
				self._emitExpression(expr.left, 0)
				self._emitExpression(expr.right, 1)
				self.emitInsn(b"\x0F\xAF\xC3")	# imul rax/eax, rbx/ebx

			elif isinstance(expr, DivNode):
				self._emitExpression(expr.left)
				self._emitExpression(expr.right)
				self.emitInsn(b"\x99")			# cqo/cdq
				self.emitInsn(b"\xF7\xFB")		# idiv rbx/ebx

			else:
				self.panic("[ERR] unsupported expression node '%s'", type(expr).__name__)
		
		elif type(expr) == CallNode:
			self._emitCall(expr)
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
				self.emitImm(expr.value)

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
		
	def _emitCall(self, node: CallNode):
		cleanup = 0
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
			self.alloca(0x20)
			cleanup = 0x20

			for i, arg in enumerate(node.args[:4]):
				self._emitExpression(arg)
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
