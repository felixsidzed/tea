from llvmlite import ir

def type_(T):
	if isinstance(T, ir.PointerType):
		return "PE" + type_(T.pointee)
	if isinstance(T, ir.VoidType):
		return "X"
	if isinstance(T, ir.IntType):
		bits = T.width
		if bits == 1:
			return "_N"
		elif bits == 8:
			return "C"
		elif bits == 16:
			return "F"
		elif bits == 32:
			return "H"
		elif bits == 64:
			return "_J"
	if isinstance(T, ir.FloatType):
		return "D"
	if isinstance(T, ir.DoubleType):
		return "N"
	return "?"

def args(function_type):
	if not function_type.args:
		return "@Z"
	arg_codes = "".join(type_(arg) for arg in function_type.args)
	return arg_codes + "@Z"

def mangle(sym, cl, sig=None, method=None):
	if sym == "ctor":
		return f"??0{cl}@@QEAA@{args(sig)}"
	elif sym == "dtor":
		return f"??1{cl}@@QEAA@{args(sig)}"
	elif sym == "vtable":
		return f"??_7{cl}@@6B@"
	elif sym == "method":
		return f"?{method}@{cl}@@QEAA{type_(sig.return_type)}{args(sig)}"
	
__all__ = ["mangle"]
