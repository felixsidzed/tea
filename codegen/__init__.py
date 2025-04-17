import sys

from lark import Token as LarkToken

from parser.nodes import *
import codegen.pygen as pygen

ENDIAN			= sys.byteorder
STORAGE_PRIVATE	= STORAGE2I["private"]
STORAGE_PUBLIC	= STORAGE2I["public"]


def defaultPanic(fmt, *args):
	print(fmt % args)
	exit(1)

class CodeGen:
	def __init__(self, is64Bit: bool = True, panic=defaultPanic) -> None:
		self.is64Bit = is64Bit
		self.panic = panic

	def generate(self, root: ModuleNode) -> bytes:
		gen: pygen.COFFGenerator = pygen.COFFGenerator(self.is64Bit)
		self.gen = gen

		self.data = bytearray()
		self.rdata = bytearray()

		text: pygen.COFFSection = self.gen.section(".text", 0x60000020)
		self.emit = text.emit
		self.emitInsn = (lambda x: text.emit(b"\x48" + x)) if self.is64Bit else text.emit
		self.emitImm = lambda x, y=None, z=False: self.emit(x.to_bytes((8 if self.is64Bit else 4) if y is None else y, ENDIAN, signed=z))
		self.text = text

		self._emitCode(root)

		if len(self.rdata) > 0:
			rdata: pygen.COFFSection = self.gen.section(".rdata", 0x40000040)
			rdata.data = self.rdata
		if len(self.data) > 0:
			data: pygen.COFFSection = self.gen.section(".data", 0xC0000040)
			data.data = self.data
		
		del self.emit, self.emitImm
		return gen.emit()

	def _emitCode(self, root: ModuleNode) -> None:
		self._subroutines = {}

		for node in root.body:
			if type(node) == FunctionNode:
				self._emitFunction(node)

			else:
				self.panic("[ERR] invalid root node '%s'", type(node).__name__)

		del self._subroutines

	def _emitFunction(self, root: FunctionNode):
		self._subroutines[root.name] = len(self.text.data)
		if root.storage == STORAGE_PUBLIC:
			self.gen.symbol(root.name, len(self.text.data), 1, 0x20)

		self.emit(b"\x55")				# push rbp/ebp
		self.emitInsn(b"\x89\xE5")		# mov  rbp/ebp, rsp/esp

		self._curFunc		= root
		self._arguments		= tuple(root.args.keys())
		self._locals		= {}
		self._localOffset	= 0

		for node in root.body:
			if type(node) == ReturnNode:
				self._emitExpression(node.value)
				self.freea(self._localOffset)
				self.emit(b"\x5D\xC3")	# pop rbp
										# ret
				del self._curFunc, self._arguments, self._locals, self._localOffset
				return
			
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
				self.emitInsn(b"\x89\xc3")
		
		elif type(expr) == LarkToken:
			if expr.type == "INT":
				if target == 0:
					self.emitInsn(b"\xB8")		# mov rax, imm32/imm64
				elif target == 1:
					self.emitInsn(b"\xBB")		# mov rbx, imm32/imm64
				else:
					self.panic("INTRN_INVALID_EXPR_TARGET")
				self.emitImm(expr.value)

			elif expr.type == "IDENTF" and expr.value in self._arguments:
				if self._curFunc.conv == "__cdecl":
					index = (self._arguments.index(expr.value) + 2) * (8 if self.is64Bit else 4)
					if target == 0:
						self.emitInsn(b"\x8B\x45")	# mov rax, [rbp + imm8/imm32]
					elif target == 1:
						self.emitInsn(b"\x8B\x5D")	# mov rbx, [rbp + imm8/imm32]
					else:
						self.panic("INTRN_INVALID_EXPR_TARGET")
					if index <= 0x7F:
						self.emitImm(index, 1)
					else:
						self.emitImm(index, 4)
				else:
					self.panic("[ERR] unsupported calling convention '%s'", self._curFunc.conv)

			elif expr.type == "IDENTF" and expr.value in self._locals:
				index = -self._locals[expr.value]
				# TODO: use ebp for 32bit
				# TODO: imm32
				self.emit(b"\x48\x8B\x45")			# mov rax, [rbp - imm8/imm32]
				self.emitImm(index, 1, True)

			else:
				self.panic("[ERR] undefined symbol '%s' in expression", expr.value)

		else:
			self.panic("[ERR] unsupported node '%s' in expression", type(expr).__name__)
		
	def _emitCall(self, node: CallNode):
		conv = node.function.conv if node.function is not None else "__fastcall"
		cleanup = 0
		if conv == "__cdecl":
			for arg in reversed(node.args):
				self._emitExpression(arg)
				self.emit(b"\x50")
				cleanup += 8 if self.is64Bit else 4
		else:
			self.panic("[ERR] unsupported calling convention '%s'", conv)

		self.emit(b"\xE8")
		if node.name in self._subroutines:
			self.emit((-(len(self.text.data) + 4)).to_bytes(4, ENDIAN, signed=True))

		self.freea(cleanup)
		
	def alloca(self, size: int) -> int:
		# TODO: it would be great to modify the previous alloca instead of emitting a new one
		if size > 0:
			align = (7 if self.is64Bit else 3)
			aligned = (size + align) & ~align

			self.emitInsn(b"\x83\xEC")	# sub rsp, imm8/imm3
			if aligned <= 0x10:
				self.emitImm(aligned, 1)
			else:
				self.emitImm(aligned, 4)

			return aligned
		return 0
	def freea(self, size: int) -> int:
		# TODO: it would be great to modify the previous freea instead of emitting a new one
		if size > 0:
			align = (7 if self.is64Bit else 3)
			aligned = (size + align) & ~align

			self.emitInsn(b"\x83\xC4")	# sub rsp, imm8/imm32
			if aligned <= 0x10:
				self.emitImm(aligned, 1)
			else:
				self.emitImm(aligned, 4)

			return aligned
		return 0
