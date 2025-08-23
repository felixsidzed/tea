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
			argTypes.push_back(arg.first.llvm);

		LLVMValueRef imported = LLVMAddFunction(
			module,
			node->name.c_str(),
			LLVMFunctionType(node->returnType.llvm, argTypes.data(), (uint32_t)node->args.size(), 0)
	);
		LLVMSetFunctionCallConv(imported, cc2llvm[node->cc]);
	}
}
