#include "../codegen.h"

#include "tea.h"

namespace tea {
	static const char* ccname[CC__COUNT] = {
		"__cdecl",
		"__fastcall",
		"__stdcall"
	};

	LLVMCallConv cc2llvm[CC__COUNT] = {
		LLVMCCallConv,
		LLVMFastCallConv,
		LLVMX86StdcallCallConv
	};

	void CodeGen::emitFunction(FunctionNode* node) {
		log("Entering function '{} {} func {}(...) ({} arguments)",
			node->storage == STORAGE_PUBLIC ? "public" : "private",
			ccname[node->cc],
			node->name,
			node->args.size()
		);

		std::vector<LLVMTypeRef> argTypes;
		for (const auto& arg : node->args)
			argTypes.push_back(type2llvm[arg.first]);

		curArgs = &node->args;

		LLVMTypeRef funcType = LLVMFunctionType(type2llvm[node->returnType], argTypes.data(), (uint32_t)node->args.size(), 0);
		func = LLVMAddFunction(module, node->name.c_str(), funcType);

		LLVMSetFunctionCallConv(func, cc2llvm[node->cc]);

		if (node->storage == STORAGE_PRIVATE)
			LLVMSetLinkage(func, LLVMPrivateLinkage);

		int i = 0;
		for (const auto& arg : node->args)
			LLVMSetValueName(LLVMGetParam(func, i++), arg.second.c_str());

		for (const auto& attr : node->attrs) {
			if (attr == ATTR_INLINE)
				// fucking hate llvm c api broooo
				// i cant even use alwaysinline because for it to actually be triggered it needs to be ran thru an optimizer
				// and i cant use an optimizer because llvm c api is dogshit and doesnt have proper support for it
				// llvm please reply to my issue on github https://github.com/llvm/llvm-project/issues/153617
				LLVMSetLinkage(func, LLVMLinkOnceAnyLinkage);
			else
				LLVMAddAttributeAtIndex(func, LLVMAttributeFunctionIndex, LLVMCreateEnumAttribute(LLVMGetGlobalContext(), attr2llvm[attr], 0));
		}

		emitBlock(node->body, "entry", func);
	}
}
