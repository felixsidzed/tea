#include "codegen/codegen.h"

#include "common/tea.h"
#include <iostream>

namespace tea {

	mir::Value* CodeGen::emitExpression(const AST::ExpressionNode* node) {
		switch (node->getEKind()) {
		case AST::ExprKind::String: {
			const AST::LiteralNode* literal = (const AST::LiteralNode*)node;
			return builder.globalString(literal->value);
		}

		case AST::ExprKind::Char: {
			const AST::LiteralNode* literal = (const AST::LiteralNode*)node;
			return mir::ConstantNumber::get(literal->value[0], 8);
		}

		case AST::ExprKind::Int: {
			const AST::LiteralNode* literal = (const AST::LiteralNode*)node;
			uint64_t val = std::stoull(std::string(literal->value.data(), literal->value.length()), nullptr, 0);
			return mir::ConstantNumber::get(
				val, 32, node->type->sign
			);
		}

		case AST::ExprKind::Double:
		case AST::ExprKind::Float: {
			const AST::LiteralNode* literal = (const AST::LiteralNode*)node;
			return mir::ConstantNumber::get<double>(
				std::stod(std::string(literal->value.data(), literal->value.length()), nullptr),
				node->getEKind() == AST::ExprKind::Float ? 32 : 64,
				node->type->sign
			);
		}

		case AST::ExprKind::Identf: {
			const AST::LiteralNode* literal = (const AST::LiteralNode*)node;

			if (strchr(literal->value, ':')) {
				bool first = true;
				std::string str(literal->value.data(), literal->value.length());
				tea::vector<tea::string> parts;

				size_t start = 0, pos = 0;
				while ((pos = str.find("::", start)) != std::string::npos) {
					parts.emplace(str.data() + start, pos - start);
					start = pos + 2;
				}
				parts.emplace(str.data() + start, str.length() - start);

				for (uint32_t i = 0; i < parts.size; i++) {
					const tea::string& part = parts[i];

					if (!first)
						TEA_PANIC("deep scopes are not yet implemented. line %d, column %d", node->line, node->column);
					else {
						if (auto* modIt = importedModules.find(part)) {
							if (i + 1 < parts.size) {
								const tea::string& next = parts[i + 1];
								if (auto* it = modIt->find(next))
									return *it;
								else
									TEA_PANIC("'%s' is not a valid member of module '%s'. line %d, column %d", next.data(), part.data(), node->line, node->column);
							}
						} else
							TEA_PANIC("'%s' does not reference a valid scope. line %d, column %d", part.data(), node->line, node->column);
						first = false;
					}
				}
			} else {
				if (literal->value == "true")
					return mir::ConstantNumber::get(1, 1);
				else if (literal->value == "false")
					return mir::ConstantNumber::get(0, 1);
				else if (literal->value == "null")
					return mir::ConstantPointer::get(Type::Void(), 0);

				{
					mir::Global* g = module->getNamedGlobal(literal->value);
					if (g) return builder.load(g, "");
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
				} {
					if (auto* it = locals.find(std::hash<tea::string>()(literal->value))) {
						if (!it->load)
							it->load = builder.load(it->allocated, literal->value.data());
						return it->load;
					}
				}
			}

			TEA_PANIC("use of undefined symbol '%s'. line %d, column %d", literal->value.data(), literal->line, literal->column);
		} break;

		case AST::ExprKind::Call: {
			const AST::CallNode* call = (const AST::CallNode*)node;
			mir::Value* val = emitExpression(call->callee.get());
			
			tea::vector<mir::Value*> args;
			for (const auto& arg : call->args)
				args.emplace(emitExpression(arg.get()));

			return builder.call(val, args.data, args.size, "");
		} break;

		case AST::ExprKind::Add:
		case AST::ExprKind::Sub:
		case AST::ExprKind::Mul:
		case AST::ExprKind::Div: {
			const AST::BinaryExpr* be = (const AST::BinaryExpr*)node;

			mir::Value* lhs = emitExpression(be->lhs.get());
			mir::Value* rhs = emitExpression(be->rhs.get());

			switch (node->getEKind()) {
			case AST::ExprKind::Add:
				return builder.arithm(mir::OpCode::Add, lhs, rhs, "");
			case AST::ExprKind::Sub:
				return builder.arithm(mir::OpCode::Sub, lhs, rhs, "");
			case AST::ExprKind::Mul:
				return builder.arithm(mir::OpCode::Mul, lhs, rhs, "");
			case AST::ExprKind::Div:
				return builder.arithm(mir::OpCode::Div, lhs, rhs, "");
			default:
				break;
			}
		} break;

		default:
			TEA_PANIC("unknown expression kind %d", node->getEKind());
			TEA_UNREACHABLE();
		}

		return nullptr;
	}

} // namespace tea
