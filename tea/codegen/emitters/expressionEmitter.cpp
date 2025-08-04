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
			return {
				type2llvm[TYPE_STRING],
				LLVMBuildGlobalStringPtr(block, node->value.c_str(), "")
			};
		} break;

		case EXPR_CALL: {
			CallNode* call = (CallNode*)node.get();
			std::vector<LLVMValueRef> args;
			args.reserve(call->args.size());

			for (const auto& arg : call->args)
				args.push_back(emitExpression(arg).second);

			LLVMValueRef callee = nullptr;
			if (call->scope.size() == 0) {
				callee = LLVMGetNamedFunction(module, call->value.c_str()); // callee is stored in ExpressionNode::value
			} // TODO: scoped calls

			if (!callee)
				TEA_PANIC("call to undefined function '%s'. line %d, column %d", call->value.c_str(), node->line, node->column);

			// TODO: argument type checking

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
			
			TEA_PANIC("'%s' is not defined. line %d, column %d", node->value.c_str(), node->line, node->column);
			TEA_UNREACHABLE();
		}

		default:
		invalid:
			TEA_PANIC("invalid expression. line %d, column %d", node->line, node->column);
			TEA_UNREACHABLE();
		}
	}
}
