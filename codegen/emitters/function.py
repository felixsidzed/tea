from parser.ast import FunctionNode, VariableNode, ir
from codegen.util import STORAGE_PRIVATE, CCONV, STORAGE_PUBLIC, VOID

def emit(self, root: FunctionNode):
	self._log("Entering function '%s %s func %s(...)' (%d arguments)",
		"public" if root.storage == STORAGE_PUBLIC else "private",
		root.conv,
		root.name,
		len(root.args)
	)
	self._func = ir.Function(self._module, ir.FunctionType(
		root.returnType[0] if root.returnType is not None else VOID,
		(T for T, _ in root.args.values()),
		root.vararg
	), root.name)
	self._funcNode = root
	self._func.calling_convention = CCONV[root.conv]
	if root.storage == STORAGE_PRIVATE: self._func.linkage = "private"
	self._args = tuple(root.args.keys())
	self._argsTi = tuple(root.args.values())
	self._returned = False
	self._block = ir.IRBuilder(self._func.append_basic_block("entry"))

	# TODO: find a better way to fix possible rsp overflow
	self._preallocated = {}
	def preallocate(root):
		for node in root.body:
			if type(node) == VariableNode and node.dataType is not None:
				self._preallocated[id(node)] = self._block.alloca(node.dataType[0], name=node.name)
			elif hasattr(node, "body"):
				preallocate(node)
	preallocate(root)

	self._emitBlock(root, "entry")
	del self._block, self._locals, self._preallocated
	
	if not self._returned:
		if self._func.ftype.return_type == VOID:
			b = ir.IRBuilder(self._func.blocks[-1])
			b.ret_void()
			del b
		else:
			self.panic("missing return statement in non-void function")

	del self._func, self._funcNode, self._args, self._argsTi, self._returned

__all__ = ["emit"]
