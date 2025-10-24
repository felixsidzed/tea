#include "../codegen.h"

#include "tea/tea.h"

namespace tea {
	static const char* ccname[CC__COUNT] = {
		"__cdecl",
		"__fastcall",
		"__stdcall",
		"__auto"
	};

	LLVMCallConv cc2llvm[CC__COUNT] = {
		LLVMCCallConv,
		LLVMFastCallConv,
		LLVMX86StdcallCallConv,
	};

	LLVMAttributeKind attr2llvm[ATTR__LLVM_COUNT] = {
		LLVMAlwaysInlineAttributeKind,
		LLVMNoReturnAttributeKind,
	};

	void CodeGen::emitFunction(FunctionNode* node) {
		auto it = std::find(node->attrs.begin(), node->attrs.end(), ATTR_INLINE);
		if (it != node->attrs.end()) {
			log("Marked function '{}' as inlined. Emission skipped", node->name.data);

			inlineables[node->name] = node;
			return;
		}

		log("Entering function '{} {} func {}(...) ({} arguments)",
			node->storage == STORAGE_PUBLIC ? "public" : "private",
			ccname[node->cc],
			node->name.data,
			node->args.size
		);

		vector<LLVMTypeRef> argTypes;
		for (const auto& arg : node->args)
			argTypes.push(arg.first.llvm);

		curArgs = &node->args;
		curFunc = node;

		if (!node->returnType.llvm) {
			fnDeduceRetTy = true;
			node->returnType.llvm = LLVMVoidType();
		}
		LLVMTypeRef funcType = LLVMFunctionType(node->returnType.llvm, argTypes.data, (uint32_t)node->args.size, node->vararg);
		func = LLVMAddFunction(module, node->name, funcType);

		if (node->cc != CC_AUTO)
			LLVMSetFunctionCallConv(func, cc2llvm[node->cc]);

		if (node->storage == STORAGE_PRIVATE)
			LLVMSetLinkage(func, LLVMPrivateLinkage);

		int i = 0;
		for (const auto& arg : node->args)
			LLVMSetValueName(LLVMGetParam(func, i++), arg.second);

		bool noreturn = false;
		for (const auto& attr : node->attrs) {
			if (attr < ATTR__LLVM_COUNT) {
				LLVMAddAttributeAtIndex(func, LLVMAttributeFunctionIndex, LLVMCreateEnumAttribute(LLVMGetGlobalContext(), attr2llvm[attr], 0));
				if (attr == LLVMNoReturnAttributeKind)
					noreturn = true;
			} else if (attr == ATTR_NONAMESPACE)
				hasNoNamespaceFunctions = true;
		}

		if (!node->prealloc.empty()) {
			{
				LLVMBasicBlockRef _ = LLVMAppendBasicBlock(func, "entry");
				block = LLVMCreateBuilder();
				LLVMPositionBuilderAtEnd(block, _);
			}

			for (auto& [paNode, prealloc] : node->prealloc) {
				if (paNode->dataType.llvm)
					prealloc = LLVMBuildAlloca(block, paNode->dataType.llvm, "prealloc." + paNode->name);
			}

			fnPrealloc = &node->prealloc;
			emitBlock(node->body, "entry", nullptr);
			fnPrealloc = nullptr;
		} else
			emitBlock(node->body, "entry", func);

		if (!LLVMGetBasicBlockTerminator(LLVMGetInsertBlock(block))) {
			if (LLVMGetTypeKind(node->returnType.llvm) != LLVMVoidTypeKind)
				TEA_PANIC("control reaches end of non-void function");
			else {
				if (noreturn) LLVMBuildUnreachable(block);
				else LLVMBuildRetVoid(block);
			}
		}

		fnDeduceRetTy = false;
	}
}
