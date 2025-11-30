#include "SemanticAnalyzer.h"

#include <fstream>
#include <filesystem>

#include "mir/Context.h"
#include "frontend/parser/Parser.h"

namespace fs = std::filesystem;

#define pushscope() scope = scopeHistory.emplace()
#define popscope() scope = &scopeHistory[scopeHistory.size - 1]; scopeHistory.pop()

#define pushsym(name, type, ...) scope->emplace(name, type, __VA_ARGS__)

namespace tea::frontend::analysis {

	static const char* binExprName[] = {
		"+", "-", "*", "/",
		"==", "!=", "<", ">", "<=", ">=",
		"!", "&&", "||"
	};

	tea::vector<tea::string> SemanticAnalyzer::visit(const AST::Tree& root) {
		pushscope();

		for (const auto& node : root) {
			switch (node->kind) {
			case AST::NodeKind::Function: {
				func = (AST::FunctionNode*)node.get();
				
				tea::vector<Type*> params;
				for (const auto& [ty, _] : func->params)
					params.emplace(ty);

				Type* ftype = Type::Function(func->returnType, params, func->vararg);
				pushsym(func->name, ftype, false, true, false, func->vis == AST::StorageClass::Public, true);
				
				pushscope();
				for (const auto& [ty, name] : func->params)
					pushsym(name, ty, false, false, false, false, true);

				visitBlock(func->body);
				popscope();

				func = nullptr;
			} break;

			case AST::NodeKind::FunctionImport: {
				AST::FunctionImportNode* fi = (AST::FunctionImportNode*)node.get();

				tea::vector<Type*> params;
				for (const auto& [ty, _] : fi->params)
					params.emplace(ty);

				Type* ftype = Type::Function(fi->returnType, params, fi->vararg);
				pushsym(fi->name, ftype, false, true, false, false, true);
			} break;

			case AST::NodeKind::ModuleImport: {
				AST::ModuleImportNode* mi = (AST::ModuleImportNode*)node.get();
				
				std::string fullModuleName(mi->path + ".itea");

				fs::path path;
				std::ifstream file;
				for (const auto& lookup : importLookup) {
					path = fs::path(lookup) / fullModuleName;
					file.open(path);
					if (file.is_open())
						goto cont;
				}

				if (!file.is_open()) {
					errors.emplace(std::format("Failed to import module '{}': failed to open file", mi->path).c_str());
					break;
				}

			cont:
				std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());

				const auto& tokens = tea::frontend::lex({ content.data(), content.size() });
				tea::frontend::Parser parser(tokens);
				tea::frontend::AST::Tree tree = parser.parse();
				const tea::string& moduleName = path.stem().string().c_str();

				for (const auto& node_ : tree) {
					if (node_->kind == AST::NodeKind::FunctionImport) {
						AST::FunctionImportNode* fi = (AST::FunctionImportNode*)node_.get();

						tea::vector<Type*> params;
						for (const auto& [ty, _] : fi->params)
							params.emplace(ty);

						Type* ftype = Type::Function(fi->returnType, params, fi->vararg);
						pushsym(moduleName + "::" + fi->name, ftype, false, true, false, true, true);
					} else
						TEA_PANIC("invalid root statement. line %d, column %d", node_->line, node_->column);
				}

				file.close();
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

			if (it == scopeHistory.begin())
				break;
			--it;
		}
		return nullptr;
	}

	void SemanticAnalyzer::visitBlock(const AST::Tree& tree) {
		for (const auto& node : tree)
			visitStat(node.get());
	}

	void SemanticAnalyzer::visitStat(const AST::Node* node) {
		switch (node->kind) {
		case AST::NodeKind::Return: {
			Type* returnedType = visitExpression(((AST::ReturnNode*)node)->value.get());
			if (func->returnType != returnedType)
				errors.emplace(std::format("Function '{}': return type mismatch, expected '{}', got '{}'. line {}, column {}",
					func->name, func->returnType->str(), returnedType->str(), node->line, node->column).c_str());
		} break;

		case AST::NodeKind::Expression:
			visitExpression((AST::ExpressionNode*)node);
			break;

		case AST::NodeKind::Variable:
			visitVariable((AST::VariableNode*)node);
			break;

		case AST::NodeKind::If: {
			AST::IfNode* ifNode = (AST::IfNode*)node;
			visitExpression(ifNode->pred.get());
			visitBlock(ifNode->body);
			if (ifNode->otherwise)
				visitBlock(ifNode->otherwise->body);

			AST::ElseIfNode* elseIf = ifNode->elseIf.get();
			while (elseIf) {
				visitExpression(elseIf->pred.get());
				visitBlock(elseIf->body);

				elseIf = elseIf->next.get();
			}
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
			AST::CallNode* call = (AST::CallNode*)node;
			type = visitExpression(call->callee.get());
			if (type->kind != TypeKind::Function)
				errors.emplace(std::format("Function '{}': cannot call a value of type '{}'. line {}, column {}",
					func->name, type->str().data(), node->line, node->column).c_str());
			else {
				FunctionType* ftype = (FunctionType*)type;
				type = ftype->returnType;

				uint32_t i = 0;
				for (const auto& arg : call->args) {
					Type* argType = visitExpression(arg.get());
					Type* paramType = ftype->params[i];

					if (argType != paramType)
						errors.emplace(std::format("Function '{}': argument {}: expected type {}, got {}. line {}, column {}",
							func->name, i, paramType->str(), argType->str(), node->line, node->column).c_str());
					i++;
				}
			}
		} break;

		case AST::ExprKind::Add:
		case AST::ExprKind::Sub:
		case AST::ExprKind::Mul:
		case AST::ExprKind::Div: {
			AST::BinaryExpr* be = (AST::BinaryExpr*)node;

			Type* lhsType = visitExpression(be->lhs.get());
			Type* rhsType = visitExpression(be->rhs.get());

			if (lhsType != rhsType) {
				errors.emplace(std::format("Function '{}': operator '{}': type mismatch: '{}' vs '{}'. line {}, column {}",
					func->name,
					binExprName[(uint32_t)node->getEKind() - (uint32_t)AST::ExprKind::Add],
					lhsType->str(), rhsType->str(),
					node->line, node->column
				).c_str());
				type = Type::Void();
			} else if (!lhsType->isNumeric() && !lhsType->isFloat()) {
				errors.emplace(std::format("Function '{}': operator '{}' cannot be applied to non-numeric type '{}'. line {}, column {}",
					func->name,
					binExprName[(uint32_t)node->getEKind() - (uint32_t)AST::ExprKind::Add],
					lhsType->str(),
					node->line, node->column
				).c_str());
				type = Type::Void();
			} else
				type = lhsType;
		} break;

		case AST::ExprKind::Eq:
		case AST::ExprKind::Neq:
		case AST::ExprKind::Lt:
		case AST::ExprKind::Gt:
		case AST::ExprKind::Ge:
		case AST::ExprKind::Le: {
			AST::BinaryExpr* be = (AST::BinaryExpr*)node;

			Type* lhsType = visitExpression(be->lhs.get());
			Type* rhsType = visitExpression(be->rhs.get());

			if (lhsType != rhsType) {
				errors.emplace(std::format(
					"Function '{}': operator '{}': type mismatch: '{}' vs '{}'. line {}, column {}",
					func->name,
					binExprName[(uint32_t)node->getEKind() - (uint32_t)AST::ExprKind::Add],
					lhsType->str(), rhsType->str(),
					node->line, node->column
				).c_str());
				type = Type::Void();
			} else if (!lhsType->isNumeric() && lhsType->kind != TypeKind::Char && lhsType->kind != TypeKind::String) {
				errors.emplace(std::format(
					"Function '{}': operator '{}' cannot be applied to type '{}'. line {}, column {}",
					func->name,
					binExprName[(uint32_t)node->getEKind() - (uint32_t)AST::ExprKind::Add],
					lhsType->str(),
					node->line, node->column
				).c_str());
				type = Type::Void();
			} else
				type = Type::Bool();
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

	void SemanticAnalyzer::visitVariable(AST::VariableNode* node) {
		Type* initType = visitExpression(node->initializer.get());
		if (node->type) {
			if (node->type != initType)
				TEA_PANIC("variable initializer type (%s) doesn't match variable type (%s). line %d, column %d",
					initType->str().data(), node->type->str().data(), node->line, node->column);
		} else
			node->type = initType;

		#pragma warning(push)
		#pragma warning(disable : 6011)
		pushsym(node->name, node->type, (bool)node->type->constant, false, false, false, node->type != nullptr);
		#pragma warning(pop)
	}

} // namespace tea::analysis
