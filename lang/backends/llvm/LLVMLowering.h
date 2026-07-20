#pragma once

#include "mir/mir.h"
#include "backends/Lowering.h"
#include "backends/llvm/llvm-c/Types.h"

namespace tea::backend {

	class LLVMLowering : Lowering {
		LLVMModuleRef M = nullptr;

		tea::map<tea::string, LLVMValueRef> globalMap;
		tea::map<const mir::Value*, LLVMValueRef> valueMap;
		tea::map<const mir::BasicBlock*, LLVMBasicBlockRef> blockMap;

	public:
		LLVMLowering(tea::Context& ctx) : Lowering(ctx) {};

		void lower(const mir::Module* module, Options options) override;

	private:
		LLVMTypeRef lowerType(const Type* ty);
		void lowerGlobal(const mir::Global* g);
		LLVMValueRef lowerValue(const mir::Value* val);
		LLVMValueRef lowerFunction(const mir::Function* f);
		void lowerBlock(const mir::BasicBlock* block, LLVMBuilderRef builder);
	};

} // namespace tea::backend
