#include "../codegen.h"

#include "tea.h"

namespace tea {
	void CodeGen::emitVariable(VariableNode* node) {
		log("Emitting local '{}' of type {} (initialized = {})", node->name, (uint8_t)node->dataType, node->value != nullptr);

		LLVMTypeRef expected = type2llvm[node->dataType];

		auto insn = LLVMBuildAlloca(block, type2llvm[node->dataType], node->name.c_str());
		if (node->value != nullptr) {
			auto [got, value] = emitExpression(node->value);
			if (got != expected) {
				std::string expectedStr; {
					char* _ = LLVMPrintTypeToString(expected);
					expectedStr.assign(_);
					LLVMDisposeMessage(_);
				}
				std::string gotStr; {
					char* _ = LLVMPrintTypeToString(got);
					gotStr.assign(_);
					LLVMDisposeMessage(_);
				}
				TEA_PANIC("variable value type (%s) is incompatible with variable type (%s). line %d, column %d", expectedStr.c_str(), gotStr.c_str(), node->line, node->column);
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
