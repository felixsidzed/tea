import re
import sys
import traceback

from llvmlite import ir, binding as llvm

from parser import Parser
from codegen.util import *
from parser.nodes import *
from codegen.emitters import block, call, import_, assignment, expression, object_, variable, function


class CodeGen:
	def __init__(self, verbose: bool = False, is64Bit: bool = True, panic = defaultPanic, objectAllocator: str = "_mem__alloc", objectDeallocator: str = "_mem__free") -> None:
		self.verbose = verbose
		self.is64Bit = is64Bit
		self.panic = panic
		self.objectAllocator = objectAllocator
		self.objectDeallocator = objectDeallocator

		opt = llvm.create_module_pass_manager()
		opt.add_gvn_pass()
		#opt.add_loop_unroll_pass() # this sucks
		opt.add_instruction_combining_pass()
		opt.add_dead_code_elimination_pass()
		opt.add_reassociate_expressions_pass()
		opt.add_global_dce_pass()
		self._optimizer = opt

	def _log(self, fmt: str, *args):
		if self.verbose:
			print(" -", fmt % args)

	def generate(self, root: ModuleNode, optimize: bool = True) -> bytes:
		llvm.initialize()
		llvm.initialize_native_target()
		llvm.initialize_native_asmprinter()

		triple = llvm.get_default_triple()
		if not self.is64Bit:
			split = triple.split("-")
			split[0] = "i386"
			triple = "-".join(split)
			del split
		target = llvm.Target.from_triple(triple)
		self._machine = target.create_target_machine(codemodel="default")
		module = ir.Module(root.name)
		self._module = module
		module.triple = self._machine.triple
		module.data_layout = self._machine.target_data
		self._log("Target machine: '%s'", str(target))

		try:
			fltused = ir.GlobalVariable(module, I32, name="_fltused")
			fltused.initializer = ir.Constant(I32, 1)
			fltused.global_constant = False

			null = ir.GlobalVariable(module, PI8, name="null")
			null.initializer = ir.Constant(PI8, None)
			null.global_constant = True

			try:
				self._allocator = ir.Function(self._module, ir.FunctionType(PI8, [I32]), self.objectAllocator)
			except Exception as e:
				self._log("Failed to import allocator: %s", e)

			try:
				self._deallocator = ir.Function(self._module, ir.FunctionType(VOID, [PI8]), self.objectDeallocator)
			except Exception as e:
				self._log("Failed to import deallocator: %s", e)

			self._ctors = {}
			self._objects = {}

			self._emitCode(root)
			del self._ctors, self._objects, self._allocator, self._deallocator

			ir_ = str(module)
			if self.verbose: print(ir_)

			split = ir_.split("\n", 1)
			ref = llvm.parse_assembly(f"{split[0]}\nsource_filename = \"{module.name}\"\n{split[1]}")
			ref.name = module.name
			ref.verify()

			if optimize:
				self._optimizer.run(ref)
			
			obj = self._machine.emit_object(ref)
			del self._module, self._machine
			return obj
		except Exception as e:
			if self.verbose:
				_, _, tb = sys.exc_info()
				fr = traceback.extract_tb(tb)[-1]
				self.panic("code generation failed due to a fatal error (%s : %d): %s: %s", fr.filename, fr.lineno, type(e).__name__, e)
			else:
				self.panic("code generation failed due to a fatal error")
			del self._module, self._machine
			return b""
	
	def _emitCode(self, root: ModuleNode) -> None:
		self._modules = {}

		for node in root.body:
			if type(node) == FunctionNode:
				self._emitFunction(node)

			elif type(node) == FunctionImportNode or type(node) == ObjectImportNode:
				imported = self._emitImport(node)
				if type(imported) == ir.FunctionType:
					ir.Function(self._module, imported, node.name)

			elif type(node) == UsingNode:
				try:
					with open(node.path) as f:
						parser = Parser()
						imported = parser.parse(f.read(), node.name)
						del parser
						f.close()
				except Exception as e:
					self.panic(f"failed to import module '{node.path}': {e}")
					continue

				included = {}
				for _node in imported.body:
					if type(_node) == FunctionImportNode:
						origName = _node.name
						_node.name = f"_{node.name}__{_node.name}"
						included[origName] = self._emitImport(_node)
						del origName
					elif type(_node) == ObjectImportNode:
						self._emitImport(_node)
					else:
						self.panic("module '%s' is not a valid module: '%s' is not a valid top-level entity in this context", node.name, type(_node).__name__[:-4])
						continue

				self._modules[node.name] = included
			
			elif type(node) == GlobalVariableNode:
				expected, const = node.dataType
				type_, value = self._emitExpression(node.value, True)
				if expected == PI8 and type(type_) == ir.ArrayType:
					self.panic("not now,,,")
					continue

				if type_ != expected:
					self.panic("value type '%s' is incompatible with variable type '%s'. line %d, column %d", str(type_), str(expected), node.line, node.column)
					continue
				var = ir.GlobalVariable(self._module, type_, node.name)
				var.initializer = value
				if node.storage == STORAGE_PRIVATE: var.linkage = "private"
				var.global_constant = const

			elif type(node) == ObjectNode:
				self._emitObject(node)

			else:
				self.panic("invalid root statement '%s'", type(node).__name__[:-4])

		del self._modules

# due to pylance becoming laggy because of the amount of code (893 lines at the time of writing)
# we have to split the codegen into multiple files
CodeGen._emitCall = call.emit
CodeGen._emitBlock = block.emit
CodeGen._emitImport = import_.emit
CodeGen._emitAssignment = assignment.emit
CodeGen._emitExpression = expression.emit
CodeGen._emitObject = object_.emit
CodeGen._emitVariable = variable.emit
CodeGen._emitFunction = function.emit
