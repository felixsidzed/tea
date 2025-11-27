#include "SemanticAnalyzer.h"

#include "mir/Context.h"

#define pushscope() scope = scopeHistory.emplace()
#define popscope() scope = &scopeHistory[scopeHistory.size - 1]; scopeHistory.pop()

#define pushsym(name, type, ...) scope->emplace(name, type, __VA_ARGS__)

namespace tea::frontend::analysis {

	tea::vector<tea::string> SemanticAnalyzer::visit(const AST::Tree& root) {
		pushscope();

		for (const auto& node : root) {
			switch (node->kind) {
			case AST::NodeKind::Function: {
				func = (AST::FunctionNode*)node.get();
				
				tea::vector<Type*> params;
				for (const auto& [ty, _] : func->params)
					params.emplace(ty);
				Type* ftype = Type::Function(func->returnType, params, func->isVarArg());
				pushsym(func->name, ftype, false, true, false, func->getVisibility() == AST::StorageClass::Public, true);
				visitBlock(func->body);
				func = nullptr;
			} break;

			default:
				#ifdef _DEBUG
				fprintf(stderr, "SemanticAnalyzer: unhandled root statement kind %d\n", node->kind);
				#endif
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
		auto it = scopeHistory.end() - 1;
		while (true) {
			for (auto& sym : *it)
				if (sym.name == name)
					return &sym;

			if (it == scopeHistory.begin()) break;
			--it;
		}
		return nullptr;
	}

	void SemanticAnalyzer::visitBlock(const AST::Tree& tree) {
		pushscope();

		for (const auto& node : tree)
			visitStat(node.get());

		popscope();
	}

	void SemanticAnalyzer::visitStat(const AST::Node* node) {
		switch (node->kind) {
		case AST::NodeKind::Return: {
			Type* returnedType = visitExpression(((AST::ReturnNode*)node)->value.get());
			if (func->returnType != returnedType)
				errors.emplace(std::format("Function '{}': return type mismatch, expected '{}', got '{}'. line {}, column {}",
					func->name, func->returnType->str(), returnedType->str(), node->line, node->column).c_str());
		} break;

		default:
			#ifdef _DEBUG
			fprintf(stderr, "SemanticAnalyzer: unhandled statement kind %d\n", node->kind);
			#endif
			break;
		}
	}

	Type* SemanticAnalyzer::visitExpression(AST::ExpressionNode* node) {
		Type* type = nullptr;
		switch (node->getEKind()) {
		case AST::ExprKind::String:
			type = Type::String();
			break;

		case AST::ExprKind::Char:
			type = Type::Char();
			break;

		case AST::ExprKind::Int:
			type = Type::Int();
			break;

		case AST::ExprKind::Float:
			type = Type::Float();
			break;

		case AST::ExprKind::Double:
			type = Type::Double();
			break;

		case AST::ExprKind::Identf: {
			const tea::string& name = ((AST::LiteralNode*)node)->value;
			const Symbol* sym = lookup(name);
			if (sym)
				type = sym->type;
			else {
				errors.emplace(std::format("Function '{}': use of undefined symbol '{}'. line {}, column {}",
					func->name, name, node->line, node->column).c_str());
				type = Type::Void();
			}
		} break;

		case AST::ExprKind::Call: {
			type = visitExpression(((AST::CallNode*)node)->callee.get());
			if (type->kind != TypeKind::Function)
				errors.emplace(std::format("Function '{}': cannot call a value of type '{}'. line {}, column {}",
					func->name, type->str().data(), node->line, node->column).c_str());
			else
				type = ((FunctionType*)type)->returnType;
		} break;

		default:
			#ifdef _DEBUG
			fprintf(stderr, "SemanticAnalyzer: unhandled expression kind %d\n", node->extra);
			#endif
			return nullptr;
		}
		node->type = type;
		return type;
	}

} // namespace tea::analysis
