#pragma once

#include "mir/mir.h"
#include "llvm-c/Types.h"

namespace tea::backend {

	class MIRLowering {
		LLVMModuleRef M = nullptr;

		tea::map<size_t, LLVMValueRef> globalMap;
		tea::map<const mir::Value*, LLVMValueRef> valueMap;
		tea::map<const mir::BasicBlock*, LLVMBasicBlockRef> blockMap;

	public:
		struct Options {
			bool DumpLLVMModule : 1 = true;
			uint8_t OptimizationLevel : 2 = 0;
		};

		Options options;

		std::pair<std::unique_ptr<uint8_t[]>, size_t> lower(const mir::Module* module, Options options = {});

	private:
		LLVMTypeRef lowerType(const Type* ty);
		void lowerGlobal(const mir::Global* g);
		LLVMValueRef lowerValue(const mir::Value* val);
		LLVMValueRef lowerFunction(const mir::Function* f);
		void lowerBlock(const mir::BasicBlock* block, LLVMBuilderRef builder);
	};

} // namespace tea::backend
