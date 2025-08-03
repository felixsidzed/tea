#include "../codegen.h"

#include "tea.h"

namespace tea {
	void CodeGen::emitFunction(FunctionNode* node) {
		log("Entering function '{} func {}(...) ({} arguments)",
			node->storage == STORAGE_PUBLIC ? "public" : "private",
			// TODO: calling convention
			node->name,
			0 // TODO: arguments
		);
		LLVMTypeRef funcType = LLVMFunctionType(type2llvm[node->returnType], nullptr, 0, 0);
		func = LLVMAddFunction(module, node->name.c_str(), funcType);
		if (node->storage == STORAGE_PRIVATE)
			LLVMSetLinkage(func, LLVMInternalLinkage);

		emitBlock(node->body, "entry", func);
	}
}
