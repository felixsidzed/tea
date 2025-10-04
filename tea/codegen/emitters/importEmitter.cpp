#include "../codegen.h"

#include <fstream>
#include <unordered_map>

#include "tea.h"
#include "lexer/lexer.h"
#include "parser/parser.h"

namespace tea {
	extern LLVMCallConv cc2llvm[CC__COUNT];

	void CodeGen::emitInclude(UsingNode* node) {
		std::ifstream file(importLookup / (node->name + ".tea"));
		if (!file.is_open()) {
			TEA_PANIC(("failed to import module '" + node->name + "'").c_str());
			return;
		}

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
			modules[node->name] = importedModule;
			log("Imported {} function(s) from module '{}'", importedModule.size(), node->name);

		} catch (const std::exception& e) {
			TEA_PANIC(("failed to import module '" + node->name + "': " + e.what()).c_str());
			return;
		}

		file.close();
	}

	void CodeGen::emitFunctionImport(FunctionImportNode* node) {
		std::vector<LLVMTypeRef> argTypes;
		for (const auto& arg : node->args)
			argTypes.push_back(type2llvm[arg.first]);

		LLVMValueRef imported = LLVMAddFunction(
			module,
			node->name.c_str(),
			LLVMFunctionType(type2llvm[node->returnType], argTypes.data(), (uint32_t)node->args.size(), 0)
		);
		LLVMSetFunctionCallConv(imported, cc2llvm[node->cc]);
	}
}
