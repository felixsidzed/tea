#include "util.h"

namespace tea {
	bool util::checki(LLVMTypeRef i1, LLVMTypeRef i2) {
		return LLVMGetIntTypeWidth(i1) <= LLVMGetIntTypeWidth(i2);
	}

	std::pair<bool, LLVMValueRef> util::cast(LLVMBuilderRef block, LLVMTypeRef expected, LLVMTypeRef got, LLVMValueRef val) {
		if (got == expected)
			return { true, val };
			
		LLVMTypeKind gotKind = LLVMGetTypeKind(got);
		LLVMTypeKind expectedKind = LLVMGetTypeKind(expected);

		if (expectedKind == LLVMPointerTypeKind && gotKind == LLVMPointerTypeKind)
			return { true, LLVMBuildBitCast(block, val, expected, "") };

		if (expected == LLVMInt1Type()) {
			if (gotKind == LLVMPointerTypeKind)
				return { true, LLVMBuildICmp(block, LLVMIntEQ, val, LLVMConstNull(got), "") };
			else
				return { true, LLVMBuildICmp(block, LLVMIntEQ, val, LLVMConstInt(got, 0, true), "") };
		} else if (expectedKind == LLVMIntegerTypeKind && gotKind == LLVMIntegerTypeKind) {
			if (checki(expected, got))
				return { true, LLVMBuildTrunc(block, val, expected, "") };
			else if (checki(got, expected))
				return { true, LLVMBuildZExt(block, val, expected, "") };
		}

		if (expectedKind == LLVMPointerTypeKind && gotKind == LLVMIntegerTypeKind && LLVMGetIntTypeWidth(got) >= 32)
			return { true, LLVMBuildIntToPtr(block, val, expected, "") };

		if (expectedKind == LLVMIntegerTypeKind && gotKind == LLVMPointerTypeKind)
			return { true, LLVMBuildPtrToInt(block, val, expected, "") };

		if (expectedKind == LLVMIntegerTypeKind && (gotKind == LLVMFloatTypeKind || gotKind == LLVMDoubleTypeKind))
			return { true, LLVMBuildFPToSI(block, val, expected, "") };
		if ((expectedKind == LLVMFloatTypeKind || expectedKind == LLVMDoubleTypeKind) && gotKind == LLVMIntegerTypeKind)
			return { true, LLVMBuildSIToFP(block, val, expected, "") };

		return { false, val };
	}
}
