#include "lowering.h"

#include "common/tea.h"

#include "llvm-c/Core.h"
#include "llvm-c/Analysis.h"
#include "llvm-c/TargetMachine.h"

#define lowerBasicBlock(x) blockMap[(x)]

namespace tea::backend {

	static LLVMIntPredicate icmpPred2llvm[] = {
		LLVMIntEQ,
		LLVMIntNE,
		LLVMIntSGT,
		LLVMIntUGT,
		LLVMIntSGE,
		LLVMIntUGE,
		LLVMIntSLT,
		LLVMIntULT,
		LLVMIntSLE,
		LLVMIntULE
	};

	static LLVMRealPredicate fcmpPred2llvm[] = {
		LLVMRealOEQ,
		LLVMRealONE,
		LLVMRealOGT,
		LLVMRealOGE,
		LLVMRealOLT,
		LLVMRealOLE,
		LLVMRealPredicateTrue,
		LLVMRealPredicateFalse
	};

	std::pair<std::unique_ptr<uint8_t[]>, size_t> MIRLowering::lower(const mir::Module* module, Options options) {
		LLVMInitializeAllTargets();
		LLVMInitializeAllTargetMCs();
		LLVMInitializeAllTargetInfos();
		LLVMInitializeAllAsmPrinters();
		LLVMContextSetOpaquePointers(LLVMGetGlobalContext(), false);

		M = LLVMModuleCreateWithName(module->source.data());
		this->options = options;

		char* err = nullptr;

		tea::string triple = module->triple;
		if (module->triple.empty())
			triple = LLVMGetDefaultTargetTriple();

		std::string dl; {
			LLVMSetTarget(M, triple.data());

			if (module->dl.maxNativeBytes == 8)
				dl = "e-m:e-i64:64-f80:128-n8:16:32:64";
			else if (module->dl.maxNativeBytes == 4)
				dl = "e-m:e-p:32:32-i64:32-f80:32-n8:16:32";
			else
				dl = "e";

			if (module->dl.endian == (uint8_t)std::endian::big)
				dl[0] = 'E';

			LLVMSetDataLayout(M, dl.c_str());
		}

		LLVMTargetRef T = nullptr;
		if (LLVMGetTargetFromTriple(triple.data(), &T, &err))
			TEA_PANIC("%s", err);
		LLVMTargetMachineRef TM = LLVMCreateTargetMachine(
			T, triple.data(),
			"", "",
			LLVMCodeGenLevelDefault, LLVMRelocDefault, LLVMCodeModelDefault
		);
		if (!TM)
			TEA_PANIC("Lowering to machine code failed: failed to create target machine");

		for (const auto& node : module->body) {
			switch (node->kind) {
			case mir::ValueKind::Global:
				lowerGlobal((const mir::Global*)node.get());
				break;

			case mir::ValueKind::Function:
				lowerFunction((const mir::Function*)node.get());
				break;

			default:
				TEA_UNREACHABLE();
			}
		}

		if (options.DumpLLVMModule)
			LLVMDumpModule(M);

		if (LLVMVerifyModule(M, LLVMPrintMessageAction, &err))
			TEA_PANIC("%s", err);

		LLVMMemoryBufferRef buf;
		if (LLVMTargetMachineEmitToMemoryBuffer(TM, M, LLVMObjectFile, &err, &buf))
			TEA_PANIC("%s", err);

		LLVMDisposeModule(M);
		LLVMDisposeTargetMachine(TM);

		size_t size = LLVMGetBufferSize(buf);
		auto mc = std::make_unique<uint8_t[]>(size);
		memcpy_s(mc.get(), size, LLVMGetBufferStart(buf), size);
		return { std::move(mc), size };
	}

