#include "../codegen.h"

#include "tea.h"

namespace tea {
	extern LLVMCallConv cc2llvm[CC__COUNT];

	LLVMValueRef CodeGen::emitFunctionImport(FunctionImportNode* node) {
		std::vector<LLVMTypeRef> argTypes;
		for (const auto& arg : node->args)
			argTypes.push_back(arg.first.llvm);

		LLVMValueRef imported = LLVMAddFunction(
			module,
			node->name.c_str(),
			LLVMFunctionType(node->returnType.llvm, argTypes.data(), (uint32_t)node->args.size(), node->vararg)
		);
		LLVMSetFunctionCallConv(imported, cc2llvm[node->cc]);
		return imported;
	}
}
