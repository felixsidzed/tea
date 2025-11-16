#include "SemanticAnalyzer.h"

#include "mir/Context.h"

#define pushscope() scope = scopeHistory.emplace()
#define popscope() scope = &scopeHistory[scopeHistory.size - 1]; scopeHistory.pop()

#define pushsym(name, type, ...) scope->emplace(name, type, __VA_ARGS__)

namespace tea::frontend::analysis {

	tea::vector<tea::string> SemanticAnalyzer::visit(const frontend::AST::Tree& root) {
		pushscope();

		for (const auto& node : root) {
			switch (node->kind) {
			case AST::NodeKind::Function: {
				func = (AST::FunctionNode*)node.get();
				pushsym(func->name, Type::Pointer(Type::Void()), false, true, false, func->vis == AST::StorageClass::Public, true);
				visitBlock(func->body);
				func = nullptr;
			} break;

			default:
				// TODO: maybe we should throw an error here
				break;
			}
		}

		popscope();

		{
			auto copy = errors;
			errors.clear();
			return copy;
		}
	}

	SemanticAnalyzer::Symbol* SemanticAnalyzer::lookup(const tea::string& name) {
		for (auto it = scopeHistory.end() - 1; it != scopeHistory.begin(); it--)
			for (auto& sym : *it)
				if (sym.name == name)
					return &sym;
		return nullptr;
	}

	void SemanticAnalyzer::visitBlock(const AST::Tree& tree) {
		pushscope();

		for (const auto& node : tree)
			visitStat(node.get());

		popscope();
	}

	void SemanticAnalyzer::visitStat(const frontend::AST::Node* node) {
		switch (node->kind) {
		case AST::NodeKind::Return: {
			Type* returnedType = visitExpression(((const frontend::AST::ReturnNode*)node)->value.get());
			if (func->returnType != returnedType)
				errors.push(std::format("Function '{}': return type mismatch, expected '{}', got '{}'. line {}, column {}",
					func->name, func->returnType->str(), returnedType->str(), node->line, node->column).c_str());
		} break;

		default:
			break;
		}
	}

	Type* SemanticAnalyzer::visitExpression(const frontend::AST::ExpressionNode* node) {
		switch (node->ekind) {
		case AST::ExprKind::String:
			return Type::String();
		case AST::ExprKind::Char:
			return Type::Char();
		case AST::ExprKind::Int:
			return Type::Int();
		case AST::ExprKind::Float:
			return Type::Float();
		case AST::ExprKind::Double:
			return Type::Float();
		default:
			return nullptr;
		}
	}

} // namespace tea::analysis
