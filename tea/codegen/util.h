#pragma once

#include <memory>

#include "llvm-c/Core.h"

namespace tea {
	class Type;

	namespace util {
		bool checki(LLVMTypeRef i1, LLVMTypeRef i2);
		std::pair<bool, LLVMValueRef> cast(LLVMBuilderRef block, const Type& castTo, const Type& type, LLVMValueRef val);
	}
}
