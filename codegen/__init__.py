import sys
import struct
from parser.nodes import *
from datetime import datetime
from lark import Token as LarkToken

ENDIAN = sys.byteorder
STORAGE_PRIVATE = STORAGE2I["private"]
STORAGE_PUBLIC = STORAGE2I["public"]


def defaultPanic(fmt, *args):
	print(fmt % args)
	exit(1)

EXPR2BYTES = {
	AddNode: (b"\x01\xd8",),
	SubNode: (b"\x29\xd8",),
	MulNode: (b"\x0f\xaf\xc3",),
	DivNode: (b"\x31\xd2", b"\xf7\xfb"),
}

class CodeGen:
	def __init__(self, emit64: bool = True, panic=defaultPanic) -> None:
		self.emit64 = emit64
		self.panic = panic

	def generate(self, root: ModuleNode) -> bytearray:
		self.output = bytearray()
		self.symbols = {}
		self.subroutines = {}
		self.reloc = []
		self.stringTable = bytearray(b"\x00\x00\x00\x00")

		self.__allocacache = None
		self.__freescache = None
		self.__externed = {}

		COFFHeaderOffset = len(self.output)
		self.emit(b"\x00" * 20)

		textHeaderOffset = len(self.output)
		self.emit(b"\x00" * 40)

		codeOffset = len(self.output)
		self.emitCode(root)
		codeSize = len(self.output) - codeOffset

		relocOffset = len(self.output)
		self.emit(b"\x00" * (10 * len(self.reloc)))

		symbolCount = 0
		symbolIndices = {}
		symbolTableOffset = len(self.output)
		for name, sym in self.symbols.items():
			if len(name) <= 8:
				bname = name.encode().ljust(8, b"\x00")
			else:
				offset = len(self.stringTable)
				self.stringTable += name.encode() + b"\x00"
				bname = struct.pack("@II", 0, offset)

			self.emit(bname)
			self.emit(struct.pack("@LhHBB", sym["val"], sym["sec"], sym["ty"], sym["scl"], sym["nx"]))
			symbolIndices[name] = symbolCount
			symbolCount += 1

		for reloc in self.reloc:
			if reloc["symbol"] not in symbolIndices:
				name = reloc["symbol"]
				if len(name) <= 8:
					bname = name.encode().ljust(8, b"\x00")
				else:
					offset = len(self.stringTable)
					self.stringTable += name.encode() + b"\x00"
					bname = struct.pack("@II", 0, offset)

				self.emit(bname)
				self.emit(struct.pack("@LhHBB", 0, 0, 0x20, 2, 0))
				symbolIndices[name] = symbolCount
				symbolCount += 1

		stringTableSize = len(self.stringTable)
		self.stringTable[0:4] = struct.pack("@I", stringTableSize)
		self.output += self.stringTable

		for i, reloc in enumerate(self.reloc, 1):
			sym = symbolIndices.get(reloc["symbol"], None)
			if sym is None:
				raise ValueError(f"invalid symbol {reloc["symbol"]}")

			self.output[relocOffset + 10 * (i - 1):relocOffset + 10 * i] = struct.pack("@IIH",
				reloc["offset"] - 60,
				sym,
				reloc["type"],
			)

		self.output[COFFHeaderOffset:COFFHeaderOffset + 20] = struct.pack("@HHLLLHH",
			0x8664 if self.emit64 else 0x14C, # machine
			1,                                # number of sections
			int(datetime.now().timestamp()),  # timestamp
			symbolTableOffset,                # ptr to symbol table
			symbolCount,                      # number of symbols
			0,                                # size of optional header
			0x0000                            # characteristics
		)

		self.output[textHeaderOffset:textHeaderOffset + 40] = struct.pack("@8sLLLLLLHHL",
			b".text\x00\x00\x00",  # name
			codeSize,             # virtual size
			0x0000,               # virtual addr
			codeSize,             # size of raw data
			codeOffset,           # ptr to raw data
			relocOffset,		  # ptr to relocations
			0,	  				  # ptr to line numbers
			len(self.reloc),	  # number of relocations
			0,					  # number of line numbers
			0x60000020            # characteristics
		)

		del self.symbols, self.subroutines, self.reloc, self.stringTable

		# this should never happen
		if hasattr(self, "activeFuncArgs"):
			del self.activeFuncArgs
		if hasattr(self, "activeLocals"):
			del self.activeLocals

		return self.output

	def emitCode(self, root: ModuleNode):
		HEADERSIZE = len(self.output)
		for node in root.body:
			if isinstance(node, FunctionNode):
				self.subroutines[node.name] = len(self.output) - HEADERSIZE
				if node.storage == STORAGE_PUBLIC:
					self.symbols[node.name] = {
						"val": len(self.output) - HEADERSIZE,
						"sec": 1,
						"ty": 0x20,
						"scl": 2,
						"nx": 0
					}
				self.emitFunction(node)

			elif isinstance(node, UsingNode):
				# TODO
				continue

			elif isinstance(node, ExternNode):
				self.__externed[node.name] = {
					"storage": node.storage,
					"returnType": node.returnType,
					"args": node.args,
					"conv": node.convention
				}

			else:
				return self.panic(f"[ERR] invalid root node '{type(node)}'")

	def emitFunction(self, fn: FunctionNode) -> None:
		self.emit(b"\x55")
		self.emitOp(b"\x89\xe5")

		self.activeFuncArgs = tuple(fn.args.keys())
		self.activeLocals = {}
		localOffset = 0

		for node in fn.body:
			if type(node) == ReturnNode:
				if isinstance(node.value, ExpressionNode) or type(node.value) == CallNode or type(node.value) == LarkToken:
					self.emitExpr(node.value)

				elif not isinstance(node.value, Node):
					self.emitOp(b"\xb8")
					self.emitImm(node.value)

				else:
					self.panic(f"[ERR] invalid return value node '{type(node)}'")

				self.frees(-localOffset)
				del self.activeFuncArgs, self.activeLocals
				self.emit(b"\x5d\xc3")
				return # maybe this isnt necessary
		
			elif type(node) == VariableNode:
				size = Type.size(node.dataType)
				alignedSize = self.alloca(size)
				localOffset -= alignedSize

				if node.value is not None:
					self.emitExpr(node.value)
					self.emitOp(b"\x89\x45")
					if localOffset <= 0x7F:
						self.emit(localOffset.to_bytes(1, ENDIAN, signed=True))
					else:
						self.emit(localOffset.to_bytes(4, ENDIAN, signed=True))
				self.activeLocals[node.name] = localOffset

			elif type(node) == CallNode:
				self.emitCall(node)
			
			else:
				return self.panic(f"[ERR] unsupported node '{type(node)}'")

	def emit(self, b: bytes) -> None:
		self.output.extend(b)
	def emitOp(self, b: bytes) -> None:
		if self.emit64:
			self.emit(b"\x48")
		self.emit(b)
	def emitImm(self, value: int, **kw) -> None:
		self.output.extend(value.to_bytes(
			8 if self.emit64 else 4,
			ENDIAN,
			**kw
		))

	def emitExpr(self, node: ExpressionNode | LarkToken, target: str = "rax") -> None:
		if type(node) == CallNode:
			self.emitCall(node)

		elif isinstance(node, ExpressionNode):
			self.emitExpr(node.left)
			self.emitExpr(node.right, "rbx")

			for insn in EXPR2BYTES[type(node)]:
				self.emitOp(insn)

		elif type(node) == LarkToken:
			if node.type == "INT":
				if target == "rax":
					self.emitOp(b"\xb8")
				elif target == "rbx":
					self.emitOp(b"\xbb")
				else:
					return self.panic(f"[ERR] unsupported register target '{target}'")
				self.emitImm(node.value)

			elif node.type == "IDENTF" and node.value in self.activeFuncArgs:
				argIndex = self.activeFuncArgs.index(node.value)
				offset = 16 + argIndex * (8 if self.emit64 else 4)

				if target == "rax":
					self.emitOp(b"\x8b\x45")
				elif target == "rbx":
					self.emitOp(b"\x8b\x5d")
				else:
					return self.panic(f"ERR_INTERNAL_INVALID_EXPR_REG")

				self.emit(offset.to_bytes(1, ENDIAN))
			elif node.type == "IDENTF" and node.value in self.activeLocals:
				localOffset = self.activeLocals[node.value]
				self.emitOp(b"\x8b\x45")
				if localOffset <= 0x7F:
					self.emit(localOffset.to_bytes(1, ENDIAN, signed=True))
				else:
					self.emit(localOffset.to_bytes(4, ENDIAN, signed=True))
			else:
				return self.panic(f"[ERR] unsupported constant type in expression '{node.type}'")
		else:
			return self.panic(f"[ERR] invalid expression node '{type(node)}'")

	def emitCall(self, node: CallNode) -> None:
		conv = self.__externed.get(node.name, { "conv": "__cdecl" })["conv"]
		cleanup = None

		if conv == "__fastcall":
			instructions = [b"\x48\x89\xc1", b"\x48\x89\xc2", b"\x49\x89\xc0", b"\x49\x89\xc1"]
			stackArgs = 0

			for i, arg in enumerate(reversed(node.args[:4])):
				if type(arg) == LarkToken:
					self.emitExpr(arg)
					self.emit(instructions[i])
				else:
					return self.panic(f"[ERR] unsupported argument type '{type(arg)}'")

			for arg in reversed(node.args[4:]):
				if type(arg) == LarkToken:
					self.emitExpr(arg)
					self.emit(b"\x50")

					size = 8 if self.emit64 else 4
					stackArgs += size
					cleanup += size
				else:
					return self.panic(f"[ERR] unsupported argument type '{type(arg)}'")
		else:
			cleanup = 0
			for arg in reversed(node.args):
				if type(arg) == LarkToken:
					self.emitExpr(arg)
					self.emit(b"\x50")
					cleanup += 8 if self.emit64 else 4 # TODO: this is a guess
				else:
					return self.panic(f"[ERR] unsupported argument type '{type(arg)}'")

		funcName = "::".join(node.scope + [node.name])
		self.emit(b"\xe8")
		if funcName in self.subroutines:
			self.emit((-(len(self.output) - 60 + 4)).to_bytes(4, ENDIAN, signed=True))
		else:
			callee = len(self.output)
			self.emit(b"\x00\x00\x00\x00")

			self.reloc.append({
				"symbol": funcName,
				"offset": callee,
				"type": 4,  # rel 32 bit
			})

		if cleanup is not None:
			self.frees(cleanup)

	def alloca(self, size: int):
		if size > 0:
			align = 8 if self.emit64 else 4
			aligned = (size + (align - 1)) & ~(align - 1)

			if self.__allocacache:
				offset, current = self.__allocacache
				new_size = current + aligned
				self.output[offset] = new_size.to_bytes(1, ENDIAN)[0]
				self.__allocacache = (offset, new_size)
			else:
				self.emitOp(b"\x83\xec")
				offset = len(self.output)
				self.emit(aligned.to_bytes(1, ENDIAN))
				self.__allocacache = (offset, aligned)

			self.__freescache = None
			return aligned
	def frees(self, size: int):
		if size > 0:
			align = 8 if self.emit64 else 4
			aligned = (size + (align - 1)) & ~(align - 1)

			if self.__freescache:
				offset, current = self.__freescache
				new_size = current + aligned
				self.output[offset] = new_size.to_bytes(1, ENDIAN)[0]
				self.__freescache = (offset, new_size)
			else:
				self.emitOp(b"\x83\xc4")
				offset = len(self.output)
				self.emit(aligned.to_bytes(1, ENDIAN))
				self.__freescache = (offset, aligned)

			self.__allocacache = None
			return aligned
