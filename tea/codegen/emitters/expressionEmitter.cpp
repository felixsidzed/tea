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

				if (ltype == Type::get(Type::INT).llvm) {
					uint64_t lval = LLVMConstIntGetSExtValue(lhs);
					uint64_t rval = LLVMConstIntGetSExtValue(rhs);
					switch (node->etype) {
					case EXPR_ADD: return { ltype, LLVMConstInt(Type::get(Type::INT).llvm, lval + rval, ltype.sign) };
					case EXPR_SUB: return { ltype, LLVMConstInt(Type::get(Type::INT).llvm, lval - rval, ltype.sign) };
					case EXPR_MUL: return { ltype, LLVMConstInt(Type::get(Type::INT).llvm, lval * rval, ltype.sign) };
					case EXPR_DIV: return { ltype, LLVMConstInt(Type::get(Type::INT).llvm, lval / rval, ltype.sign) };
					}
				}
				else if (ltype == Type::get(Type::FLOAT) || ltype == Type::get(Type::DOUBLE)) {
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

			if (ltype != rtype)
				TEA_PANIC("type mismatch in expression. line %d, column %d", node->line, node->column);

			LLVMValueRef result;

			switch (node->etype) {
			case EXPR_ADD: {
				if (ltype == Type::get(Type::INT))
					result = LLVMBuildAdd(block, lhs, rhs, "");
				else if (ltype == Type::get(Type::FLOAT) || rtype == Type::get(Type::DOUBLE))
					result = LLVMBuildFAdd(block, lhs, rhs, "");
				else {
					TEA_PANIC("unsupported types for addition. line %d, column %d", node->line, node->column);
					TEA_UNREACHABLE();
				}
			} break;

			case EXPR_SUB: {
				if (ltype == Type::get(Type::INT))
					result = LLVMBuildSub(block, lhs, rhs, "");
				else if (ltype == Type::get(Type::FLOAT) || rtype == Type::get(Type::DOUBLE))
					result = LLVMBuildFSub(block, lhs, rhs, "");
				else {
					TEA_PANIC("unsupported types for subtraction. line %d, column %d", node->line, node->column);
					TEA_UNREACHABLE();
				}
			} break;

			case EXPR_MUL: {
				if (ltype == Type::get(Type::INT))
					result = LLVMBuildMul(block, lhs, rhs, "");
				else if (ltype == Type::get(Type::FLOAT) || rtype == Type::get(Type::DOUBLE))
					result = LLVMBuildFMul(block, lhs, rhs, "");
				else {
					TEA_PANIC("unsupported types for multiplication. line %d, column %d", node->line, node->column);
					TEA_UNREACHABLE();
				}
			} break;

			case EXPR_DIV: {
				if (ltype == Type::get(Type::INT))
					result = LLVMBuildSDiv(block, lhs, rhs, "");
				else if (ltype == Type::get(Type::FLOAT) || rtype == Type::get(Type::DOUBLE))
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
				LLVMConstInt(Type::get(Type::CHAR).llvm, node->value[0], true)
			};
		}

		case EXPR_CALL: {
			if (constant)
				TEA_PANIC("value is not a constant expression. line %d, column %d", node->line, node->column);

			CallNode* call = (CallNode*)node.get();
			vector<LLVMValueRef> args;
			vector<LLVMTypeRef> argTypes;
			args.reserve(call->args.size);

			for (const auto& arg : call->args) {
				auto [t, v] = emitExpression(arg);
				argTypes.push(t.llvm);
				args.push(v);
			}

			LLVMValueRef callee = nullptr;
			if (call->scope.size == 0) {
				// callee is stored in ExpressionNode::value
				callee = LLVMGetNamedFunction(module, call->value);

				if (!callee) {
					auto it = inlineables.find(call->value);
					if (it) { // TODO: improve
						FunctionNode* funcNode = *it;

						LLVMTypeRef returnType = funcNode->returnType.llvm;
						if (!returnType)
							TEA_PANIC("can't inline '%s' because the return type is unknown", funcNode->name.data);
						LLVMValueRef returnValue = nullptr;
						if (LLVMGetTypeKind(returnType) != LLVMVoidTypeKind)
							returnValue = LLVMBuildAlloca(block, returnType, ""); // TODO: force this to be in rax

						vector<std::pair<Type, string>>* oldCurArgs = curArgs;

						argsMap = &args;
						curArgs = &funcNode->args;

						std::pair<LLVMTypeRef, LLVMValueRef> returnInto = { returnType, returnValue };
						emitBlock(funcNode->body, ".`inlinedfunction", nullptr, &returnInto);

						argsMap = nullptr;
						curArgs = oldCurArgs;

						if (!returnValue)
							return { returnType, LLVMConstNull(LLVMPointerType(LLVMVoidType(), 0)) };
						else {
							if (ptr) {
								*ptr = true;
								return { LLVMTypeOf(returnValue), returnValue, };
							} else
								return { returnType, LLVMBuildLoad(block, returnValue, "") };
						}
						
					} else if (hasNoNamespaceFunctions) {
						for (int i = 0; i < modules.size(); i++) {
							const auto& [name, mod] = modules.data[i];
							auto it2 = mod.find(call->value);
							if (it2) {
								callee = *it2;
								break;
							}
						}
					}
				}

			} else if (call->scope.size == 1) {
				const string& scope = call->scope[0];
				auto it = modules.find(scope);
				if (it) {
					auto& imported = *it;
					auto it2 = imported.find(call->value);
					if (it2)
						callee = *it2;
				}
			} else
				TEA_PANIC("deep scopes are not yet implemented. line &d, column %d", node->line, node->column);

			if (!callee) {
				std::string fqn;
				if (!call->scope.empty()) {
					for (const auto& s : call->scope) {
						fqn.append(s);
						fqn.append(2, ':');
					}
				}
				fqn += call->value;

				TEA_PANIC("call to undefined function '%s'. line %d, column %d", fqn.c_str(), node->line, node->column);
			}

			LLVMTypeRef ftype = LLVMGlobalGetValueType(callee);
			uint32_t nargs = LLVMCountParamTypes(ftype);
			LLVMTypeRef* calleeArgTypes = new LLVMTypeRef[nargs + 1];
			LLVMGetParamTypes(ftype, calleeArgTypes);

			if ((!LLVMIsFunctionVarArg(ftype) && nargs != argTypes.size) || (LLVMIsFunctionVarArg(ftype) && nargs > argTypes.size))
				TEA_PANIC("argument count mismatch. expected %d, got %d. line %d, column %d", nargs, argTypes.size, node->line, node->column);

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
			if (node->value == "true") {
				return {
					Type::get(Type::BOOL),
					LLVMConstInt(Type::get(Type::BOOL).llvm, true, false)
				};
			} else if (node->value == "false") {
				return {
					Type::get(Type::BOOL),
					LLVMConstInt(Type::get(Type::BOOL).llvm, false, false)
				};
			} else if (node->value == "null") {
				auto pvoid = LLVMPointerType(Type::get(Type::VOID_).llvm, 0);
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
						return {
							LLVMTypeOf(local.allocated),
							local.allocated
						};
					} else {
						LLVMValueRef value = nullptr;
						if (LLVMGetTypeKind(type) == LLVMArrayTypeKind) {
							LLVMValueRef zero = LLVMConstInt(LLVMInt32Type(), 0, 0);
							LLVMValueRef idx[] = { zero, zero };
							value = LLVMBuildInBoundsGEP(block, local.allocated, idx, _countof(idx), "");
							type = LLVMTypeOf(value);
						} else
							value = LLVMBuildLoad(block, local.allocated, "");

						return {
							type,
							value
						};
					}
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

			return {
				LLVMIsAGlobalValue(value) ? LLVMGlobalGetValueType(value) : LLVMTypeOf(value),
				value
			};
		} break;

		case EXPR_DEREF: {
			if (constant)
				TEA_PANIC("value is not a constant expression. line %d, column %d", node->line, node->column);

			auto [type, value] = emitExpression(node->left);
			if (LLVMGetTypeKind(type.llvm) != LLVMPointerTypeKind)
				TEA_PANIC("cannot dereference non-pointer type. line %d, column %d", node->line, node->column);

			if (ptr) {
				*ptr = true;
				return {
					type.llvm,
					value
				};
			} else
				return {
					LLVMGetElementType(type.llvm),
					LLVMBuildLoad(block, value, "")
				};
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
			auto [arrType, arr] = emitExpression(indexNode->val);
			auto [idxType, idx] = emitExpression(indexNode->idx);

			if (ptr) {
				*ptr = true;
				return { arrType.llvm, LLVMBuildGEP(block, arr, &idx, 1, "") };
			} else
				return { LLVMGetElementType(arrType.llvm), LLVMBuildLoad(block, LLVMBuildGEP(block, arr, &idx, 1, ""), "") };
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
