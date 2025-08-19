#include "../codegen.h"
#include "tea.h"

namespace tea {
	std::pair<LLVMTypeRef, LLVMValueRef> CodeGen::emitExpression(const std::unique_ptr<ExpressionNode>& node, bool constant) {
		if (node->etype >= EXPR_ADD && node->etype <= EXPR_DIV) {
			auto [ltype, lhs] = emitExpression(node->left, constant);
			auto [rtype, rhs] = emitExpression(node->right, constant);

			if (constant) {
				if (!LLVMIsConstant(lhs))
					TEA_PANIC("value is not a constant expression. line %d, column %d", node->left->line, node->left->column);
				if (!LLVMIsConstant(rhs))
					TEA_PANIC("value is not a constant expression. line %d, column %d", node->right->line, node->right->column);

				if (ltype == type2llvm[TYPE_INT]) {
					long long lval = LLVMConstIntGetSExtValue(lhs);
					long long rval = LLVMConstIntGetSExtValue(rhs);
					switch (node->etype) {
					case EXPR_ADD: return { ltype, LLVMConstInt(type2llvm[TYPE_INT], lval + rval, true) };
					case EXPR_SUB: return { ltype, LLVMConstInt(type2llvm[TYPE_INT], lval - rval, true) };
					case EXPR_MUL: return { ltype, LLVMConstInt(type2llvm[TYPE_INT], lval * rval, true) };
					case EXPR_DIV: return { ltype, LLVMConstInt(type2llvm[TYPE_INT], lval / rval, true) };
					}
				}
				else if (ltype == type2llvm[TYPE_FLOAT] || ltype == type2llvm[TYPE_DOUBLE]) {
					LLVMBool _;
					double lval = LLVMConstRealGetDouble(lhs, &_);
					double rval = LLVMConstRealGetDouble(rhs, &_);
					switch (node->etype) {
					case EXPR_ADD: return { ltype, LLVMConstReal(ltype, lval + rval) };
					case EXPR_SUB: return { ltype, LLVMConstReal(ltype, lval - rval) };
					case EXPR_MUL: return { ltype, LLVMConstReal(ltype, lval * rval) };
					case EXPR_DIV: return { ltype, LLVMConstReal(ltype, lval / rval) };
					}
				}
			}

			if (ltype != rtype)
				TEA_PANIC("type mismatch in expression. line %d, column %d", node->line, node->column);

			LLVMValueRef result;

			switch (node->etype) {
			case EXPR_ADD: {
				if (ltype == type2llvm[TYPE_INT])
					result = LLVMBuildAdd(block, lhs, rhs, "");
				else if (ltype == type2llvm[TYPE_FLOAT] || rtype == type2llvm[TYPE_DOUBLE])
					result = LLVMBuildFAdd(block, lhs, rhs, "");
				else {
					TEA_PANIC("unsupported types for addition. line %d, column %d", node->line, node->column);
					TEA_UNREACHABLE();
				}
			} break;

			case EXPR_SUB: {
				if (ltype == type2llvm[TYPE_INT])
					result = LLVMBuildSub(block, lhs, rhs, "");
				else if (ltype == type2llvm[TYPE_FLOAT] || rtype == type2llvm[TYPE_DOUBLE])
					result = LLVMBuildFSub(block, lhs, rhs, "");
				else {
					TEA_PANIC("unsupported types for subtraction. line %d, column %d", node->line, node->column);
					TEA_UNREACHABLE();
				}
			} break;

			case EXPR_MUL: {
				if (ltype == type2llvm[TYPE_INT])
					result = LLVMBuildMul(block, lhs, rhs, "");
				else if (ltype == type2llvm[TYPE_FLOAT] || rtype == type2llvm[TYPE_DOUBLE])
					result = LLVMBuildFMul(block, lhs, rhs, "");
				else {
					TEA_PANIC("unsupported types for multiplication. line %d, column %d", node->line, node->column);
					TEA_UNREACHABLE();
				}
			} break;

			case EXPR_DIV: {
				if (ltype == type2llvm[TYPE_INT])
					result = LLVMBuildSDiv(block, lhs, rhs, "");
				else if (ltype == type2llvm[TYPE_FLOAT] || rtype == type2llvm[TYPE_DOUBLE])
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
				type2llvm[TYPE_INT],
				LLVMConstInt(type2llvm[TYPE_INT], std::stoll(node->value), true)
			};
		} break;

		case EXPR_FLOAT: {
			return {
				type2llvm[TYPE_FLOAT],
				LLVMConstReal(type2llvm[TYPE_FLOAT], std::stod(node->value))
			};
		} break;

		case EXPR_DOUBLE: {
			return {
				type2llvm[TYPE_DOUBLE],
				LLVMConstReal(type2llvm[TYPE_DOUBLE], std::stod(node->value))
			};
		} break;

		case EXPR_STRING: {
			LLVMValueRef ptr = LLVMBuildGlobalString(block, node->value.c_str(), "");
			LLVMValueRef indicies[] = { LLVMConstInt(LLVMInt32Type(), 0, 0), LLVMConstInt(LLVMInt32Type(), 0, 0) };
			return {
				type2llvm[TYPE_STRING],
				ptr
			};
		} break;

		case EXPR_CALL: {
			if (constant)
				TEA_PANIC("value is not a constant expression. line %d, column %d", node->line, node->column);

			CallNode* call = (CallNode*)node.get();
			std::vector<LLVMValueRef> args;
			std::vector<LLVMTypeRef> argTypes;
			args.reserve(call->args.size());

			for (const auto& arg : call->args) {
				auto [t, v] = emitExpression(arg);
				argTypes.push_back(t);
				args.push_back(v);
			}

			LLVMValueRef callee = nullptr;
			if (call->scope.size() == 0) {
				// callee is stored in ExpressionNode::value
				callee = LLVMGetNamedFunction(module, call->value.c_str());

				if (!callee) {
					auto it = inlineables.find(call->value);
					if (it != inlineables.end()) { // TODO: improve
						FunctionNode* funcNode = it->second;

						LLVMTypeRef returnType = type2llvm[funcNode->returnType];
						LLVMValueRef returnValue = nullptr;
						if (LLVMGetTypeKind(returnType) != LLVMVoidTypeKind)
							returnValue = LLVMBuildAlloca(block, returnType, "");

						std::vector<std::pair<enum Type, std::string>>* oldCurArgs = curArgs;

						argsMap = &args;
						curArgs = &funcNode->args;

						std::pair<LLVMTypeRef, LLVMValueRef> returnInto = { returnType, returnValue };
						emitBlock(funcNode->body, nullptr, nullptr, &returnInto);

						argsMap = nullptr;
						curArgs = oldCurArgs;

						if (!returnValue) {
							auto pvoid = LLVMPointerType(type2llvm[TYPE_VOID], 0);
							return {
								returnType,
								LLVMConstNull(pvoid)
							};
						} else
							return {
								returnType,
								LLVMBuildLoad2(block, returnType, returnValue, "")
							};
						
					}
				}

			} else if (call->scope.size() == 1) {
				const std::string& scope = call->scope[0];
				auto it = modules.find(scope);
				if (it != modules.end()) {
					auto& imported = it->second;
					auto it2 = imported.find(call->value);
					if (it2 != imported.end())
						callee = it2->second;
				}
			} else
				TEA_PANIC("deep scopes are not yet implemented. line &d, column %d", node->line, node->column);

			if (!callee) {
				std::string fqn;
				if (!call->scope.empty()) {
					for (const auto& s : call->scope)
						fqn += s + "::";
				}
				fqn += call->value;

				TEA_PANIC("call to undefined function '%s'. line %d, column %d", fqn.c_str(), node->line, node->column);
			}

			LLVMTypeRef ftype = LLVMGlobalGetValueType(callee);
			uint32_t nargs = LLVMCountParamTypes(ftype);
			LLVMTypeRef* calleeArgTypes = new LLVMTypeRef[nargs + 1];
			LLVMGetParamTypes(ftype, calleeArgTypes);

			if (nargs != argTypes.size())
				TEA_PANIC("argument count mismatch. expected %d, got %d. line %d, column %d", nargs, argTypes.size(), node->line, node->column);

			for (uint32_t i = 0; i < nargs; i++) {
				LLVMTypeRef got = argTypes[i];
				LLVMTypeRef expected = calleeArgTypes[i];

				if (got != expected)
					TEA_PANIC("argument %d: expected type %s, got %s. line %d, column %d",
						i + 1, llvm2readable(expected), llvm2readable(got), node->line, node->column);
			}

			delete[] calleeArgTypes;
			return {
				LLVMGetReturnType(ftype),
				LLVMBuildCall2(block, LLVMGlobalGetValueType(callee), callee,
								args.data(), (uint32_t)args.size(), "")
			};
		}

		case EXPR_IDENTF: {
			if (node->value == "true") {
				return {
					type2llvm[TYPE_BOOL],
					LLVMConstInt(type2llvm[TYPE_BOOL], 1, 0)
				};
			} else if (node->value == "false") {
				return {
					type2llvm[TYPE_BOOL],
					LLVMConstInt(type2llvm[TYPE_BOOL], 0, 0)
				};
			} else if (node->value == "null") {
				auto pvoid = LLVMPointerType(type2llvm[TYPE_VOID], 0);
				return {
					pvoid,
					LLVMConstNull(pvoid)
				};
			}

			LLVMValueRef global = LLVMGetNamedGlobal(module, node->value.c_str());
			if (global) {
				LLVMTypeRef type = LLVMGlobalGetValueType(global);
				return {
					type,
					LLVMBuildLoad2(block, type, global, "")
				};
			}

			if (constant)
				TEA_PANIC("value is not a constant expression. line %d, column %d", node->line, node->column);

			uint32_t i = 0;
			for (const auto& arg : *curArgs) {
				if (arg.second == node->value) {
					if (i < LLVMCountParams(func)) {
						LLVMValueRef arg = LLVMGetParam(func, i);
						if (arg)
							return {
								LLVMTypeOf(arg),
								arg
							};
					}
					if (argsMap)
						return {
							LLVMTypeOf(argsMap->at(i)),
							argsMap->at(i)
						};
				}
				i++;
			}

			for (const auto& local : locals) {
				if (local.name == node->value) {
					LLVMTypeRef type = type2llvm[local.type];
					LLVMValueRef val = LLVMBuildLoad2(block, type, local.allocated, "");
					if (val)
						return {
							type,
							val
						};
				}
			}
			
			TEA_PANIC("'%s' is not defined. line %d, column %d", node->value.c_str(), node->line, node->column);
			TEA_UNREACHABLE();
		} break;

		case EXPR_NOT: {
			auto [exprType, expr] = emitExpression(node->left, constant);

			if (LLVMGetTypeKind(exprType) != LLVMIntegerTypeKind)
				TEA_PANIC("type mismatch in '!' operation. line %d, column %d", node->line, node->column);

			LLVMValueRef notVal = LLVMBuildNot(block, expr, "");
			return { type2llvm[TYPE_BOOL], notVal };
		} break;

		case EXPR_AND: {
			auto [lhsType, lhs] = emitExpression(node->left, constant);
			auto [rhsType, rhs] = emitExpression(node->right, constant);

			return { type2llvm[TYPE_BOOL], LLVMBuildAnd(block, lhs, rhs, "") };
		} break;

		case EXPR_OR: {
			auto [lhsType, lhs] = emitExpression(node->left, constant);
			auto [rhsType, rhs] = emitExpression(node->right, constant);

			return { type2llvm[TYPE_BOOL], LLVMBuildOr(block, lhs, rhs, "") };
		} break;

		case EXPR_EQ: {
			auto [lhsType, lhs] = emitExpression(node->left, constant);
			auto [rhsType, rhs] = emitExpression(node->right, constant);

			LLVMValueRef cmp = NULL;

			if (LLVMGetTypeKind(lhsType) == LLVMIntegerTypeKind && LLVMGetTypeKind(rhsType) == LLVMIntegerTypeKind)
				cmp = LLVMBuildICmp(block, LLVMIntEQ, lhs, rhs, "");
			else if (
				(LLVMGetTypeKind(lhsType) == LLVMFloatTypeKind || LLVMGetTypeKind(lhsType) == LLVMDoubleTypeKind) &&
				(LLVMGetTypeKind(rhsType) == LLVMFloatTypeKind || LLVMGetTypeKind(rhsType) == LLVMDoubleTypeKind)
			)
				cmp = LLVMBuildFCmp(block, LLVMRealUEQ, lhs, rhs, "");
			else
				TEA_PANIC("type mismatch in '==' operation. line %d, column %d", node->line, node->column);

			return { type2llvm[TYPE_BOOL], cmp};
		} break;

		case EXPR_NEQ: {
			auto [lhsType, lhs] = emitExpression(node->left, constant);
			auto [rhsType, rhs] = emitExpression(node->right, constant);

			if (LLVMGetTypeKind(lhsType) != LLVMGetTypeKind(rhsType))
				TEA_PANIC("type mismatch in '!=' operation. line %d, column %d", node->line, node->column);

			LLVMValueRef cmp = LLVMBuildICmp(block, LLVMIntNE, lhs, rhs, "");
			return { type2llvm[TYPE_BOOL], cmp };
		} break;

		case EXPR_LT: {
			auto [lhsType, lhs] = emitExpression(node->left, constant);
			auto [rhsType, rhs] = emitExpression(node->right, constant);

			if (LLVMGetTypeKind(lhsType) != LLVMGetTypeKind(rhsType))
				TEA_PANIC("type mismatch in '<' operation. line %d, column %d", node->line, node->column);

			LLVMValueRef cmp = LLVMBuildICmp(block, LLVMIntSLT, lhs, rhs, "");
			return { type2llvm[TYPE_BOOL], cmp };
		} break;

		case EXPR_GT: {
			auto [lhsType, lhs] = emitExpression(node->left, constant);
			auto [rhsType, rhs] = emitExpression(node->right, constant);

			if (LLVMGetTypeKind(lhsType) != LLVMGetTypeKind(rhsType))
				TEA_PANIC("type mismatch in '>' operation. line %d, column %d", node->line, node->column);

			LLVMValueRef cmp = LLVMBuildICmp(block, LLVMIntSGT, lhs, rhs, "");
			return { type2llvm[TYPE_BOOL], cmp };
		} break;

		case EXPR_LE: {
			auto [lhsType, lhs] = emitExpression(node->left, constant);
			auto [rhsType, rhs] = emitExpression(node->right, constant);

			if (LLVMGetTypeKind(lhsType) != LLVMGetTypeKind(rhsType))
				TEA_PANIC("type mismatch in '<=' operation. line %d, column %d", node->line, node->column);

			LLVMValueRef cmp = LLVMBuildICmp(block, LLVMIntSLE, lhs, rhs, "");
			return { type2llvm[TYPE_BOOL], cmp };
		} break;

		case EXPR_GE: {
			auto [lhsType, lhs] = emitExpression(node->left, constant);
			auto [rhsType, rhs] = emitExpression(node->right, constant);

			if (LLVMGetTypeKind(lhsType) != LLVMGetTypeKind(rhsType))
				TEA_PANIC("type mismatch in '>=' operation. line %d, column %d", node->line, node->column);

			LLVMValueRef cmp = LLVMBuildICmp(block, LLVMIntSGE, lhs, rhs, "");
			return { type2llvm[TYPE_BOOL], cmp };
		} break;

		default:
		invalid:
			TEA_PANIC("invalid expression. line %d, column %d", node->line, node->column);
			TEA_UNREACHABLE();
		}
	}
}
