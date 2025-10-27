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

		if (LLVMIsAAllocaInst(thingy.second)) {
			insn = thingy.second;
			LLVMSetValueName(insn, node->name);
		}  else if (!insn)
			insn = LLVMBuildAlloca(block, expected, node->name);

		if (LLVMGetTypeKind(expected) == LLVMStructTypeKind) {
			if (!node->value) {
				LLVMValueRef ctor = LLVMGetNamedFunction(module, ".ctor`" + string(LLVMGetStructName(expected)));
				if (ctor)
					LLVMBuildCall(block, ctor, &insn, 1, "");
			}
			self = insn;
		}

		log("Emitting local '{}' of type '{}' (initialized = {})", node->name.data, type2readable(node->dataType), node->value != nullptr);
		if (node->value != nullptr) {
			if (!thingy.second) {
				thingy = emitExpression(node->value);

				char* context;
				if (LLVMIsACallInst(thingy.second) && strcmp(strtok_s((char*)LLVMGetCalledValue(thingy.second), "`", &context), ".ctor")) {
					thingy.first = LLVMGetAllocatedType(insn);
					thingy.second = insn;
				}
			}
			if (thingy.first != expected)
				TEA_PANIC("variable initializer type (%s) is incompatible with variable type (%s). line %d, column %d",
					type2readable(expected).data, type2readable(thingy.first).data, node->line, node->column);
		}

		self = nullptr;

		locals.push({
			.type = node->dataType,
			.name = node->name,
			.allocated = insn,
		});
	}
}
