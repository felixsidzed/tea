#include "../codegen.h"

#include "tea/tea.h"

namespace tea {
	void CodeGen::emitVariable(VariableNode* node) {
		log("Emitting local '{}' of type {} (initialized = {})", node->name.data, type2readable(node->dataType), node->value != nullptr);

		LLVMTypeRef expected = node->dataType.llvm;

		LLVMValueRef insn = nullptr;
		if (fnPrealloc) {
			auto it = fnPrealloc->find(node);
			if (it)
				insn = *it;
		}

		if (!insn)
			insn = LLVMBuildAlloca(block, node->dataType.llvm, node->name);

		if (node->value != nullptr) {
			auto [got, value] = emitExpression(node->value);
			if (got != expected)
				TEA_PANIC("variable value type (%s) is incompatible with variable type (%s). line %d, column %d",
					type2readable(expected), type2readable(got), node->line, node->column);
			LLVMBuildStore(block, value, insn);
		}

		locals.push({
			.type = node->dataType,
			.name = node->name,
			.allocated = insn,
		});
	}
}
