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
		auto it = std::find(node->attrs.begin(), node->attrs.end(), ATTR_INLINE);
		if (it != node->attrs.end()) {
			inlineables[node->name] = node;
			return;
		}

		log("Entering function '{} {} func {}(...) ({} arguments)",
			node->storage == STORAGE_PUBLIC ? "public" : "private",
			ccname[node->cc],
			node->name,
			node->args.size()
		);

		std::vector<LLVMTypeRef> argTypes;
		for (const auto& arg : node->args)
			argTypes.push_back(arg.first.llvm);

		curArgs = &node->args;

		LLVMTypeRef funcType = LLVMFunctionType(node->returnType.llvm, argTypes.data(), (uint32_t)node->args.size(), 0);
		func = LLVMAddFunction(module, node->name.c_str(), funcType);

		LLVMSetFunctionCallConv(func, cc2llvm[node->cc]);

		if (node->storage == STORAGE_PRIVATE)
			LLVMSetLinkage(func, LLVMPrivateLinkage);

		int i = 0;
		for (const auto& arg : node->args)
			LLVMSetValueName(LLVMGetParam(func, i++), arg.second.c_str());

		bool noreturn = false;
		for (const auto& attr : node->attrs) {
			LLVMAddAttributeAtIndex(func, LLVMAttributeFunctionIndex, LLVMCreateEnumAttribute(LLVMGetGlobalContext(), attr2llvm[attr], 0));
			if (attr == ATTR_NORETURN)
				noreturn = true;
		}

		emitBlock(node->body, "entry", func);

		if (!LLVMGetBasicBlockTerminator(LLVMGetInsertBlock(block))) {
			if (node->returnType != Type::get(Type::VOID_))
				TEA_PANIC("control reaches end of non-void function");
			else {
				if (noreturn)
					LLVMBuildUnreachable(block);
				else
					LLVMBuildRetVoid(block);
			}
		}
	}
}
