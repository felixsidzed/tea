from codegen.util import mangle, VOID, I32
from parser.ast import FunctionImportNode, ObjectImportNode, ir

def emit(self, node: FunctionImportNode | ObjectImportNode):
	if type(node) == FunctionImportNode:
		f = self._module.globals.get(node.name)
		if not f:
			f = ir.FunctionType(node.returnType[0], (T for T, _ in node.args.values()), node.vararg)
		return f
	elif type(node) == ObjectImportNode:
		struct = ir.global_context.get_identified_type(node.name)
		pstruct = struct.as_pointer()

		vtable_t = ir.global_context.get_identified_type(mangle("vtable", node.name))
		vtable_t.set_body(ir.FunctionType(VOID, [pstruct]).as_pointer(), *[
			ir.FunctionType(method.returnType[0], [pstruct] + [T for T, _ in method.args.values()], method.vararg).as_pointer()
			for method in node.methods if method.name[0] != "."
		])
		pvtable = vtable_t.as_pointer()

		fields = {field.name: (field.storage, field.dataType) for field in node.fields}
		struct.set_body(pvtable, I32, *[field.dataType[0] for field in node.fields])

		self._log("Importing object '%s'. %d method(s), %d field(s)", node.name, len(node.methods), len(node.fields))

		methods = {}

		self._objects[node.name] = (pstruct, fields, methods)

		vtableIdx = 1
		for method in node.methods:
			if method.name == ".ctor":
				self._ctors[node.name] = ir.Function(self._module, ir.FunctionType(pstruct, [T for T, _ in method.args.values()], method.vararg), f"__{node.name}__constructor")
			else:
				sig = ir.FunctionType(method.returnType[0], [pstruct] + [T for T, _ in method.args.values()], method.vararg)
				methods[method.name] = (method.storage, sig, vtableIdx)
				vtableIdx += 1
	else:
		return self.panic("'%s' is not a valid import", type(node).__name__[:-4])

__all__ = ["emit"]