	LLVMTypeRef MIRLowering::lowerType(const Type* ty) {
		switch (ty->kind) {
		case TypeKind::Void:
			return LLVMVoidType();
		case TypeKind::Bool:
			return LLVMInt1Type();
		case TypeKind::Char:
			return LLVMInt8Type();
		case TypeKind::Short:
			return LLVMInt16Type();
		case TypeKind::Float:
			return LLVMFloatType();
		case TypeKind::Int:
			return LLVMInt32Type();
		case TypeKind::Double:
			return LLVMDoubleType();
		case TypeKind::Long:
			return LLVMInt64Type();
		case TypeKind::String:
			return LLVMPointerType(LLVMInt8Type(), 0);
		case TypeKind::Pointer:
			return LLVMPointerType(lowerType(((PointerType*)ty)->pointee), 0);
		case TypeKind::Function: {
			FunctionType* ftype = (FunctionType*)ty;

			tea::vector<LLVMTypeRef> params;
			for (const auto& ty : ftype->params)
				params.emplace(lowerType(ty));
			return LLVMFunctionType(lowerType(ftype->returnType), params.data, params.size, ftype->vararg);
		}
		case TypeKind::Array: {
			ArrayType* arr = (ArrayType*)ty;
			return LLVMArrayType(lowerType(arr->elementType), arr->size);
		}
		// TODO: struct type lowering
		default:
			return nullptr;
		}
	}

	void MIRLowering::lowerGlobal(const mir::Global* g) {
		LLVMValueRef global = LLVMAddGlobal(M, lowerType(g->type), g->name);
		if (g->storage == mir::StorageClass::Private)
			LLVMSetLinkage(global, LLVMPrivateLinkage);

		if (g->initializer)
			LLVMSetInitializer(global, lowerValue(g->initializer));
		
		globalMap[std::hash<tea::string>()(g->name)] = global;
	}

	void MIRLowering::lowerFunction(const mir::Function* f) {
		LLVMValueRef func = LLVMAddFunction(M, f->name, lowerType(f->type));
		if (f->storage == mir::StorageClass::Private)
			LLVMSetLinkage(func, LLVMPrivateLinkage);

		globalMap[std::hash<tea::string>()(f->name)] = func;
		
		if (f->params.size) {
			for (uint32_t i = 0; i < f->params.size; i++) {
				LLVMValueRef param = LLVMGetParam(func, i);
				valueMap[f->getParam(i)] = param;
				
				if (f->params[i]->name)
					LLVMSetValueName(param, f->getParam(i)->name);
			}
		}

		LLVMBuilderRef builder = LLVMCreateBuilder();

		for (const auto& block : f->blocks)
			lowerBasicBlock(&block) = LLVMAppendBasicBlock(func, block.name);
		
		for (const auto& block : f->blocks) {
			LLVMPositionBuilderAtEnd(builder, lowerBasicBlock(&block));
			lowerBlock(&block, builder);
		}
		
		LLVMDisposeBuilder(builder);
		blockMap.data.clear();
		valueMap.data.clear();
	}

