#include "../codegen.h"

#include "tea/tea.h"

namespace tea {
	LLVMValueRef CodeGen::emitFunctionImport(FunctionImportNode* node) {
		vector<LLVMTypeRef> argTypes;
		for (const auto& arg : node->args)
			argTypes.push(arg.first.llvm);

		LLVMValueRef imported = LLVMAddFunction(
			module,
			node->name,
			LLVMFunctionType(node->returnType.llvm, argTypes.data, (uint32_t)node->args.size, node->vararg)
		);
		if (node->cc != CC_AUTO)
			LLVMSetFunctionCallConv(imported, node->cc);
		return imported;
	}
}
