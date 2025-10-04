#include "../codegen.h"

#include "tea.h"

namespace tea {
	void CodeGen::emitVariable(VariableNode* node) {
		log("Emitting local '{}' of type {} (initialized = {})", node->name, llvm2readable(node->dataType.llvm), node->value != nullptr);
		LLVMTypeRef expected = node->dataType.llvm;
		auto insn = LLVMBuildAlloca(block, node->dataType.llvm, node->name.c_str());
		if (node->value != nullptr) {
			auto [got, value] = emitExpression(node->value);
			if (got != expected) {
				TEA_PANIC("variable value type (%s) is incompatible with variable type (%s). line %d, column %d",
					llvm2readable(expected), llvm2readable(got.llvm), node->line, node->column);
			}
			LLVMBuildStore(block, value, insn);
		}
		locals.push_back({
			.type = node->dataType,
			.name = node->name,
			.allocated = insn,
		});
	}
}
