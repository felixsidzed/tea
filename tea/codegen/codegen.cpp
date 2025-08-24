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
		const char* triple = TEA_IS64BIT ? "x86_64-pc-windows-msvc" : "i386-pc-windows-msvc";

		LLVMTargetRef target;

		char* err = nullptr;
		if (LLVMGetTargetFromTriple(triple, &target, &err)) {
			std::string _(err);
			LLVMDisposeMessage(err);
			TEA_PANIC("Failed to create target machine: %s", _.c_str());
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
				fs::path path = importLookup / (usingNode->name + ".tea");
				std::ifstream file(path);
				if (!file.is_open())
					TEA_PANIC(("failed to import module '" + usingNode->name + "'").c_str());

				try {
					std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());

					Parser parser;
					Tree tree = parser.parse(Lexer::tokenize(content));

					ImportedModule importedModule;
					for (const auto& node_ : tree) {
						if (node_->type == tnode(FunctionImportNode)) {
							FunctionImportNode* fiNode = (FunctionImportNode*)node_.get();
							std::string unprefixed = fiNode->name;
							fiNode->name.insert(0, "_" + path.stem().string() + "__");
							importedModule[unprefixed] = emitFunctionImport(fiNode);
						} else
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

				LLVMTypeRef expected = globalVarNode->dataType.llvm;
				LLVMValueRef global = LLVMAddGlobal(module, expected, globalVarNode->name.c_str());

				if (globalVarNode->storage == STORAGE_PRIVATE)
					LLVMSetLinkage(global, LLVMLinkerPrivateLinkage);

				if (globalVarNode->value != nullptr) {
					auto [got, value] = emitExpression(globalVarNode->value, true);
					if (got != expected)
						TEA_PANIC("variable value type (%s) is incompatible with variable type (%s). line %d, column %d",
							llvm2readable(expected), llvm2readable(got.llvm), globalVarNode->line, globalVarNode->column);

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
			} else if (bits == 32) {
				result = "int";
				break;
			} else if (bits >= 64) {
				result = "long";
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
