#include "../codegen.h"

#include <fstream>
#include <unordered_map>

#include "tea.h"
#include "lexer/lexer.h"
#include "parser/parser.h"

namespace tea {
	extern LLVMCallConv cc2llvm[CC__COUNT];

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
