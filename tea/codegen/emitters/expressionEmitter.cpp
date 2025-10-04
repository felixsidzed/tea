#include "../codegen.h"
#include "tea.h"

namespace tea {
	std::pair<LLVMTypeRef, LLVMValueRef> CodeGen::emitExpression(const std::unique_ptr<ExpressionNode>& node) {
		if (node->etype >= EXPR_ADD && node->etype <= EXPR_DIV) {
			auto [ltype, lhs] = emitExpression(node->left);
			auto [rtype, rhs] = emitExpression(node->right);

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
				callee = LLVMGetNamedFunction(module, call->value.c_str()); // callee is stored in ExpressionNode::value

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
						i + 1, llvm2readable(expected), llvm2readable(got, args[i]), node->line, node->column);
			}

			delete[] calleeArgTypes;

			return {
				LLVMGetReturnType(LLVMGlobalGetValueType(callee)),
				LLVMBuildCall2(block, LLVMGlobalGetValueType(callee), callee, args.data(), (uint32_t)args.size(), "")
			};
		}

		case EXPR_IDENTF: {
			int i = 0;
			for (const auto& arg : *curArgs) {
				if (arg.second == node->value) {
					LLVMValueRef arg = LLVMGetParam(func, i);
					if (arg)
						return {
							LLVMTypeOf(arg),
							arg
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
				i++;
			}
			
			TEA_PANIC("'%s' is not defined. line %d, column %d", node->value.c_str(), node->line, node->column);
			TEA_UNREACHABLE();
			break;
		}

		default:
		invalid:
			TEA_PANIC("invalid expression. line %d, column %d", node->line, node->column);
			TEA_UNREACHABLE();
		}
	}
}
