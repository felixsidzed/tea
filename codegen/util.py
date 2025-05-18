import re

from codegen.mangle import mangle
from parser.nodes import TYPE2LLVM, ir, STORAGE2I

I32  = TYPE2LLVM[0]
F32  = TYPE2LLVM[1]
F64  = TYPE2LLVM[2]
I8	 = TYPE2LLVM[3]
PI8	 = TYPE2LLVM[4]
VOID = TYPE2LLVM[5]
I1	 = TYPE2LLVM[6]
I64	 = TYPE2LLVM[7]
I32_0 = ir.Constant(I32, 0)

STORAGE_PRIVATE = STORAGE2I["private"]
STORAGE_PUBLIC = STORAGE2I["public"]

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

def getElementType(t: ir.Type):
	if t.is_pointer:
		return t.pointee
	return None
