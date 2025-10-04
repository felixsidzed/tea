#include "util.h"

#include "../parser/types.h"

namespace tea {
	bool util::checki(LLVMTypeRef i1, LLVMTypeRef i2) {
		return LLVMGetIntTypeWidth(i1) <= LLVMGetIntTypeWidth(i2);
	}

	std::pair<bool, LLVMValueRef> util::cast(LLVMBuilderRef block, const Type& expected, const Type& got, LLVMValueRef val) {
		if (got == expected)
			return { true, val };
			
		LLVMTypeKind gotKind = LLVMGetTypeKind(got.llvm);
		LLVMTypeKind expectedKind = LLVMGetTypeKind(expected.llvm);

		if (expectedKind == LLVMPointerTypeKind && gotKind == LLVMPointerTypeKind)
			return { true, LLVMBuildBitCast(block, val, expected.llvm, "") };

		if (expected.llvm == LLVMInt1Type()) {
			if (gotKind == LLVMPointerTypeKind)
				return { true, LLVMBuildICmp(block, LLVMIntEQ, val, LLVMConstNull(got.llvm), "") };
			else
				return { true, LLVMBuildICmp(block, LLVMIntEQ, val, LLVMConstInt(got.llvm, 0, true), "") };
		} else if (expectedKind == LLVMIntegerTypeKind && gotKind == LLVMIntegerTypeKind) {
			if (checki(expected.llvm, got.llvm))
				return { true, LLVMBuildTrunc(block, val, expected.llvm, "") };
			else if (checki(got.llvm, expected.llvm))
				return { true, LLVMBuildZExt(block, val, expected.llvm, "") };
		}

		if (expectedKind == LLVMPointerTypeKind && gotKind == LLVMIntegerTypeKind && LLVMGetIntTypeWidth(got.llvm) >= 32)
			return { true, LLVMBuildIntToPtr(block, val, expected.llvm, "") };

		if (expectedKind == LLVMIntegerTypeKind && gotKind == LLVMPointerTypeKind)
			return { true, LLVMBuildPtrToInt(block, val, expected.llvm, "") };

		if (expectedKind == LLVMIntegerTypeKind && (gotKind == LLVMFloatTypeKind || gotKind == LLVMDoubleTypeKind))
			return { true, LLVMBuildFPToSI(block, val, expected.llvm, "") };
		if ((expectedKind == LLVMFloatTypeKind || expectedKind == LLVMDoubleTypeKind) && gotKind == LLVMIntegerTypeKind)
			return { true, LLVMBuildSIToFP(block, val, expected.llvm, "") };

		return { false, val };
	}
}
