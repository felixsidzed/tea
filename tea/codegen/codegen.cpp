#include "codegen.h"

#include "tea.h"

#include <llvm-c/Target.h>
#include <llvm-c/BitWriter.h>
#include <llvm-c/TargetMachine.h>
#include <llvm-c/Transforms/PassBuilder.h>

namespace tea {
	LLVMTypeRef type2llvm[TYPE__COUNT] = {
		nullptr,
		nullptr,
		nullptr,
		nullptr,
		nullptr,
		nullptr,
		nullptr,
	};

	void CodeGen::emit(const Tree& tree, const char* output) {
		LLVMInitializeNativeTarget();
		LLVMInitializeNativeAsmPrinter();

		// hard coded :C
		const char* triple = is64Bit ? "x86_64-pc-windows-msvc" : "i386-pc-windows-msvc";

		LLVMTargetRef target;
		char* errMsg = nullptr;
		if (LLVMGetTargetFromTriple(triple, &target, &errMsg) != 0) {
			lastError.reset(errMsg);
			TEA_PANIC("Failed to get target from triple", lastError.get());
			return;
		}

		LLVMTargetMachineRef machine = LLVMCreateTargetMachine(
			target, triple,
			"", "",
			LLVMCodeGenLevelDefault, LLVMRelocDefault, LLVMCodeModelDefault
		);

		if (!machine) {
			TEA_PANIC("Failed to create target machine");
			return;
		}

		type2llvm[TYPE_INT] = LLVMInt32Type();
		type2llvm[TYPE_FLOAT] = LLVMFloatType();
		type2llvm[TYPE_DOUBLE] = LLVMDoubleType();
		type2llvm[TYPE_CHAR] = LLVMInt8Type();
		type2llvm[TYPE_STRING] = LLVMPointerType(LLVMInt8Type(), 0);
		type2llvm[TYPE_VOID] = LLVMVoidType();
		type2llvm[TYPE_BOOL] = LLVMInt1Type();

		module = LLVMModuleCreateWithName("[module]");
		LLVMSetTarget(module, LLVMGetTargetMachineTriple(machine));
		
		char* dl = LLVMCopyStringRepOfTargetData(LLVMCreateTargetDataLayout(machine));
		LLVMSetDataLayout(module, dl);
		LLVMDisposeMessage(dl);

		emitCode(tree);

		if (verbose) {
			putchar('\n');
			char* moduleStr = LLVMPrintModuleToString(module);
			std::cout << moduleStr << std::endl;
			LLVMDisposeMessage(moduleStr);
		}

		char* err = nullptr;
		if (LLVMTargetMachineEmitToFile(machine, module, output, LLVMObjectFile, &err) != 0) {
			std::string _(err);
			LLVMDisposeMessage(err);
			TEA_PANIC("Failed to emit to file: %s", _.c_str());
		}

		LLVMDisposeModule(module);
		LLVMDisposeTargetMachine(machine);
	}

	void CodeGen::emitCode(const Tree& tree) {
		for (const auto& node : tree) {
			switch (node->type) {
			case tnode(FunctionNode):
				emitFunction((FunctionNode*)node.get());
				break;
			default:
				TEA_PANIC("invalid root statement. line %d, column %d", node->line, node->column);
			}
		}
	}
}
