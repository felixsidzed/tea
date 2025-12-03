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

				for (const auto& var : func->variables)
					visitVariable(var.get());

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
				const tea::string& moduleName = path.stem().string().c_str();

				try {
					std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());

					const auto& tokens = tea::frontend::lex({ content.data(), content.size() });
					tea::frontend::Parser parser(tokens);
					tea::frontend::AST::Tree tree = parser.parse();

					for (const auto& node_ : tree) {
						if (node_->kind == AST::NodeKind::FunctionImport) {
							AST::FunctionImportNode* fi = (AST::FunctionImportNode*)node_.get();

							tea::vector<Type*> params;
							for (const auto& [ty, _] : fi->params)
								params.emplace(ty);

							Type* ftype = Type::Function(fi->returnType, params, fi->vararg);
							pushsym(fi->hasAttribute(AST::FunctionAttribute::NoNamespace) ? fi->name : moduleName + "::" + fi->name, ftype, false, true, false, true, true);
						} else
							errors.emplace(std::format("Module '{}': invalid root statement. line {}, column {}",
								moduleName,
								node_->line, node_->column
							).c_str());
					}

					file.close();
				} catch (const std::exception& e) {
					errors.emplace(std::format("Failed to import module '{}': {}", moduleName, e.what()).c_str());
				}
			} break;

			case AST::NodeKind::GlobalVariable: {
				AST::GlobalVariableNode* gv = (AST::GlobalVariableNode*)node.get();

				Type* initType = visitExpression(gv->initializer.get());
				if (gv->type) {
					if (gv->type != initType)
						errors.emplace(std::format("Global variable initializer type ({}) doesn't match variable type ({}). line {}, column {}",
							initType->str(), gv->type->str(),
							gv->line, gv->column
						).c_str());
				} else
					gv->type = initType;

				pushsym(gv->name, gv->type, (bool)gv->type->constant, false, false, gv->vis == AST::StorageClass::Public, gv->initializer != nullptr);
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

	void SemanticAnalyzer::visitBlock(const AST::Tree& tree, bool inLoop) {
		for (const auto& node : tree)
			visitStat(node.get(), inLoop);
	}

	void SemanticAnalyzer::visitStat(const AST::Node* node, bool inLoop) {
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

		case AST::NodeKind::If: {
			AST::IfNode* ifNode = (AST::IfNode*)node;
			visitExpression(ifNode->pred.get());
			visitBlock(ifNode->body, inLoop);
			if (ifNode->otherwise)
				visitBlock(ifNode->otherwise->body, inLoop);

			AST::ElseIfNode* elseIf = ifNode->elseIf.get();
			while (elseIf) {
				visitExpression(elseIf->pred.get());
				visitBlock(elseIf->body, inLoop);

				elseIf = elseIf->next.get();
			}
		} break;

		case AST::NodeKind::WhileLoop: {
			AST::WhileLoopNode* loop = (AST::WhileLoopNode*)node;

			visitExpression(loop->pred.get());
			visitBlock(loop->body, true);
		} break;

		case AST::NodeKind::LoopInterrupt:
			if (!inLoop)
				errors.emplace(std::format("Function '{}': cannot '{}' outside of a loop. line {}, column {}",
					func->name,
					node->extra ? "continue" : "break",
					node->line, node->column
				).c_str());
			break;

		case AST::NodeKind::ForLoop: {
			AST::ForLoopNode* loop = (AST::ForLoopNode*)node;
			if (loop->var)
				visitVariable(loop->var.get());
			if (loop->pred)
				visitExpression(loop->pred.get());
			if (loop->step)
				visitExpression(loop->step.get());
			visitBlock(loop->body, true);
		} break;

		default:
			#ifdef _DEBUG
			fprintf(stderr, "SemanticAnalyzer: unhandled statement kind %d\n", node->kind);
			#endif
			break;
		}
	}

	Type* SemanticAnalyzer::visitExpression(AST::ExpressionNode* node, bool isCallee) {
		Type* type = Type::Void();
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
			if (name == "true")
				type = Type::Bool();
			else if (name == "false")
				type = Type::Bool();
			else if (name == "null")
				type = Type::Pointer(Type::Void());
			else {
				const Symbol* sym = lookup(name);
				if (sym) {
					type = sym->type;
					if (!isCallee && type->kind == TypeKind::Function)
						type = Type::Pointer(type);
				} else
					errors.emplace(std::format("Function '{}': use of undefined symbol '{}'. line {}, column {}",
						func->name, name, node->line, node->column
					).c_str());
			}
		} break;

		case AST::ExprKind::Call: {
			AST::CallNode* call = (AST::CallNode*)node;
			type = visitExpression(call->callee.get(), true);

			if (type->kind != TypeKind::Function)
				errors.emplace(std::format("Function '{}': cannot call a value of type '{}'. line {}, column {}",
					func->name, type->str().data(), node->line, node->column
				).c_str());
			else {
				FunctionType* ftype = (FunctionType*)type;
				type = ftype->returnType;

				if ((ftype->extra && ftype->params.size > call->args.size) || (!ftype->extra && ftype->params.size != call->args.size))
					errors.emplace(std::format("Function '{}': argument count mismatch. expected{} {}, got {}. line {}, column {}",
						func->name,
						ftype->extra ? " atleast" : "",
						ftype->params.size, call->args.size,
						node->line, node->column
					).c_str());
				else
					for (uint32_t i = 0; i < ftype->params.size; i++) {
						Type* argType = visitExpression(call->args[i].get());
						Type* paramType = ftype->params[i];

						if (argType != paramType)
							errors.emplace(std::format("Function '{}': argument {}: expected type {}, got {}. line {}, column {}",
								func->name, i, paramType->str(), argType->str(), node->line, node->column
							).c_str());
					}
			}
		} break;

		case AST::ExprKind::Add:
		case AST::ExprKind::Sub:
		case AST::ExprKind::Mul:
		case AST::ExprKind::Div: {
			AST::BinaryExprNode* be = (AST::BinaryExprNode*)node;

			Type* lhsType = visitExpression(be->lhs.get());
			Type* rhsType = visitExpression(be->rhs.get());

			if (lhsType != rhsType)
				errors.emplace(std::format("Function '{}': operator '{}': type mismatch: '{}' vs '{}'. line {}, column {}",
					func->name,
					binExprName[(uint32_t)node->getEKind() - (uint32_t)AST::ExprKind::Add],
					lhsType->str(), rhsType->str(),
					node->line, node->column
				).c_str());
			else if (!lhsType->isNumeric() && !lhsType->isFloat())
				errors.emplace(std::format("Function '{}': operator '{}' cannot be applied to non-numeric type '{}'. line {}, column {}",
					func->name,
					binExprName[(uint32_t)node->getEKind() - (uint32_t)AST::ExprKind::Add],
					lhsType->str(),
					node->line, node->column
				).c_str());
			else
				type = lhsType;
		} break;

		case AST::ExprKind::Eq:
		case AST::ExprKind::Neq:
		case AST::ExprKind::Lt:
		case AST::ExprKind::Gt:
		case AST::ExprKind::Ge:
		case AST::ExprKind::Le:
		case AST::ExprKind::And:
		case AST::ExprKind::Or: {
			AST::BinaryExprNode* be = (AST::BinaryExprNode*)node;

			Type* lhsType = visitExpression(be->lhs.get());
			Type* rhsType = visitExpression(be->rhs.get());

			if (lhsType != rhsType)
				errors.emplace(std::format("Function '{}': operator '{}': type mismatch: '{}' vs '{}'. line {}, column {}",
					func->name,
					binExprName[(uint32_t)node->getEKind() - (uint32_t)AST::ExprKind::Add],
					lhsType->str(), rhsType->str(),
					node->line, node->column
				).c_str());
			else if (!lhsType->isNumeric() && lhsType->kind != TypeKind::Char)
				errors.emplace(std::format("Function '{}': operator '{}' cannot be applied to type '{}'. line {}, column {}",
					func->name,
					binExprName[(uint32_t)node->getEKind() - (uint32_t)AST::ExprKind::Add],
					lhsType->str(),
					node->line, node->column
				).c_str());
			else
				type = Type::Bool();
		} break;

		case AST::ExprKind::Ref:
			type = Type::Pointer(visitExpression(((AST::UnaryExprNode*)node)->value.get()));
			break;

		case AST::ExprKind::Deref: {
			Type* valueType = visitExpression(((AST::UnaryExprNode*)node)->value.get());
			if (valueType->kind != TypeKind::Pointer)
				errors.emplace(std::format("Function '{}': cannot dereference non-pointer type '{}'. line {}, column {}",
					func->name, valueType->str(),
					node->line, node->column
				).c_str());
			else
				type = ((PointerType*)valueType)->pointee;
		} break;

		case AST::ExprKind::Not: {
			Type* valueType = visitExpression(((AST::UnaryExprNode*)node)->value.get());
			if (!valueType->isNumeric() && !valueType->isFloat() && valueType->kind != TypeKind::Pointer)
				errors.emplace(std::format("Function '{}': operator '!' cannot be applied to type '{}'. line {}, column {}",
					func->name, valueType->str(),
					node->line, node->column
				).c_str());
			type = Type::Bool();
		} break;

		// TODO: check for illegal casts
		case AST::ExprKind::Cast:
			type = node->type;
			visitExpression(((AST::UnaryExprNode*)node)->value.get());
			break;

		case AST::ExprKind::Index: {
			AST::BinaryExprNode* be = (AST::BinaryExprNode*)node;

			Type* ltype = visitExpression(be->lhs.get());
			Type* rtype = visitExpression(be->rhs.get());

			if (!ltype->isIndexable())
				errors.emplace(std::format("Function '{}': cannot index a value of type '{}'. line {}, column {}",
					func->name, ltype->str(),
					node->line, node->column
				).c_str());

			if (!rtype->isNumeric())
				errors.emplace(std::format("Function '{}': array subscript is not an integer. line {}, column {}",
					func->name, rtype->str(),
					node->line, node->column
				).c_str());

			type = ltype->getElementType();
		} break;

		case AST::ExprKind::Array: {
			AST::ArrayNode* arr = (AST::ArrayNode*)node;

			if (arr->values.empty())
				errors.emplace(std::format("Function '{}': cannot create an empty array. line {}, column {}", func->name, node->line, node->column).c_str());
			else {
				Type* elementType = nullptr;
				for (const auto& val : arr->values) {
					Type* valType = visitExpression(val.get());
					if (!elementType)
						elementType = valType;

					if (valType != elementType)
						errors.emplace(std::format("Function '{}': array element types do not match. line {}, column {}", func->name, node->line, node->column).c_str());
				}
				type = Type::Array(elementType, arr->values.size);
			}
		} break;

		case AST::ExprKind::Assignment: {
			AST::AssignmentNode* assign = (AST::AssignmentNode*)node;

			type = visitExpression(assign->lhs.get());
			Type* rhsType = visitExpression(assign->rhs.get());
			if (rhsType != type)
				errors.emplace(std::format("Function: '{}': assignment type mismatch: '{}' vs '{}'. line {}, column {}",
					func->name,
					type->str(), rhsType->str(),
					node->line, node->column
				).c_str());
		} break;

		default:
			#ifdef _DEBUG
			fprintf(stderr, "SemanticAnalyzer: unhandled expression kind %d\n", node->extra);
			#endif
			break;
		}
		node->type = type;
		return type;
	}

	void SemanticAnalyzer::visitVariable(AST::VariableNode* node) {
		Type* initType = nullptr;
		if (node->initializer)
			initType = visitExpression(node->initializer.get());

		if (node->type) {
			if (node->type != initType) {
				errors.emplace(std::format(
					"Function '{}': variable initializer type ({}) doesn't match variable type ({}). line {}, column {}",
					func->name,
					initType->str(), node->type->str(),
					node->line, node->column
				).c_str());
			}
		} else {
			if (initType)
				node->type = initType;
			else {
				errors.emplace(std::format(
					"Function '{}': variable '{}' declared without a type or initializer. line {}, column {}",
					func->name, node->name, node->line, node->column
				).c_str());
				return;
			}
		}

		#pragma warning(push)
		#pragma warning(disable : 6011)
		pushsym(node->name, node->type, (bool)node->type->constant, false, false, false, node->type != nullptr);
		#pragma warning(pop)
	}

} // namespace tea::analysis
