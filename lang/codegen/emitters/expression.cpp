#include "codegen/codegen.h"

#include "common/tea.h"
#include <iostream>

namespace tea {

	mir::Value* CodeGen::emitExpression(const AST::ExpressionNode* node) {
		switch (node->getEKind()) {
		case AST::ExprKind::String: {
			AST::LiteralNode* literal = (AST::LiteralNode*)node;
			return builder.globalString(literal->value);
		}

		case AST::ExprKind::Char: {
			AST::LiteralNode* literal = (AST::LiteralNode*)node;
			return mir::ConstantNumber::get(literal->value[0], 8);
		}

		case AST::ExprKind::Int: {
			AST::LiteralNode* literal = (AST::LiteralNode*)node;
			uint64_t val = std::stoull(std::string(literal->value.data(), literal->value.length()), nullptr, 0);
			return mir::ConstantNumber::get(
				val, 32, node->type->sign
			);
		}

		case AST::ExprKind::Double:
		case AST::ExprKind::Float: {
			AST::LiteralNode* literal = (AST::LiteralNode*)node;
			return mir::ConstantNumber::get<double>(
				std::stod(std::string(literal->value.data(), literal->value.length()), nullptr),
				node->getEKind() == AST::ExprKind::Float ? 32 : 64,
				node->type->sign
			);
		}

		case AST::ExprKind::Identf: {
			AST::LiteralNode* literal = (AST::LiteralNode*)node;

			{
				mir::Global* g = module->getNamedGlobal(literal->value);
				if (g) return g;
			} {
				mir::Function* f = module->getNamedFunction(literal->value);
				if (f) return f;
			} if (curParams) {
				uint32_t i = 0;
				for (const auto& [_, name] : *curParams) {
					if (literal->value == name)
						return builder.getInsertBlock()->parent->getParam(i);
					i++;
				}
			}

			TEA_PANIC("use of undefined symbol '%s'. line %d, column %d", literal->value, literal->line, literal->column);
		} break;

		case AST::ExprKind::Call: {
			AST::CallNode* call = (AST::CallNode*)node;
			mir::Value* val = emitExpression(call->callee.get());
			
			tea::vector<mir::Value*> args;
			for (const auto& arg : call->args)
				args.emplace(emitExpression(arg.get()));

			return builder.call(val, args.data, args.size, "");
		} break;

		default:
			TEA_PANIC("unknown expression kind %d", node->getEKind());
			TEA_UNREACHABLE();
		}

		return nullptr;
	}

} // namespace tea
