#pragma once

#include <memory>

#include "llvm-c/Core.h"

namespace tea {
	namespace util {
		bool checki(LLVMTypeRef i1, LLVMTypeRef i2);
		std::pair<bool, LLVMValueRef> cast(LLVMBuilderRef block, LLVMTypeRef castTo, LLVMTypeRef type, LLVMValueRef val);
	}
}
