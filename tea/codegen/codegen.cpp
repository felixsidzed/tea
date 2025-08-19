#include "codegen.h"

#include <fstream>

#include "tea.h"
#include "tea/lexer/lexer.h"
#include "tea/parser/parser.h"

#include <llvm-c/Target.h>
#include <llvm-c/BitWriter.h>
#include <llvm-c/BitReader.h>
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

	LLVMAttributeKind attr2llvm[ATTR__COUNT] = {
		LLVMAttrAlwaysInline,
		LLVMAttrNoReturn,
	};

	std::unordered_map<LLVMTypeKind, const char*> typeKindName = {
		{LLVMIntegerTypeKind, "int"},
		{LLVMFloatTypeKind, "float"},
		{LLVMDoubleTypeKind, "double"},
		{LLVMVoidTypeKind, "void"},
		{LLVMIntegerTypeKind, "long"},
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
		type2llvm[TYPE_STRING] = LLVMPointerType(type2llvm[TYPE_CHAR], 0);
		type2llvm[TYPE_VOID] = LLVMVoidType();
		type2llvm[TYPE_BOOL] = LLVMInt1Type();
		if (is64Bit)
			type2llvm[TYPE_LONG] = LLVMInt64Type();
		else
			type2llvm[TYPE_LONG] = LLVMInt32Type();

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
		if (LLVMTargetMachineEmitToFile(machine, module, output, LLVMObjectFile, &err)) {
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
			case tnode(UsingNode): {
				UsingNode* usingNode = (UsingNode*)node.get();
				std::ifstream file(importLookup / (usingNode->name + ".tea"));
				if (!file.is_open())
					TEA_PANIC(("failed to import module '" + usingNode->name + "'").c_str());

				try {
					std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());

					Parser parser;
					Tree tree = parser.parse(Lexer::tokenize(content));

					ImportedModule importedModule;
					for (const auto& node_ : tree) {
						if (node_->type == tnode(FunctionImportNode))
							emitFunctionImport((FunctionImportNode*)node_.get());
						else
							TEA_PANIC("invalid root statement. line %d, column %d", node_->line, node_->column);
					}
					modules[usingNode->name] = importedModule;
					log("Imported {} function(s) from module '{}'", importedModule.size(), usingNode->name);

				} catch (const std::exception& e) {
					TEA_PANIC(("failed to import module '" + usingNode->name + "': " + e.what()).c_str());
				}

				file.close();
			} break;
			case tnode(FunctionImportNode):
				emitFunctionImport((FunctionImportNode*)node.get());
				break;
			case tnode(GlobalVariableNode): {
				GlobalVariableNode* globalVarNode = (GlobalVariableNode*)node.get();

				LLVMTypeRef expected = type2llvm[globalVarNode->dataType];
				LLVMValueRef global = LLVMAddGlobal(module, expected, globalVarNode->name.c_str());

				if (globalVarNode->storage == STORAGE_PRIVATE)
					LLVMSetLinkage(global, LLVMLinkerPrivateLinkage);

				if (globalVarNode->value != nullptr) {
					auto [got, value] = emitExpression(globalVarNode->value, true);
					if (got != expected)
						TEA_PANIC("variable value type (%s) is incompatible with variable type (%s). line %d, column %d",
							llvm2readable(expected), llvm2readable(got), globalVarNode->line, globalVarNode->column);

					LLVMSetInitializer(global, value);
				} else
					LLVMSetInitializer(global, LLVMConstNull(expected));
			} break;
			default:
				TEA_PANIC("invalid root statement. line %d, column %d", node->line, node->column);
			}
		}
	}

	const char* CodeGen::llvm2readable(LLVMTypeRef type) {
		std::string result;
		LLVMTypeRef t = type;

		int nptr = 0;
		while (LLVMGetTypeKind(t) == LLVMPointerTypeKind) {
			t = LLVMGetElementType(t);
			nptr++;
		}

		LLVMTypeKind kind = LLVMGetTypeKind(t);
		switch (kind) {
		case LLVMIntegerTypeKind: {
			uint32_t bits = LLVMGetIntTypeWidth(t);
			if (bits == 1) {
				result = "bool";
				break;
			} else if (bits == 8) {
				result = "char";
				break;
			} else if (bits >= 32) {
				result = "int";
				break;
			}
			TEA_FALLTHROUGH;
		}

		case LLVMMetadataTypeKind: {
			result = "pointer";
			nptr--;
		} break;

		default:
			auto it = typeKindName.find(kind);
			if (it != typeKindName.end())
				result = it->second;
			else {
				char* llvmName = LLVMPrintTypeToString(t);
				result = llvmName;
				LLVMDisposeMessage(llvmName);
			}
			break;
		}

		for (int i = 0; i < nptr; i++)
			result += '*';

		char* cstr = new char[result.size() + 1];
		std::copy(result.begin(), result.end(), cstr);
		cstr[result.size()] = '\0';
		return cstr;
	}
}
