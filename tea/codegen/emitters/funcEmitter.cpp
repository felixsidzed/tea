#include "../codegen.h"

#include "tea.h"

namespace tea {
	void CodeGen::emitFunction(FunctionNode* node) {
		log("Entering function '{} func {}(...) ({} arguments)",
			node->storage == STORAGE_PUBLIC ? "public" : "private",
			// TODO: calling convention
			node->name,
			node->args.size()
		);

		std::vector<LLVMTypeRef> argTypes;
		for (const auto& arg : node->args)
			argTypes.push_back(type2llvm[arg.first]);

		curArgs = &node->args;

		LLVMTypeRef funcType = LLVMFunctionType(type2llvm[node->returnType], argTypes.data(), (uint32_t)node->args.size(), 0);
		func = LLVMAddFunction(module, node->name.c_str(), funcType);
		if (node->storage == STORAGE_PRIVATE)
			LLVMSetLinkage(func, LLVMPrivateLinkage);

		int i = 0;
		for (const auto& arg : node->args)
			LLVMSetValueName(LLVMGetParam(func, i++), arg.second.c_str());

		emitBlock(node->body, "entry", func);
	}
}