	void MIRLowering::lowerBlock(const mir::BasicBlock* block, LLVMBuilderRef builder) {
		for (const auto& insn : block->body) {
			LLVMValueRef result = nullptr;
			
			switch (insn.op) {
			case mir::OpCode::Add: {
				LLVMValueRef lhs = lowerValue(insn.operands[0]);
				LLVMValueRef rhs = lowerValue(insn.operands[1]);
				if (insn.result.type->isFloat())
					result = LLVMBuildFAdd(builder, lhs, rhs, "");
				else
					result = LLVMBuildAdd(builder, lhs, rhs, "");
			} break;

			case mir::OpCode::Sub: {
				LLVMValueRef lhs = lowerValue(insn.operands[0]);
				LLVMValueRef rhs = lowerValue(insn.operands[1]);
				if (insn.result.type->isFloat())
					result = LLVMBuildFSub(builder, lhs, rhs, "");
				else
					result = LLVMBuildSub(builder, lhs, rhs, "");
			} break;

			case mir::OpCode::Mul: {
				LLVMValueRef lhs = lowerValue(insn.operands[0]);
				LLVMValueRef rhs = lowerValue(insn.operands[1]);
				if (insn.result.type->isFloat())
					result = LLVMBuildFMul(builder, lhs, rhs, "");
				else
					result = LLVMBuildMul(builder, lhs, rhs, "");
			} break;

			case mir::OpCode::Div: {
				LLVMValueRef lhs = lowerValue(insn.operands[0]);
				LLVMValueRef rhs = lowerValue(insn.operands[1]);
				if (insn.result.type->isFloat())
					result = LLVMBuildFDiv(builder, lhs, rhs, "");
				else if (insn.result.type->sign)
					result = LLVMBuildSDiv(builder, lhs, rhs, "");
				else
					result = LLVMBuildUDiv(builder, lhs, rhs, "");
			} break;

			case mir::OpCode::Mod: {
				LLVMValueRef lhs = lowerValue(insn.operands[0]);
				LLVMValueRef rhs = lowerValue(insn.operands[1]);
				if (insn.result.type->isFloat())
					result = LLVMBuildFRem(builder, lhs, rhs, "");
				else if (insn.result.type->sign)
					result = LLVMBuildSRem(builder, lhs, rhs, "");
				else
					result = LLVMBuildURem(builder, lhs, rhs, "");
			} break;

			case mir::OpCode::Not:
				result = LLVMBuildNot(builder, lowerValue(insn.operands[0]), "");
				break;

			case mir::OpCode::And:
				result = LLVMBuildAnd(builder, lowerValue(insn.operands[0]), lowerValue(insn.operands[1]), "");
				break;

			case mir::OpCode::Or:
				result = LLVMBuildOr(builder, lowerValue(insn.operands[0]), lowerValue(insn.operands[1]), "");
				break;

			case mir::OpCode::Xor:
				result = LLVMBuildXor(builder, lowerValue(insn.operands[0]), lowerValue(insn.operands[1]), "");
				break;

			case mir::OpCode::Shl:
				result = LLVMBuildShl(builder, lowerValue(insn.operands[0]), lowerValue(insn.operands[1]), "");
				break;

			case mir::OpCode::Shr:
				if (insn.operands[0]->type->sign)
					result = LLVMBuildAShr(builder, lowerValue(insn.operands[0]), lowerValue(insn.operands[1]), "");
				else
					result = LLVMBuildLShr(builder, lowerValue(insn.operands[0]), lowerValue(insn.operands[1]), "");
				break;

			case mir::OpCode::ICmp:
				result = LLVMBuildICmp(
					builder, icmpPred2llvm[insn.extra],
					lowerValue(insn.operands[0]),
					lowerValue(insn.operands[1]),
					""
				);
				break;

			case mir::OpCode::FCmp:
				result = LLVMBuildFCmp(
					builder, fcmpPred2llvm[insn.extra],
					lowerValue(insn.operands[0]),
					lowerValue(insn.operands[1]),
					""
				);
				break;

			case mir::OpCode::Load: {
				LLVMValueRef ptr = lowerValue(insn.operands[0]);
				result = LLVMBuildLoad(builder, ptr, "");
				if (insn.extra & 1)
					LLVMSetVolatile(result, true);
			} break;

			case mir::OpCode::Store:
				LLVMBuildStore(builder, lowerValue(insn.operands[0]), lowerValue(insn.operands[1]));
				if (insn.extra & 1)
					LLVMSetVolatile(LLVMGetLastInstruction(LLVMGetInsertBlock(builder)), true);
				break;

			case mir::OpCode::Alloca:
				result = LLVMBuildAlloca(builder, lowerType(insn.result.type), "");
				break;

			case mir::OpCode::GetElementPtr: {
				LLVMValueRef ptr = lowerValue(insn.operands[0]);

				tea::vector<LLVMValueRef> indices;
				for (uint32_t i = 1; i < insn.operands.size; i++)
					indices.emplace(lowerValue(insn.operands[i]));

				result = LLVMBuildGEP(builder, ptr, indices.data, indices.size, "");
			} break;

			case mir::OpCode::Br:
				LLVMBuildBr(builder, lowerBasicBlock((const mir::BasicBlock*)insn.operands[0]));
				break;

			case mir::OpCode::CondBr:
				LLVMBuildCondBr(
					builder, lowerValue(insn.operands[0]),
					lowerBasicBlock((const mir::BasicBlock*)insn.operands[1]),
					lowerBasicBlock((const mir::BasicBlock*)insn.operands[2])
				);
				break;

			case mir::OpCode::Ret: {
				if (!insn.operands.size)
					LLVMBuildRetVoid(builder);
				else
					LLVMBuildRet(builder, lowerValue(insn.operands[0]));
			} break;

			case mir::OpCode::Phi: {
				tea::vector<LLVMValueRef> incomingValues;
				tea::vector<LLVMBasicBlockRef> incomingBlocks;

				for (uint32_t i = 0; i < insn.operands.size;) {
					incomingValues.emplace(lowerValue(insn.operands[++i]));
					incomingBlocks.emplace(lowerBasicBlock((const mir::BasicBlock*)insn.operands[++i]));
				}

				result = LLVMBuildPhi(builder, lowerType((const Type*)insn.operands[0]), "x");
				LLVMAddIncoming(
					result,
					incomingValues.data, incomingBlocks.data,
					incomingValues.size
				);
			} break;

			case mir::OpCode::Call: {
				LLVMValueRef callee = lowerValue(insn.operands[0]);
				
				tea::vector<LLVMValueRef> args;
				for (uint32_t i = 1; i < insn.operands.size; i++)
					args.emplace(lowerValue(insn.operands[i]));
				
				FunctionType* ftype = (FunctionType*)insn.operands[0]->type;
				result = LLVMBuildCall(builder, callee, args.data, args.size, "");
			} break;

			case mir::OpCode::Nop:
				break;

			case mir::OpCode::Cast: {
				LLVMValueRef value = lowerValue(insn.operands[0]);
				LLVMTypeRef destType = lowerType(insn.result.type);
				
				const Type* src = insn.operands[0]->type;
				const Type* dst = insn.result.type;
				
				if (src->isFloat() && dst->isNumeric()) {
					if (dst->sign)
						result = LLVMBuildFPToSI(builder, value, destType, "");
					else
						result = LLVMBuildFPToUI(builder, value, destType, "");
				} else if (src->isNumeric() && dst->isFloat()) {
					if (src->sign)
						result = LLVMBuildSIToFP(builder, value, destType, "");
					else
						result = LLVMBuildUIToFP(builder, value, destType, "");
				} else if (src->isNumeric() && dst->isNumeric())
					result = LLVMBuildIntCast(builder, value, destType, "");
				else
					result = LLVMBuildBitCast(builder, value, destType, "");
			} break;

			case mir::OpCode::Unreachable:
				LLVMBuildUnreachable(builder);
				break;

			default:
				TEA_PANIC("cannot lower unknown opcode: %d", insn.op);
			}
			
			if (result && insn.result.kind == mir::ValueKind::Instruction)
				valueMap[&insn.result] = result;
		}
	}

