import sys
import struct
from datetime import datetime

ENDIAN = sys.byteorder


class COFFSymbol:
	def __init__(self, name: str, value: int, section: int, type_: int, generator, storage=2):
		self.name = name
		self.value = value
		self.section = section
		self.type_ = type_
		self.storage = storage
		self.generator = generator

	def pack(self):
		"""Packs the COFF symbol into bytes"""
		if len(self.name) <= 8:
			nameBytes = self.name.encode().ljust(8, b"\x00")
		else:
			nameBytes = struct.pack("II", 0, self.generator.string(self.name))
		return struct.pack("8sIHHBB",
			nameBytes,
			self.value,
			self.section,
			self.type_,
			self.storage,
			0
		)

	@staticmethod
	def size():
		return 0x12


class COFFSection:
	def __init__(self, name: str, characteristics: int, VA: int = 0x0):
		self.name = name
		self.relocations = []
		self.VirtualAddress = VA
		self.characteristics = characteristics
		self.data = bytearray()

	def emit(self, b: bytes):
		"""Adds the provided to section data and returns the offset where it was added to"""
		offset: int = len(self.data)
		self.data.extend(b)
		return offset

	def reloc(self, offset: int, symbol: int, type_: int):
		"""
			Adds a COFF relocation

			`offset` - The relative offset where to patch the bytes\n
			`symbol` - The index of the symbol to base the relocation on in the [symbol table](https://learn.microsoft.com/en-us/windows/win32/debug/pe-format#coff-symbol-table)\n
			`type_`  - The [relocation type](https://learn.microsoft.com/en-us/windows/win32/debug/pe-format#type-representation)\n
		"""
		self.relocations.append(COFFRelocation(offset, symbol, type_))

	def pack(self):
		"""Packs the section **header** into bytes"""
		nameBytes = self.name[:8].encode().ljust(8, b"\x00")

		return struct.pack("8sIIIIIHHHI",
			nameBytes,
			len(self.data), self.VirtualAddress,
			len(self.data), 0,
			0, 0,
			0, len(self.relocations),
			self.characteristics
		)

	@staticmethod
	def size():
		return 0x28


class COFFRelocation:
	def __init__(self, VirtualAddress: int, SymbolTableIndex: int, Type: int):
		self.VirtualAddress = VirtualAddress
		self.SymbolTableIndex = SymbolTableIndex
		self.Type = Type

	def pack(self):
		"""Packs the relocation into bytes"""
		return struct.pack("IIH",
			self.VirtualAddress,
			self.SymbolTableIndex,
			self.Type
		)
	
	@staticmethod
	def size():
		return 0xa


class COFFHeader:
	def __init__(self):
		self.Machine = 0
		self.TimeDateStamp = int(datetime.now().timestamp())
		self.NumberOfSections = 0
		self.PointerToSymbolTable = 0
		self.NumberOfSymbols = 0
		self.SizeOfOptionalHeader = 0
		self.Characteristics = 0

	def pack(self):
		"""Packs the COFF File Header into bytes"""
		self.TimeDateStamp = int(datetime.now().timestamp())
		return struct.pack("HHIIIHH",
			self.Machine,
			self.NumberOfSections,
			self.TimeDateStamp,
			self.PointerToSymbolTable,
			self.NumberOfSymbols,
			self.SizeOfOptionalHeader,
			self.Characteristics
		)

	@staticmethod
	def size():
		return 0x14


class COFFGenerator:
	def __init__(self, is64bit: bool = True):
		self.is64bit: bool = is64bit
		self.symbols: list[COFFSymbol] = []
		self.sections: list[COFFSection] = []
		self.strings: bytearray = bytearray(b"\x04\x00\x00\x00")

		self.header: COFFHeader = COFFHeader()
		self.header.Machine = 0x8664 if is64bit else 0x014C

	def section(self, name: str, characteristics: int):
		"""
			Appends a section with the specified name and characteristics, then returns it

			`name` - The section name\n
			`characteristics` - The section characteristics\n
		"""
		section: COFFSection = COFFSection(name, characteristics)
		self.sections.append(section)
		self.header.NumberOfSections += 1
		return section

	def symbol(self, name: str, value: int, section: int, type_: int, storage: int = 2):
		"""
			Appends a symbol to the symbol table

			`name`    - [The symbol name](https://learn.microsoft.com/en-us/windows/win32/debug/pe-format#symbol-name-representation)\n
			`value`   - The symbol value, this depends on `type`\n
			`section` - The 1-based index of the section that the symbol references\n
			`type_`   - [The symbol type](https://learn.microsoft.com/en-us/windows/win32/debug/pe-format#type-representation)\n
			`storage` - [The symbol storage class](https://learn.microsoft.com/en-us/windows/win32/debug/pe-format#storage-class). External is default\n
		"""
		sym: COFFSymbol = COFFSymbol(name, value, section, type_, self, storage)
		self.symbols.append(sym)
		self.header.NumberOfSymbols += 1
		return sym

	def string(self, s: str):
		"""
			Appends a string to the string name\n
			Note that PE files do not support string tables\n
			If you need a PE-compatible string, emit it to `.rdata'. See example.py

			`s`  - The string to append
		"""
		offset: int = len(self.strings)
		self.strings += s.encode() + b"\x00"
		self.strings[:4] = struct.pack("I", len(self.strings))
		return offset

	def emit(self):
		"""Emits everything into bytes"""
		result = bytearray()

		result.extend(b"\x00" * COFFHeader.size())

		sectionHeaders = {}
		for section in self.sections:
			sectionHeaders[section.name] = len(result)
			result.extend(section.pack())

		for section in self.sections:
			header = sectionHeaders[section.name]
			result[header + 20:header + 24] = struct.pack("I", len(result))
			result.extend(section.data)

		for section in self.sections:
			if len(section.relocations) > 0:
				header = sectionHeaders[section.name]
				result[header + 24:header + 26] = struct.pack("H", len(result))
				for reloc in section.relocations:
					result.extend(reloc.pack())

		self.header.PointerToSymbolTable = len(result)
		for symbol in self.symbols:
			result.extend(symbol.pack())

		result.extend(self.strings)

		result[:COFFHeader.size()] = self.header.pack()

		return bytes(result)
