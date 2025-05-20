from codegen.util import CCONV
from parser.ast import FunctionImportNode, ir

def emit(self, node: FunctionImportNode):
	if type(node) == FunctionImportNode:
		f = self._module.globals.get(node.name)
		if not f:
			f = ir.FunctionType(node.returnType[0], (T for T, _ in node.args.values()), node.vararg)
		return f
	else:
		return self.panic("'%s' is not a valid import", type(node).__name__[:-4])

__all__ = ["emit"]
