#include "codegen.h"

#include <fstream>

#include "tea/tea.h"
#include "tea/map.h"
#include "tea/lexer/lexer.h"
#include "tea/parser/parser.h"

#include <llvm-c/Target.h>
#include <llvm-c/BitWriter.h>
#include <llvm-c/BitReader.h>
#include <llvm-c/TargetMachine.h>
#include <llvm-c/Transforms/PassBuilder.h>

namespace tea {
	map<LLVMTypeKind, const char*> typeKindName = {
		{LLVMIntegerTypeKind, "int"},
		{LLVMFloatTypeKind, "float"},
		{LLVMDoubleTypeKind, "double"},
		{LLVMVoidTypeKind, "void"},
		{LLVMIntegerTypeKind, "long"},
	};

	void CodeGen::emit(const Tree& tree, const char* output) {
		LLVMInitializeNativeTarget();
		LLVMInitializeNativeAsmPrinter();

		std::string triple; {
			char* _ = LLVMGetDefaultTargetTriple();
			triple.assign(_);
			LLVMDisposeMessage(_);
		}
		if (TEA_IS64BIT) {
			if (triple.find("64") == std::string::npos) {
				if (triple.find("i386") != std::string::npos)
					triple.replace(triple.find("i386"), 4, "x86_64");
				else
					triple += "-64";
			}
		} else {
			if (triple.find("64") != std::string::npos) {
				if (triple.find("x86_64") != std::string::npos)
					triple.replace(triple.find("x86_64"), 6, "i386");
				else {
					size_t pos = triple.find("64");
					if (pos != std::string::npos)
						triple.erase(pos, 2);
				}
			}
		}

		LLVMTargetRef target;
		
		char* err = nullptr;
		if (LLVMGetTargetFromTriple(triple.c_str(), &target, &err)) {
			string _(err);
			LLVMDisposeMessage(err);
			TEA_PANIC("Failed to create target machine: %s", _);
		}

		LLVMTargetMachineRef machine = LLVMCreateTargetMachine(
			target, triple.c_str(),
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
			string _(err);
			LLVMDisposeMessage(err);
			TEA_PANIC("Failed to emit to file: %s", _);
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
				fs::path path = importLookup / std::string(usingNode->name + ".tea");
				std::ifstream file(path);
				if (!file.is_open())
					TEA_PANIC("failed to import module '" + usingNode->name + "'");

				try {
					std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());

					Parser parser;
					Tree tree = parser.parse(Lexer::tokenize(content));

					ImportedModule importedModule;
					for (const auto& node_ : tree) {
						if (node_->type == tnode(FunctionImportNode)) {
							FunctionImportNode* fiNode = (FunctionImportNode*)node_.get();
							string unprefixed = fiNode->name;
							// fuckass
							fiNode->name = (std::string(fiNode->name.data, fiNode->name.size).insert(0, "_" + path.stem().string() + "__")).c_str();
							importedModule[unprefixed] = emitFunctionImport(fiNode);
						} else
							TEA_PANIC("invalid root statement. line %d, column %d", node_->line, node_->column);
					}
					modules[usingNode->name] = importedModule;
					log("Imported {} function(s) from module '{}'", importedModule.size(), usingNode->name.data);

				} catch (const std::exception& e) {
					TEA_PANIC("failed to import module '" + usingNode->name + "': " + e.what());
				}

				file.close();
			} break;
			case tnode(FunctionImportNode):
				emitFunctionImport((FunctionImportNode*)node.get());
				break;
			case tnode(GlobalVariableNode): {
				GlobalVariableNode* globalVarNode = (GlobalVariableNode*)node.get();

				Type& expected = globalVarNode->dataType;
				LLVMValueRef global = LLVMAddGlobal(module, expected.llvm, globalVarNode->name);

				if (globalVarNode->storage == STORAGE_PRIVATE)
					LLVMSetLinkage(global, LLVMLinkerPrivateLinkage);

				if (globalVarNode->value != nullptr) {
					auto [got, value] = emitExpression(globalVarNode->value, true);
					if (got != expected)
						TEA_PANIC("variable value type (%s) is incompatible with variable type (%s). line %d, column %d",
							type2readable(expected), type2readable(got), globalVarNode->line, globalVarNode->column);

					LLVMSetInitializer(global, value);
				} else
					LLVMSetInitializer(global, LLVMConstNull(expected.llvm));
			} break;
			default:
				TEA_PANIC("invalid root statement. line %d, column %d", node->line, node->column);
			}
		}
	}

	string CodeGen::type2readable(const Type& type) {
		string result;
		LLVMTypeRef t = type.llvm;

		int nptr = 0;
		while (LLVMGetTypeKind(t) == LLVMPointerTypeKind) {
			t = LLVMGetElementType(t);
			nptr++;
		}

		if (type.constant)
			result += "const";
		if (!type.sign)
			result += "unsigned";

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

		default:
			auto it = typeKindName.find(kind);
			if (it)
				result = *it;
			else {
				char* llvmName = LLVMPrintTypeToString(t);
				result = llvmName;
				LLVMDisposeMessage(llvmName);
			}
			break;
		}

		for (int i = 0; i < nptr; i++)
			result += '*';

		return result;
	}
}
