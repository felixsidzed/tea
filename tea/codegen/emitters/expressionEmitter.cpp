#include "../codegen.h"

#include "tea/tea.h"
#include "tea/codegen/util.h"

namespace tea {
	std::pair<Type, LLVMValueRef> CodeGen::emitExpression(const std::unique_ptr<ExpressionNode>& node, bool constant, bool* ptr) {
		if (node->etype >= EXPR_ADD && node->etype <= EXPR_DIV) {
			auto [ltype, lhs] = emitExpression(node->left, constant);
			auto [rtype, rhs] = emitExpression(node->right, constant);
			
			if (constant) {
				if (!LLVMIsConstant(lhs))
					TEA_PANIC("value is not a constant expression. line %d, column %d", node->left->line, node->left->column);
				if (!LLVMIsConstant(rhs))
					TEA_PANIC("value is not a constant expression. line %d, column %d", node->right->line, node->right->column);

				if (LLVMGetTypeKind(ltype.llvm) == LLVMIntegerTypeKind) {
					uint64_t lval = LLVMConstIntGetSExtValue(lhs);
					uint64_t rval = LLVMConstIntGetSExtValue(rhs);
					switch (node->etype) {
					case EXPR_ADD: return { ltype, LLVMConstInt(ltype.llvm, lval + rval, ltype.sign) };
					case EXPR_SUB: return { ltype, LLVMConstInt(ltype.llvm, lval - rval, ltype.sign) };
					case EXPR_MUL: return { ltype, LLVMConstInt(ltype.llvm, lval * rval, ltype.sign) };
					case EXPR_DIV: return { ltype, LLVMConstInt(ltype.llvm, lval / rval, ltype.sign) };
					}
				} else if (ltype == LLVMFloatType() || ltype == LLVMDoubleType()) {
					LLVMBool _;
					double lval = LLVMConstRealGetDouble(lhs, &_);
					double rval = LLVMConstRealGetDouble(rhs, &_);
					switch (node->etype) {
					case EXPR_ADD: return { ltype, LLVMConstReal(ltype.llvm, lval + rval) };
					case EXPR_SUB: return { ltype, LLVMConstReal(ltype.llvm, lval - rval) };
					case EXPR_MUL: return { ltype, LLVMConstReal(ltype.llvm, lval * rval) };
					case EXPR_DIV: return { ltype, LLVMConstReal(ltype.llvm, lval / rval) };
					}
				}
			}

			if (LLVMGetTypeKind(ltype.llvm) != LLVMGetTypeKind(rtype.llvm) && !(LLVMGetTypeKind(ltype.llvm) == LLVMPointerTypeKind && LLVMGetTypeKind(rtype.llvm) == LLVMIntegerTypeKind))
				TEA_PANIC("type mismatch in expression. line %d, column %d", node->line, node->column);

			LLVMValueRef result;
			switch (node->etype) {
			case EXPR_ADD: {
				if (LLVMGetTypeKind(ltype.llvm) == LLVMIntegerTypeKind)
					result = LLVMBuildAdd(block, lhs, rhs, "");
				else if (ltype == Type::get(Type::FLOAT) || ltype == Type::get(Type::DOUBLE))
					result = LLVMBuildFAdd(block, lhs, rhs, "");
				else if (LLVMGetTypeKind(ltype.llvm) == LLVMPointerTypeKind)
					result = LLVMBuildGEP(block, lhs, &rhs, 1, "");
				else {
					TEA_PANIC("unsupported types for addition. line %d, column %d", node->line, node->column);
					TEA_UNREACHABLE();
				}
			} break;

			case EXPR_SUB: {
				if (LLVMGetTypeKind(ltype.llvm) == LLVMIntegerTypeKind)
					result = LLVMBuildSub(block, lhs, rhs, "");
				else if (ltype == Type::get(Type::FLOAT) || ltype == Type::get(Type::DOUBLE))
					result = LLVMBuildFSub(block, lhs, rhs, "");
				else if (LLVMGetTypeKind(ltype.llvm) == LLVMPointerTypeKind)
					result = LLVMBuildGEP(block, lhs, &rhs, 1, "");
				else {
					TEA_PANIC("unsupported types for subtraction. line %d, column %d", node->line, node->column);
					TEA_UNREACHABLE();
				}
			} break;

			case EXPR_MUL: {
				if (LLVMGetTypeKind(ltype.llvm) == LLVMIntegerTypeKind)
					result = LLVMBuildMul(block, lhs, rhs, "");
				else if (ltype == Type::get(Type::FLOAT) || ltype == Type::get(Type::DOUBLE))
					result = LLVMBuildFMul(block, lhs, rhs, "");
				else {
					TEA_PANIC("unsupported types for multiplication. line %d, column %d", node->line, node->column);
					TEA_UNREACHABLE();
				}
			} break;

			case EXPR_DIV: {
				if (LLVMGetTypeKind(ltype.llvm) == LLVMIntegerTypeKind)
					result = LLVMBuildSDiv(block, lhs, rhs, "");
				else if (ltype == Type::get(Type::FLOAT) || ltype == Type::get(Type::DOUBLE))
					result = LLVMBuildFDiv(block, lhs, rhs, "");
				else {
					TEA_PANIC("unsupported types for divison. line %d, column %d", node->line, node->column);
					TEA_UNREACHABLE();
				}
			} break;

			default:
				goto invalid;
			}

			return { ltype, result };
		}

		switch (node->etype) {
		case EXPR_INT: {
			return {
				Type::get(Type::INT),
				LLVMConstInt(Type::get(Type::INT).llvm, std::stoll(std::string(node->value)), true)
			};
		} break;

		case EXPR_FLOAT: {
			return {
				Type::get(Type::FLOAT),
				LLVMConstReal(Type::get(Type::FLOAT).llvm, std::stod(std::string(node->value)))
			};
		} break;

		case EXPR_DOUBLE: {
			return {
				Type::get(Type::DOUBLE),
				LLVMConstReal(Type::get(Type::DOUBLE).llvm, std::stod(std::string(node->value)))
			};
		} break;

		case EXPR_STRING: {
			LLVMValueRef str = LLVMBuildGlobalString(block, node->value, "");
			if (ptr) *ptr = true;
			return {
				Type::get(Type::STRING),
				LLVMBuildBitCast(block, str, Type::get(Type::STRING).llvm, "")
			};
		} break;

		case EXPR_CHAR: {
			return {
				Type::get(Type::CHAR),
				LLVMConstInt(LLVMInt8Type(), node->value[0], true)
			};
		}

		case EXPR_CALL: {
			if (constant)
				TEA_PANIC("value is not a constant expression. line %d, column %d", node->line, node->column);

			CallNode* call = (CallNode*)node.get();

			vector<LLVMValueRef> args;
			vector<LLVMTypeRef> argTypes;
			if (self) {
				args.push(self);
				argTypes.push(LLVMTypeOf(self));
			}
			args.reserve(args.size + call->args.size);
			argTypes.reserve(args.size);

			for (const auto& arg : call->args) {
				auto [t, v] = emitExpression(arg);
				argTypes.push(t.llvm);
				args.push(v);
			}

			// TODO: improve
			LLVMValueRef prevSelf = self;
			auto [calleeType, callee] = emitExpression(call->callee);
			if (!prevSelf && self) {
				vector<LLVMTypeRef> tmp1;
				vector<LLVMValueRef> tmp2;
				tmp2.push(self);
				tmp1.push(LLVMTypeOf(self));
				for (uint32_t i = 0; i < args.size; i++) {
					tmp1.push(argTypes[i]);
					tmp2.push(args[i]);
				}
				args = tmp2;
				argTypes = tmp1;
			}
			if (LLVMGetTypeKind(calleeType.llvm) != LLVMPointerTypeKind || LLVMGetTypeKind(LLVMGetElementType(calleeType.llvm)) != LLVMFunctionTypeKind)
				TEA_PANIC("cannot call a value of type '%s'. line %d, column %d", type2readable(calleeType).data, node->line, node->column);

			LLVMTypeRef ftype = LLVMGetElementType(calleeType.llvm);
			uint32_t nargs = LLVMCountParamTypes(ftype);

			if ((LLVMIsFunctionVarArg(ftype) && nargs > args.size) || (!LLVMIsFunctionVarArg(ftype) && nargs != args.size))
				TEA_PANIC("argument count mismatch. expected atleast %d, got %d. line %d, column %d", nargs, args.size, node->line, node->column);

			LLVMTypeRef* calleeArgTypes = new LLVMTypeRef[nargs + 1];
			LLVMGetParamTypes(ftype, calleeArgTypes);

			for (uint32_t i = 0; i < nargs; i++) {
				LLVMTypeRef got = argTypes[i];
				LLVMTypeRef expected = calleeArgTypes[i];

				if (got != expected) {
					if (LLVMGetTypeKind(got) == LLVMIntegerTypeKind && LLVMGetTypeKind(expected) == LLVMIntegerTypeKind) {
						if (LLVMGetIntTypeWidth(got) > LLVMGetIntTypeWidth(expected))
							args[i] = LLVMBuildZExt(block, args[i], expected, "");
						else
							args[i] = LLVMBuildTrunc(block, args[i], expected, "");
					} else if (LLVMGetTypeKind(got) == LLVMPointerTypeKind && LLVMGetTypeKind(expected) == LLVMPointerTypeKind)
						args[i] = LLVMBuildBitCast(block, args[i], expected, "");
					else
						TEA_PANIC("argument %d: expected type %s, got %s. line %d, column %d",
							i + 1, type2readable(expected).data, type2readable(got).data, node->line, node->column);
				}
			}

			delete[] calleeArgTypes;
			std::pair<Type, LLVMValueRef> ret = { LLVMGetReturnType(ftype), LLVMBuildCall(block, callee, args.data, args.size, "") };

			{
				uint32_t nattrs = LLVMGetAttributeCountAtIndex(callee, LLVMAttributeFunctionIndex);
				LLVMAttributeRef* attrs = new LLVMAttributeRef[nattrs + 1];
				LLVMGetAttributesAtIndex(callee, LLVMAttributeFunctionIndex, attrs);
				for (uint32_t i = 0; i < nattrs; i++) {
					if (LLVMGetEnumAttributeKind(attrs[i]) == LLVMNoReturnAttributeKind)
						LLVMBuildUnreachable(block);
				}
				delete[] attrs;
			}

			return ret;
		}

		case EXPR_IDENTF: {
			if (!strchr(node->value, ':')) {
				if (node->value == "true") {
					return {
						Type::get(Type::BOOL),
						LLVMConstInt(LLVMInt1Type(), true, false)
					};
				} else if (node->value == "false") {
					return {
						Type::get(Type::BOOL),
						LLVMConstInt(LLVMInt1Type(), false, false)
					};
				} else if (node->value == "null") {
					LLVMTypeRef pvoid = LLVMPointerType(LLVMVoidType(), 0);
					return {
						pvoid,
						LLVMConstNull(pvoid)
					};
				}

				if (constant)
					TEA_PANIC("value is not a constant expression. line %d, column %d", node->line, node->column);

				LLVMValueRef global = LLVMGetNamedGlobal(module, node->value);
				if (global) {
					if (ptr) {
						*ptr = true;
						return { LLVMTypeOf(global), global };
					} else
						return { LLVMGlobalGetValueType(global), LLVMBuildLoad(block, global, "") };
				}

				global = LLVMGetNamedFunction(module, node->value);
				if (global) {
					if (ptr)
						*ptr = true;
					return { LLVMTypeOf(global), global};
				} else {
					global = LLVMGetNamedFunction(module, ".ctor`" + node->value);
					if (global)
						return { LLVMTypeOf(global), global };
				}

				uint32_t i = 0;
				for (const auto& arg : *curArgs) {
					if (arg.second == node->value) {
						if (i < LLVMCountParams(func)) {
							LLVMValueRef fnArg = LLVMGetParam(func, i);
							if (fnArg) {
								LLVMTypeRef type = LLVMTypeOf(fnArg);
								if (ptr) {
									*ptr = true;
									LLVMValueRef addr = LLVMBuildAlloca(block, type, "");
									LLVMBuildStore(block, fnArg, addr);
									return { LLVMTypeOf(addr), addr };
								}
								else
									return { type, fnArg };
							}
						}

						if (argsMap) {
							LLVMValueRef mapped = argsMap->operator[](i);
							LLVMTypeRef type = LLVMTypeOf(mapped);
							if (ptr) {
								*ptr = true;
								LLVMValueRef addr = LLVMBuildAlloca(block, type, "");
								LLVMBuildStore(block, mapped, addr);
								return { LLVMTypeOf(addr), addr };
							}
							else {
								return { type, mapped };
							}
						}
					}
					i++;
				}

				for (const auto& local : locals) {
					if (local.name == node->value) {
						LLVMTypeRef type = local.type.llvm;
						if (ptr) {
							*ptr = true;
							return { LLVMTypeOf(local.allocated), local.allocated };
						} else {
							LLVMValueRef value = nullptr;
							if (LLVMGetTypeKind(type) == LLVMArrayTypeKind) {
								LLVMValueRef zero = LLVMConstInt(LLVMInt32Type(), 0, 0);
								LLVMValueRef idx[] = { zero, zero };
								value = LLVMBuildInBoundsGEP(block, local.allocated, idx, _countof(idx), "");
								type = LLVMTypeOf(value);
							} else
								value = LLVMBuildLoad(block, local.allocated, "");

							return { type, value };
						}
					}
				}
			} else {
				if (constant)
					TEA_PANIC("value is not a constant expression. line %d, column %d", node->line, node->column);

				LLVMValueRef global = LLVMGetNamedGlobal(module, node->value);
				if (global) {
					if (ptr) {
						*ptr = true;
						return { LLVMTypeOf(global), global };
					} else
						return { LLVMGlobalGetValueType(global), LLVMBuildLoad(block, global, "") };
				}

				bool first = true;
				char* context;
				char* tmp = new char[node->value.size + 1];
				memcpy_s(tmp, node->value.size + 1, node->value.data, node->value.size);
				tmp[node->value.size] = '\0';
				char* tok = strtok_s(tmp, "::", &context); // bumass function modifies the passed buffer
				while (tok != nullptr) {
					char* next = strtok_s(nullptr, "::", &context);

					if (!first)
						TEA_PANIC("deep scopes are not yet implemented. line %d, column %d", node->line, node->column);
					else {
						ImportedModule& mod = modules[tok];
						if (auto* it = mod.find(next)) {
							if (ptr)
								*ptr = true;
							return { LLVMTypeOf(*it), *it };
						}
						else
							TEA_PANIC("'%s' is not a valid member of module '%s'. line %d, column %d", next, tok, node->line, node->column); // next has junk after the actual context
						first = false;
					}

					tok = strtok_s(nullptr, "::", &context);
				}
			}

			TEA_PANIC("'%s' is not defined. line %d, column %d", node->value.data, node->line, node->column);
			TEA_UNREACHABLE();
		} break;

		case EXPR_NOT: {
			auto [type, expr] = emitExpression(node->left, constant);

			LLVMValueRef val;
			switch (LLVMGetTypeKind(type.llvm)) {
			case LLVMIntegerTypeKind: {
				LLVMValueRef zero = LLVMConstInt(type.llvm, 0, false);
				val = LLVMBuildICmp(block, LLVMIntEQ, expr, zero, "");
				break;
			}

			case LLVMFloatTypeKind:
			case LLVMDoubleTypeKind: {
				LLVMValueRef zero = LLVMConstReal(type.llvm, 0.0);
				val = LLVMBuildFCmp(block, LLVMRealOEQ, expr, zero, "");
				break;
			}

			case LLVMPointerTypeKind: {
				LLVMValueRef nullPtr = LLVMConstPointerNull(type.llvm);
				val = LLVMBuildICmp(block, LLVMIntEQ, expr, nullPtr, "");
				break;
			}

			default:
				TEA_PANIC("unsupported type in '!' operation. line %d, column %d", node->line, node->column);
			}

			return { Type::get(Type::BOOL), val };
		} break;

		case EXPR_AND: {
			auto [lhsType, lhs] = emitExpression(node->left, constant);
			auto [rhsType, rhs] = emitExpression(node->right, constant);

			return { Type::get(Type::BOOL), LLVMBuildAnd(block, lhs, rhs, "") };
		} break;

		case EXPR_OR: {
			auto [lhsType, lhs] = emitExpression(node->left, constant);
			auto [rhsType, rhs] = emitExpression(node->right, constant);

			return { Type::get(Type::BOOL), LLVMBuildOr(block, lhs, rhs, "") };
		} break;

		case EXPR_EQ: {
			auto [lhsType, lhs] = emitExpression(node->left, constant);
			auto [rhsType, rhs] = emitExpression(node->right, constant);

			LLVMValueRef cmp = NULL;

			if (LLVMGetTypeKind(lhsType.llvm) == LLVMIntegerTypeKind && LLVMGetTypeKind(rhsType.llvm) == LLVMIntegerTypeKind)
				cmp = LLVMBuildICmp(block, LLVMIntEQ, lhs, rhs, "");
			else if (
				(LLVMGetTypeKind(lhsType.llvm) == LLVMFloatTypeKind || LLVMGetTypeKind(lhsType.llvm) == LLVMDoubleTypeKind) &&
				(LLVMGetTypeKind(rhsType.llvm) == LLVMFloatTypeKind || LLVMGetTypeKind(rhsType.llvm) == LLVMDoubleTypeKind)
			)
				cmp = LLVMBuildFCmp(block, LLVMRealUEQ, lhs, rhs, "");
			else
				TEA_PANIC("type mismatch in '==' operation. line %d, column %d", node->line, node->column);

			return { Type::get(Type::BOOL), cmp};
		} break;

		case EXPR_NEQ: {
			auto [lhsType, lhs] = emitExpression(node->left, constant);
			auto [rhsType, rhs] = emitExpression(node->right, constant);

			if (LLVMGetTypeKind(lhsType.llvm) != LLVMGetTypeKind(rhsType.llvm))
				TEA_PANIC("type mismatch in '!=' operation. line %d, column %d", node->line, node->column);

			LLVMValueRef cmp = LLVMBuildICmp(block, LLVMIntNE, lhs, rhs, "");
			return { Type::get(Type::BOOL), cmp };
		} break;

		case EXPR_LT: {
			auto [lhsType, lhs] = emitExpression(node->left, constant);
			auto [rhsType, rhs] = emitExpression(node->right, constant);

			if (LLVMGetTypeKind(lhsType.llvm) != LLVMGetTypeKind(rhsType.llvm))
				TEA_PANIC("type mismatch in '<' operation. line %d, column %d", node->line, node->column);

			LLVMValueRef cmp = LLVMBuildICmp(block, LLVMIntSLT, lhs, rhs, "");
			return { Type::get(Type::BOOL), cmp };
		} break;

		case EXPR_GT: {
			auto [lhsType, lhs] = emitExpression(node->left, constant);
			auto [rhsType, rhs] = emitExpression(node->right, constant);

			if (LLVMGetTypeKind(lhsType.llvm) != LLVMGetTypeKind(rhsType.llvm))
				TEA_PANIC("type mismatch in '>' operation. line %d, column %d", node->line, node->column);

			LLVMValueRef cmp = LLVMBuildICmp(block, LLVMIntSGT, lhs, rhs, "");
			return { Type::get(Type::BOOL), cmp };
		} break;

		case EXPR_LE: {
			auto [lhsType, lhs] = emitExpression(node->left, constant);
			auto [rhsType, rhs] = emitExpression(node->right, constant);

			if (LLVMGetTypeKind(lhsType.llvm) != LLVMGetTypeKind(rhsType.llvm))
				TEA_PANIC("type mismatch in '<=' operation. line %d, column %d", node->line, node->column);

			LLVMValueRef cmp = LLVMBuildICmp(block, LLVMIntSLE, lhs, rhs, "");
			return { Type::get(Type::BOOL), cmp };
		} break;

		case EXPR_GE: {
			auto [lhsType, lhs] = emitExpression(node->left, constant);
			auto [rhsType, rhs] = emitExpression(node->right, constant);

			if (LLVMGetTypeKind(lhsType.llvm) != LLVMGetTypeKind(rhsType.llvm))
				TEA_PANIC("type mismatch in '>=' operation. line %d, column %d", node->line, node->column);

			LLVMValueRef cmp = LLVMBuildICmp(block, LLVMIntSGE, lhs, rhs, "");
			return { Type::get(Type::BOOL), cmp };
		} break;

		case EXPR_REF: {
			if (constant)
				TEA_PANIC("value is not a constant expression. line %d, column %d", node->line, node->column);

			bool ptr = false;
			auto [type, value] = emitExpression(node->left, false, &ptr);

			if (!ptr)
				TEA_PANIC("value cannot be referenced. line %d, column %d", node->line, node->column);

			return { LLVMIsAGlobalValue(value) ? LLVMGlobalGetValueType(value) : LLVMTypeOf(value), value };
		} break;

		case EXPR_DEREF: {
			if (constant)
				TEA_PANIC("value is not a constant expression. line %d, column %d", node->line, node->column);

			auto [type, value] = emitExpression(node->left);
			if (LLVMGetTypeKind(type.llvm) != LLVMPointerTypeKind)
				TEA_PANIC("cannot dereference non-pointer type. line %d, column %d", node->line, node->column);

			if (ptr) {
				*ptr = true;
				return { type.llvm, value };
			} else
				return { LLVMGetElementType(type.llvm), LLVMBuildLoad(block, value, "") };
		} break;

		case EXPR_CAST: {
			auto [castTo, success1] = Type::get(node->value); // success for my buddies success for my friends // lol alex g
			if (!success1)
				TEA_PANIC("unknown type '%s' in cast. line %d, column %d", node->value.data, node->line, node->column);

			auto [type, val] = emitExpression(node->left);
			auto [success2, casted] = util::cast(block, castTo, type, val); // success is the only thing i understand // harvey - alex g
			if (!success2)
				TEA_PANIC("unable to cast '%s' to '%s'. line %d, column %d", type2readable(type).data, type2readable(castTo).data, node->line, node->column);

			return { castTo, casted };
		} break;

		case EXPR_INDEX: {
			IndexNode* indexNode = (IndexNode*)node.get();

			Type lhsType;
			LLVMValueRef lhs;
			if (indexNode->value[0] == 1) {
				bool ptr = false;
				std::tie(lhsType, lhs) = emitExpression(indexNode->left, false, &ptr);
			} else
				std::tie(lhsType, lhs) = emitExpression(indexNode->left);
			
			if (LLVMGetTypeKind(lhsType.llvm) != LLVMPointerTypeKind)
				TEA_PANIC("cannot index a value of type '%s'", type2readable(lhsType).data, node->line, node->column);

			if (indexNode->value[0] == 0) {
				auto [idxType, idx] = emitExpression(indexNode->right);
				if (ptr) {
					*ptr = true;
					return { lhsType.llvm, LLVMBuildGEP(block, lhs, &idx, 1, "") };
				} else
					return { LLVMGetElementType(lhsType.llvm), LLVMBuildLoad(block, LLVMBuildGEP(block, lhs, &idx, 1, ""), "") };
			} else {
				LLVMTypeRef t = LLVMGetElementType(lhsType.llvm);
				ObjectNode* obj = objects[t];
				if (!obj)
					TEA_PANIC("cannot index a value of type '%s'. line %d, column %d", type2readable(lhsType).data, node->line, node->column);

				int idx = 0;
				bool found = false;
				for (const auto& field : obj->fields) {
					if ((inClassContext || field->storage == STORAGE_PUBLIC) && field->name == indexNode->right->value) {
						found = true;
						break;
					}
					idx++;
				}

				if (found) {
					LLVMValueRef val = LLVMBuildStructGEP(block, lhs, idx, "");
					if (ptr)
						*ptr = true;
					else
						val = LLVMBuildLoad(block, val, "");
					return { LLVMTypeOf(val), val };
				} else {
					LLVMValueRef val = nullptr;
					for (const auto& method : obj->methods)
						if (method->name == indexNode->right->value)
							val = LLVMGetNamedFunction(module, indexNode->right->value + "`" + LLVMGetStructName(t));
					if (!val)
						TEA_PANIC("'%s' is not a valid member of object '%s'. line %d, column %d", indexNode->right->value.data, type2readable(t).data, node->line, node->column);
					if (LLVMGetTypeKind(LLVMTypeOf(val)) == LLVMPointerTypeKind && LLVMGetTypeKind(LLVMGetElementType(LLVMTypeOf(val))) == LLVMFunctionTypeKind)
						self = lhs; // 99% chance this is a method call
					return { LLVMTypeOf(val), val };
				}
			}
		} break;

		case EXPR_ARRAY: {
			ArrayNode* arrayNode = (ArrayNode*)node.get();
			int size = arrayNode->init.size;

			auto [elementType, firstValue] = emitExpression(arrayNode->init[0], constant);

			vector<LLVMValueRef> elements;
			elements.reserve(size);

			bool constInit = true;
			if (constInit)
				elements.push(firstValue);

			for (int i = 1; i < size; i++) {
				auto [t, v] = emitExpression(arrayNode->init[i]);
				if (t != elementType)
					TEA_PANIC("type mismatch in array initializer (expected '%s', got '%s'. line %d, column %d", type2readable(t), type2readable(elementType), node->line, node->column);

				if (LLVMIsAConstant(v))
					elements.push(v);
				else
					constInit = false;
			}

			LLVMTypeRef arrType = LLVMArrayType(elementType.llvm, size);

			if (constInit)
				return { arrType, LLVMConstArray(elementType.llvm, elements.data, size) };
			else {
				LLVMValueRef arr = LLVMBuildAlloca(block, arrType, "");

				for (int i = 0; i < size; i++) {
					auto [t, v] = emitExpression(arrayNode->init[i]);

					LLVMValueRef indices[] = { LLVMConstInt(LLVMInt32Type(), 0, 0), LLVMConstInt(LLVMInt32Type(), i, 0) };
					LLVMValueRef ptr = LLVMBuildInBoundsGEP(block, arr, indices, 2, "");
					LLVMBuildStore(block, v, ptr);
				}

				return { arrType, arr };
			}
		}

		default:
		invalid:
			TEA_PANIC("invalid expression. line %d, column %d", node->line, node->column);
			TEA_UNREACHABLE();
		}
	}
}