	LLVMValueRef MIRLowering::lowerValue(const mir::Value* val) {
		switch (val->kind) {
		case mir::ValueKind::Global: {
			const mir::Global* g = (const mir::Global*)val;
			if (auto* it = globalMap.find(std::hash<tea::string>()(g->name)))
				return *it;
			return nullptr;
		}

		case mir::ValueKind::Function: {
			const mir::Function* f = (const mir::Function*)val;
			if (auto* it = globalMap.find(std::hash<tea::string>()(f->name)))
				return *it;
			return nullptr;
		}

		case mir::ValueKind::Parameter:
		case mir::ValueKind::Instruction: {
			if (auto* it = valueMap.find(val))
				return *it;
			return nullptr;
		}

		case mir::ValueKind::Constant: {
			switch ((mir::ConstantKind)val->subclassData) {
			case mir::ConstantKind::Number: {
				mir::ConstantNumber* num = (mir::ConstantNumber*)val;
				if (val->type->isNumeric())
					return LLVMConstInt(lowerType(val->type), num->getInteger(), val->type->sign);
				else
					return LLVMConstReal(lowerType(val->type), num->getDouble());
			}

			case mir::ConstantKind::String: {
				mir::ConstantString* str = (mir::ConstantString*)val;
				return LLVMConstString(str->value.data(), (uint32_t)str->value.length(), false);
			} break;

			case mir::ConstantKind::Array: {
				mir::ConstantArray* arr = (mir::ConstantArray*)val;

				tea::vector<LLVMValueRef> values;
				for (const auto& el : arr->values)
					values.emplace(lowerValue(el));
				return LLVMConstArray(lowerType(arr->type), values.data, values.size);
			} break;

			case mir::ConstantKind::Pointer: {
				mir::ConstantPointer* p = (mir::ConstantPointer*)val;
				return LLVMConstIntToPtr(
					LLVMConstInt(LLVMIntPtrType(LLVMGetModuleDataLayout(M)), p->value, false),
					LLVMPointerType(lowerType(p->type), 0)
				);
			}

			default:
				return nullptr;
			}
		} break;

		default:
			return LLVMConstNull(LLVMPointerType(LLVMVoidType(), 0));
		}
	}

} // namespace tea::backend