from parser.nodes import *
from codegen.util import CCONV, STORAGE_PRIVATE, cast, I1, I32_0, VOID, I32

def emit(self, root: Node, name: str) -> None:
	self._locals = getattr(self, "_locals", {})
	oldLocals = self._locals.copy()

	i = 0
	for arg, _ in enumerate(self._args):
		arg = self._func.args[arg]
		T = arg.type
		if (T.is_pointer and isinstance(T.pointee, ir.BaseStructType) and (not hasattr(self, "_this") or arg != self._this)):
			prefcount = self._block.gep(arg, [I32_0, I32(1)])
			refcount = self._block.add(self._block.load(prefcount), I32(1))
			self._block.store(refcount, prefcount)
			i += 1
	self._log("%d object(s) retained", i)
	del i

	def releaseAll():
		print("releasing")
		i = 0
		def release(this):
			nonlocal i
			vtable = self._block.load(self._block.gep(this, [I32_0, I32_0]))
			method = self._block.load(self._block.gep(vtable, [I32_0, I32_0]))
			self._block.call(self._block.bitcast(method, ir.FunctionType(VOID, [this.type]).as_pointer()), [this])
			i += 1

		for _, (alloca, (T, _)) in self._locals.items():
			if (T.is_pointer and isinstance(T.pointee, ir.BaseStructType) and (not hasattr(self, "_this") or T != self._this.type)):
				release(self._block.load(alloca))
		for arg, _ in enumerate(self._args):
			arg = self._func.args[arg]
			T = arg.type
			if (T.is_pointer and isinstance(T.pointee, ir.BaseStructType) and (not hasattr(self, "_this") or T != self._this.type)):
				release(arg)
		self._log("%d object(s) released", i)

	for node in root.body:
		if type(node) == ReturnNode:
			self._returned = True
			if self._funcNode.returnType is None:
				if self._funcNode.returnType is None and node.value is not None:
					type_, value = self._emitExpression(node.value)
					self._log("Guessed return type for function '%s': %s", self._func.name, type_)
					self._funcNode.returnType = type_

					del self._module.globals[self._func.name]
					self._module.scope._useset.remove(self._func.name)
					patched = ir.Function(self._func.module, ir.FunctionType(type_, self._func.ftype.args, self._func.ftype.var_arg), self._func.name)
					patched.calling_convention = CCONV[self._funcNode.conv]
					if self._funcNode.storage == STORAGE_PRIVATE: patched.linkage = "private"
					mapping = {}
					for old in self._func.blocks:
						block = ir.Block(patched, old.name)
						block.instructions = old.instructions
						block.terminator = old.terminator
						block.scope = old.scope
						patched.blocks.append(block)
						mapping[old] = block
					self._func = patched

					current = mapping.get(self._block.block)
					self._block = ir.IRBuilder(current)
					self._block.position_at_end(current)
					
					releaseAll()
					self._block.ret(value)
				else:
					releaseAll()
					self._block.ret_void()
			else:
				expected: ir.Type = self._block.block.function.return_value.type

				if node.value is not None:
					type_, value = self._emitExpression(node.value)
					value, success = cast(self._block, expected, value)
					if not success:
						self.panic("return value (%s) is incompatible with function return type (%s). line %d, column %d", str(type_), str(expected), node.line, node.column)
						continue
					releaseAll()
					self._block.ret(value)
				else:
					releaseAll()
					self._block.ret_void()

		elif type(node) == CallNode or type(node) == MethodCallNode:
			self._emitCall(node)

		elif type(node) == VariableNode:
			self._emitVariable(node)

		elif type(node) == IfNode:
			_, firstCond = self._emitExpression(node.condition)
			firstCond, _ = cast(self._block, I1, firstCond)
			
			if len(node.elseIf) == 0 and node.else_ is None:
				with self._block.if_then(firstCond):
					self._emitBlock(node, "then")
			
			else:
				with self._block.if_else(firstCond) as (then, otherwise):
					with then:
						self._emitBlock(node, "then")
					
					if len(node.elseIf) > 0:
						for i, elseIf in enumerate(node.elseIf):
							with otherwise:
								_, elifCond = self._emitExpression(elseIf.condition)
								elifCond, _ = cast(self._block, I1, elifCond)

								with self._block.if_else(elifCond) as (then, otherwise):
									with then:
										self._emitBlock(elseIf, f"elseIf_{i}")

									if node.else_ is not None:
										with otherwise:
											self._emitBlock(node.else_, "else")

					elif node.else_ is not None:
						with otherwise:
							self._emitBlock(node.else_, "else")
		
		elif type(node) == AssignNode:
			self._emitAssignment(node)
		
		elif type(node) == WhileLoopNode:
			condBlock: ir.Block  = self._block.append_basic_block("loop.cond")
			bodyBlock: ir.Block  = self._block.append_basic_block("loop.body")
			mergeBlock: ir.Block = self._block.append_basic_block("loop.merge")

			self._block.branch(condBlock)
			self._block = ir.IRBuilder(condBlock)
			_, cond = self._emitExpression(node.condition)
			cond, _ = cast(self._block, I1, cond)
			self._block.cbranch(cond, bodyBlock, mergeBlock)

			oldBreak = getattr(self, "_break", None)
			oldContinue = getattr(self, "_continue", None)
			self._break = mergeBlock
			self._continue = condBlock

			self._block = ir.IRBuilder(bodyBlock)
			self._emitBlock(node, "loop.body")
			if not self._block.block.is_terminated:
				self._block.branch(condBlock)

			if oldBreak is not None: self._break = oldBreak
			else: del self._break

			if oldContinue is not None: self._continue = oldContinue
			else: del self._continue

			self._block = ir.IRBuilder(mergeBlock)

		elif type(node) == ForLoopNode:
			condBlock: ir.Block  = self._block.append_basic_block("loop.cond")
			bodyBlock: ir.Block  = self._block.append_basic_block("loop.body")
			mergeBlock: ir.Block = self._block.append_basic_block("merge")

			for var in node.variables:
				self._emitVariable(var)

			self._block.branch(condBlock)
			self._block = ir.IRBuilder(condBlock)
			_, cond = self._emitExpression(node.condition)
			cond, _ = cast(self._block, I1, cond)
			self._block.cbranch(cond, bodyBlock, mergeBlock)

			oldBreak = getattr(self, "_break", None)
			oldContinue = getattr(self, "_continue", None)
			self._break = mergeBlock
			self._continue = condBlock

			self._block = ir.IRBuilder(bodyBlock)
			self._emitBlock(node, "loop.body")
			if not self._block.block.is_terminated:
				for step in node.steps:
					self._emitAssignment(step)
					
				self._block.branch(condBlock)
			
			if oldBreak is not None: self._break = oldBreak
			else: del self._break

			if oldContinue is not None: self._continue = oldContinue
			else: del self._continue

			self._block = ir.IRBuilder(mergeBlock)

		elif type(node) == BreakNode:
			self._block.branch(self._break)
			break

		elif type(node) == ContinueNode:
			self._block.branch(self._continue)
			break

		else:
			self.panic("invalid statement '%s' in block", type(node).__name__[:-4])

	self._log("Leaving block '%s:%s' (%d local(s))", getattr(root, "name", "<unnamed>"), name, len(self._locals) - len(oldLocals))
	if not self._block.block.is_terminated: releaseAll()
	self._locals = oldLocals

__all__ = ["emit"]
