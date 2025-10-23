#include "../codegen.h"

#include "tea/tea.h"

namespace tea {
	void CodeGen::emitVariable(VariableNode* node) {
		std::pair<Type, LLVMValueRef> thingy = {nullptr, nullptr};
		LLVMTypeRef expected = node->dataType.llvm;

		LLVMValueRef insn = nullptr;
		if (fnPrealloc) {
			auto it = fnPrealloc->find(node);
			if (it)
				insn = *it;
		}

		// TODO: find a way to keep the order:
		//									   %.1 = alloca
		//									   ...result of emitExpression(node->value)
		//									   store %.1
		if (!expected) {
			thingy = emitExpression(node->value);
			node->dataType = thingy.first;
			expected = thingy.first.llvm;
		}

		if (!insn)
			insn = LLVMBuildAlloca(block, expected, node->name);

		log("Emitting local '{}' of type '{}' (initialized = {})", node->name.data, type2readable(node->dataType), node->value != nullptr);

		if (node->value != nullptr) {
			if (!thingy.second)
				thingy = emitExpression(node->value);
			if (thingy.first != expected)
				TEA_PANIC("variable initializer type (%s) is incompatible with variable type (%s). line %d, column %d",
					type2readable(expected).data, type2readable(thingy.first).data, node->line, node->column);
			LLVMBuildStore(block, thingy.second, insn);
		}

		locals.push({
			.type = node->dataType,
			.name = node->name,
			.allocated = insn,
		});
	}
}
