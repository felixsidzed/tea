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

class CodeGen:
	def __init__(self, emit64: bool = True, panic = defaultPanic) -> None:
		self.emit64 = emit64
		self.panic = panic

	def generate(self, root: ModuleNode) -> bytearray:
		self.output = bytearray()
		self.exports = {}
		self.subroutines = {}
		self.reloc = []
		self.stringTable = bytearray(b"\x00\x00\x00\x00")

		symbolIndices = {}

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
		symbolTableOffset = len(self.output)
		for name, value in self.exports.items():
			if len(name) <= 8:
				bname = name.encode().ljust(8, b"\x00")
			else:
				offset = len(self.stringTable)
				self.stringTable += name.encode() + b"\x00"
				bname = struct.pack("@II", 0, offset)

			self.emit(bname)
			self.emit(struct.pack("@LhHBB", value, 1, 0x20, 2, 0))
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
			0,                                # ???
			0x0000                            # characteristics
		)

		self.output[textHeaderOffset:textHeaderOffset + 40] = struct.pack("@8sLLLLLLHHL",
			b".text\x00\x00\x00", # name
			codeSize,             # virtual size
			0x0000,               # virtual addr
			codeSize,             # size of raw data
			codeOffset,           # ptr to raw data
			relocOffset,		  # ptr to relocations
			0,	  				  # ptr to line numbers
			len(self.reloc),      # number of relocations
			0,					  # number of line numbers
			0x60000020            # characteristics
		)

		return self.output
	
	def emitCode(self, root: ModuleNode):
		HEADERSIZE = len(self.output)
		for node in root.body:
			if isinstance(node, FunctionNode):
				self.subroutines[node.name] = len(self.output) - HEADERSIZE
				if node.storage == STORAGE_PUBLIC:
					self.exports[node.name] = len(self.output) - HEADERSIZE
				self.emitFunction(node)
			elif isinstance(node, UsingNode):
				# TODO
				continue
			else:
				return self.panic(f"[ERR] invalid root node '{type(node)}'")

	def emitFunction(self, fn: FunctionNode) -> None:
		self.emit(b"\x55")
		self.emitOp(b"\x89\xe5")

		for node in fn.body:
			if type(node) == ReturnNode:
				if isinstance(node.value, ExpressionNode) or type(node.value) == CallNode:
					self.emitExpr(node.value)
					self.emit(b"\x5d")
					self.emit(b"\xc3")

				elif not isinstance(node.value, Node):
					self.emitOp(b"\xb8")
					self.emitImm(node.value)
					self.emit(b"\x5d")
					self.emit(b"\xc3")
				
				else:
					self.panic(f"[ERR] invalid return value node '{type(node)}'")
				return # maybe this isnt necessary

			elif type(node) == CallNode:
				self.emitCall(node)
	
	def emitExpr(self, node: ExpressionNode, target: str = "rax") -> None:
		if type(node) == AddNode:
			self.emitExpr(node.left, "rax")
			self.emit(b"\x50")
			self.emitExpr(node.right, "rbx")
			self.emit(b"\x5b")
			self.emit(b"\x48\x01\xd8")

		elif type(node) == SubNode:
			self.emitExpr(node.left, "rax")
			self.emit(b"\x50")
			self.emitExpr(node.right, "rbx")
			self.emit(b"\x5b")
			self.emit(b"\x48\x29\xd8")

		elif type(node) == MulNode:
			self.emitExpr(node.left, "rax")
			self.emit(b"\x50")
			self.emitExpr(node.right, "rbx")
			self.emit(b"\x5b")
			self.emit(b"\x48\x0f\xaf\xc3")

		elif type(node) == DivNode:
			self.emitExpr(node.left, "rax")
			self.emit(b"\x50")
			self.emitExpr(node.right, "rbx")
			self.emit(b"\x5b")
			self.emit(b"\x48\x99")
			self.emit(b"\x48\xf7\xfb")

		elif type(node) == CallNode:
			self.emitCall(node)

		elif type(node) == LarkToken:
			if node.type == "INT":
				if target == "rax":
					self.emitOp(b"\xb8")
				elif target == "rbx":
					self.emitOp(b"\xbb")
				else:
					return self.panic(f"[ERR] unsupported register target '{target}'")
				self.emitImm(node.value)

			else:
				return self.panic(f"[ERR] unsupported constant type in expression '{node.type}'")

		else:
			return self.panic(f"[ERR] invalid expression node '{type(node)}'")

	def emit(self, b: bytes) -> None:
		self.output.extend(b)

	def emitOp(self, b: bytes) -> None:
		if self.emit64:
			self.emit(b"\x48")
		self.emit(b)
	
	def emitImm(self, value: int) -> None:
		self.output.extend(value.to_bytes(
			8 if self.emit64 else 4,
			ENDIAN
		))

	def emitCall(self, node: CallNode) -> None:
		for arg in reversed(node.args):
			if isinstance(arg, LarkToken) and arg.type == "INT":
				self.emitOp(b"\x68")
				self.emit(arg.value.to_bytes(4, ENDIAN))
			else:
				return self.panic(f"[ERR] unsupported argument type '{type(arg)}'")

		funcName = "::".join(node.scope + [node.name])

		if funcName in self.subroutines:
			self.emit(b"\xe8")
			self.emit((-(len(self.output) - 60 + 4)).to_bytes(4, ENDIAN, signed=True))

		else:
			self.emit(b"\xe8")
			callee = len(self.output)
			self.emit(b"\x00\x00\x00\x00")

			self.reloc.append({
				"symbol": funcName,
				"offset": callee,
				"type": 4, # rel 32 bit
			})

		if len(node.args) > 0:
			# TODO: better cleanup calculation
			cleanup = len(node.args) * (4 if self.emit64 else 8)
			self.emitOp(b"\x83\xc4")
			self.emit(cleanup.to_bytes(1, ENDIAN))
