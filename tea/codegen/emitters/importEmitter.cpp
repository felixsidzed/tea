#include "../codegen.h"

#include "tea/tea.h"

namespace tea {
	extern LLVMAttributeKind attr2llvm[ATTR__LLVM_COUNT];

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

		for (const auto& attr : node->attrs) {
			if (attr < ATTR__LLVM_COUNT)
				LLVMAddAttributeAtIndex(imported, LLVMAttributeFunctionIndex, LLVMCreateEnumAttribute(LLVMGetGlobalContext(), attr2llvm[attr], 0));
			else if (attr == ATTR_NONAMESPACE)
				hasNoNamespaceFunctions = true;
		}

		return imported;
	}
}
