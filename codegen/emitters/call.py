from parser.ast import CallNode, MethodCallNode, ir
from codegen.util import cast, I32_0, I32, STORAGE_PRIVATE, STORAGE_PUBLIC, VOID

def virtualCall(self, node: MethodCallNode):
	type_, this = self._emitExpression(node.value)
	for objName, obj in self._objects.items():
		if obj[1].type == type_: break
	else:
		return self.panic("reference to undefined object '%s' in expression. line %d, column %d", node.value, node.line, node.column)
	
	vtable = self._block.load(self._block.gep(this, [I32_0, I32_0]))

	method = obj[3].get(node.name)
	if not method:
		return self.panic("'%s' is not a valid member of '%s'. line %d, column %d", node.name, objName, node.line, node.column)
	storage, sig, idx, _ = method
	if storage == STORAGE_PRIVATE:
		return self.panic("storage type violation. line %d, column %d", node.line, node.column)

	method = self._block.load(self._block.gep(vtable, [I32_0, I32(idx)]))
	args = [list(self._emitExpression(arg)) for arg in node.args]

	for i, expected in enumerate(sig.args):
		if i == 0: continue # this
		i -= 1
		got = args[i][0]
		if got != expected:
			value, success = cast(self._block, expected, args[i][1])
			args[i][1] = value
			if not success:
				self.panic(
					"argument %d: expected type %s, got %s. line %d, column %d",
					i, expected, got, node.line, node.column
				)
				continue
	return (sig.return_type, self._block.call(self._block.bitcast(method, sig.as_pointer()), [this] + args))

def emit(self, node: CallNode | MethodCallNode):
	if type(node) == MethodCallNode:
		return virtualCall(self, node)
	
	callee = node.name
	args = [list(self._emitExpression(arg)) for arg in node.args]

	func = None
	vararg = False

	module = None
	if len(node.scope) == 0:
		if callee not in self._module.globals:
			self.panic("reference to undefined function '%s'. line %d, column %d", callee, node.line, node.column)
			return
		func: ir.Function = self._module.get_global(callee)
		vararg = func.function_type.var_arg
	else:
		module = self._modules
		fqn = ""

		for scope in node.scope:
			module = module.get(scope)
			if module is None:
				self.panic("'%s' does not name a valid scope. line %d, column %d", scope, node.line, node.column)
				return
			fqn += f"_{scope}__"
		fqn += callee

		if fqn in self._module.globals:
			func: ir.Function = self._module.get_global(fqn)
			vararg = func.function_type.var_arg
		else:
			func: ir.Function = module.get(callee)
			if func is None:
				self.panic("reference to undefined function '%s'. line %d, column %d", fqn, node.line, node.column)
				return
			vararg = func.function_type.var_arg

	expectedArgs = func.function_type.args
	if not vararg and len(args) != len(expectedArgs):
		self.panic("argument count mismatch. expected %d, got %d. line %d, column %d", len(expectedArgs), len(args), node.line, node.column)
		return
	elif vararg and len(args) < len(expectedArgs):
		self.panic("argument count mismatch. expected at least %d, got %d. line %d, column %d", len(expectedArgs), len(args), node.line, node.column)
		return

	for i, expected in enumerate(expectedArgs):
		got = args[i][0]
		if got != expected:
			value, success = cast(self._block, expected, args[i][1])
			args[i][1] = value
			if not success:
				self.panic(
					"argument %d: expected type %s, got %s. line %d, column %d",
					i, expected, got, node.line, node.column
				)
				continue

	return (func.function_type.return_type, self._block.call(func, (value for _, value in args)))

__all__ = ["emit", "virtualCall"]
