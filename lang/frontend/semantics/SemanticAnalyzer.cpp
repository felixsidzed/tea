#include "SemanticAnalyzer.h"

#include <span>
#include <fstream>
#include <filesystem>

#include "frontend/parser/Parser.h"

namespace fs = std::filesystem;

#define pushscope() scope = scopeHistory.emplace()
#define popscope() scope = &scopeHistory[scopeHistory.size - 1]; scopeHistory.pop()

#define pushsym(name, type, ...) scope->emplace(name, type, __VA_ARGS__)

namespace tea::frontend {

	static const char* binExprName[] = {
		"+", "-", "*", "/",
		"==", "!=", "<", ">", "<=", ">=",
		"!", "&&", "||"
	};

	static const char* bitwiseExprName[] = {
		"&", "|", "^", ">>", "<<"
	};

	static const char* strtypekind(const TypeKind& kind) {
		switch (kind) {
		case TypeKind::Void: return "void";
		case TypeKind::Bool: return "bool";
		case TypeKind::Char: return "char";
		case TypeKind::Short: return "short";
		case TypeKind::Float: return "float";
		case TypeKind::Int: return "int";
		case TypeKind::Double: return "double";
		case TypeKind::Long: return "long";
		case TypeKind::String: return "string";
		case TypeKind::Pointer: return "pointer";
		case TypeKind::Function: return "function";
		case TypeKind::Array: return "array";
		case TypeKind::Struct: return "struct";
		}
		return nullptr;
	}

	void SemanticAnalyzer::visit(const AST::Tree& root, uint32_t fsrc_) {
		fsrc = fsrc_;
		pushscope();

		for (const auto& node : root) {
			switch (node->kind) {
			case AST::NodeKind::Function: {
				func = (AST::FunctionNode*)node.get();

				// TODO: specify which attribute
				for (uint32_t attr = 0; attr < (uint32_t)AST::FunctionAttribute::_count; attr++) {
					AST::ExpressionList* plist = func->attrParams.find((AST::FunctionAttribute)(1 << attr));
					if (!(func->extra & (1 << attr)))
						continue;

					AST::ExpressionList savemebenjaminnetanyahu;
					if (!plist)
						plist = &savemebenjaminnetanyahu;

					std::initializer_list<TypeKind> sig;
					switch ((AST::FunctionAttribute)(1 << attr)) {
					case AST::FunctionAttribute::Link:
						sig = { TypeKind::String };
					}

					if (sig.size() != plist->size) {
						ctx.diag.fatal({ fsrc, func->line, func->column }, 3002, "argument count mismatch in attribute, expected %u, got %u", sig.size(), plist->size);
						break;
					}

					for (uint32_t i = 0; i < sig.size(); i++) {
						const TypeKind& expected = sig.begin()[i];
						const Type* got = visitExpression(plist->operator[](i).get());
						if (got->kind != expected) {
							ctx.diag.error({ fsrc, func->line, func->column }, 3003,
								"(attribute argument #%u) expected type %s, got %s",
								i + 1, strtypekind(expected), got->str()
							);
							break;
						}
					}
				}
				
				tea::vector<Type*> params;
				for (const auto& [ty, _] : func->params)
					params.emplace(ty);

				pushscope();
				for (const auto& [ty, name] : func->params)
					pushsym(name, ty, false, false, false, false, true);

				for (const auto& var : func->variables)
					visitVariable(var.get());

				Type* retty = func->returnType;
				if (!retty) {
					if (auto* ret = findFirstReturn(func->body))
						retty = visitExpression(ret->value.get());
					else retty = ctx.types.Void();
				}

				Type* ftype = ctx.types.Function(func->returnType, params, func->vararg);
				scopeHistory[scopeHistory.size - 2].emplace(func->name, ftype, false, true, false, func->vis == AST::StorageClass::Public, true);

				visitBlock(func->body);
				popscope();

				func = nullptr;
			} break;

			case AST::NodeKind::FunctionImport: {
				AST::FunctionImportNode* fi = (AST::FunctionImportNode*)node.get();

				tea::vector<Type*> params;
				for (const auto& [ty, _] : fi->params)
					params.emplace(ty);

				Type* ftype = ctx.types.Function(fi->returnType, params, fi->vararg);
				pushsym(fi->name, ftype, false, true, false, false, true);
			} break;

			case AST::NodeKind::GlobalVariable: {
				AST::GlobalVariableNode* gv = (AST::GlobalVariableNode*)node.get();

				Type* initType = visitExpression(gv->initializer.get());
				if (gv->type) {
					if (!gv->type->equals(initType))
						ctx.diag.error({ fsrc, gv->line, gv->column }, 3003,
							"global variable initializer type (%s) doesn't match variable type (%s)",
							initType->str().data(), gv->type->str().data()
						);
				} else
					gv->type = initType;

				pushsym(gv->name, gv->type, (bool)gv->type->constant, false, false, gv->vis == AST::StorageClass::Public, gv->initializer != nullptr);
			} break;

			case AST::NodeKind::Class: {
				AST::ObjectNode* obj = (AST::ObjectNode*)node.get();

				structMap[obj->type] = obj;
			} break;

			case AST::NodeKind::Attribute: {
				AST::AttributeNode* attr = (AST::AttributeNode*)node.get();

				std::initializer_list<TypeKind> sig;
				switch ((AST::GlobalAttribute)node->extra) {
				case AST::GlobalAttribute::Module:
					sig = {TypeKind::String};
				}

				if (sig.size() != attr->params.size) {
					ctx.diag.error({ fsrc, attr->line, attr->column }, 3002, "argument count mismatch, expected %u, got %u", sig.size(), attr->params.size);
					break;
				}

				for (uint32_t i = 0; i < sig.size(); i++) {
					const TypeKind& expected = sig.begin()[i];
					const Type* got = visitExpression(attr->params[i].get());
					if (got->kind != expected) {
						ctx.diag.error({ fsrc, node->line, node->column }, 3003,
							"(argument #%u) expected type %s, got %s",
							i + 1, strtypekind(expected), got->str()
						);
						break;
					}
				}

			} break;

			default:
				ctx.diag.warn({ fsrc, node->line, node->column }, "unhandled root statement kind %d in SemanticAnalyzer\n", node->kind);
				break;
			}
		}

		popscope();
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
			if (!func->returnType)
				func->returnType = returnedType;
			else if (!func->returnType->equals(returnedType))
				ctx.diag.error({ fsrc, node->line, node->column }, 3003,
					"return type mismatch, expected '%s', got '%s'",
					func->returnType->str().data(), returnedType->str().data()
				);
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
				ctx.diag.error({ fsrc, node->line, node->column }, 3005,
					"cannot '%s' outside of a loop",
					node->extra ? "continue" : "break"
				);
			break;

		case AST::NodeKind::ForLoop: {
			AST::ForLoopNode* loop = (AST::ForLoopNode*)node;
			if (loop->var) visitVariable(loop->var.get());
			if (loop->pred) visitExpression(loop->pred.get());
			if (loop->step) visitExpression(loop->step.get());
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
		Type* type = ctx.types.Void();
		switch (node->getEKind()) {
		case AST::ExprKind::Int: type = ctx.types.Int(); break;
		case AST::ExprKind::Char: type = ctx.types.Char(); break;
		case AST::ExprKind::Float: type = ctx.types.Float(); break;
		case AST::ExprKind::String: type = ctx.types.String(); break;
		case AST::ExprKind::Double: type = ctx.types.Double(); break;

		case AST::ExprKind::Identf: {
			const tea::string& name = ((AST::LiteralNode*)node)->value;
			if (name == "true")
				type = ctx.types.Bool();
			else if (name == "false")
				type = ctx.types.Bool();
			else if (name == "null")
				type = ctx.types.Pointer(ctx.types.Void());
			else {
				const Symbol* sym = lookup(name);
				if (sym) {
					type = sym->type;
					if (!isCallee && type->kind == TypeKind::Function)
						type = ctx.types.Pointer(type);
				} else
					ctx.diag.error({ fsrc, node->line, node->column }, 3005, "use of undefined symbol '%s'", name);
			}
		} break;

		case AST::ExprKind::Call: {
			AST::CallNode* call = (AST::CallNode*)node;
			type = visitExpression(call->callee.get(), true);

			if (type->kind != TypeKind::Function)
				ctx.diag.error({ fsrc, call->line, call->column }, 3003,
					"cannot call a value of non-function type '%s'",
					type->str().data()
				);
			else {
				FunctionType* ftype = (FunctionType*)type;
				type = ftype->returnType;

				uint32_t nargs = call->args.size;
				if (isMethodCall)
					nargs++;

				if ((ftype->extra && ftype->params.size > nargs) || (!ftype->extra && ftype->params.size != nargs))
					ctx.diag.error({ fsrc, node->line, node->column }, 3002,
						"argument count mismatch, expected%s %u, got %u",
						ftype->extra ? " atleast" : "",
						ftype->params.size, nargs
					);
				else {
					Type** pparams = ftype->params.data;
					uint32_t nargs = ftype->params.size;
					if (isMethodCall) {
						nargs--;
						pparams++;
					}
					for (uint32_t i = 0; i < nargs; i++) {
						Type* argType = visitExpression(call->args[i].get());
						Type* paramType = pparams[i];

						if (!argType->equals(paramType))
							ctx.diag.error({ fsrc, node->line, node->column }, 3003,
								"(argument #%u) expected type %s, got %s",
								i, paramType->str().data(), argType->str().data()
							);
					}
				}
			}

			isMethodCall = false;
		} break;

		case AST::ExprKind::Add:
		case AST::ExprKind::Sub:
		case AST::ExprKind::Mul:
		case AST::ExprKind::Div: {
			AST::BinaryExprNode* be = (AST::BinaryExprNode*)node;

			Type* lhsType = visitExpression(be->lhs.get());
			Type* rhsType = visitExpression(be->rhs.get());

			if (!lhsType->equals(rhsType))
				ctx.diag.error({ fsrc, node->line, node->column }, 3003,
					"type mismatch ('%s' vs '%s') in operator '%s'",
					lhsType->str().data(), rhsType->str().data(),
					binExprName[(uint32_t)node->getEKind() - (uint32_t)AST::ExprKind::Add]
				);
			else if (!lhsType->isNumeric() && !lhsType->isFloat())
				ctx.diag.error({ fsrc, node->line, node->column }, 3003,
					"operator '%s' cannot be used with non-numeric type '%s'",
					binExprName[(uint32_t)node->getEKind() - (uint32_t)AST::ExprKind::Add],
					lhsType->str().data()
				);
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

			if (!lhsType->equals(rhsType))
				ctx.diag.error({ fsrc, node->line, node->column }, 3003,
					"type mismatch ('%s' vs '%s') in operator '%s'",
					lhsType->str().data(), rhsType->str().data(),
					binExprName[(uint32_t)node->getEKind() - (uint32_t)AST::ExprKind::Add]
				);
			else if (!lhsType->isNumeric())
				ctx.diag.error({ fsrc, node->line, node->column }, 3003,
					"operator '%s' cannot be used with non-numeric type '%s'",
					binExprName[(uint32_t)node->getEKind() - (uint32_t)AST::ExprKind::Add],
					lhsType->str().data()
				);
			else
				type = ctx.types.Bool();
		} break;

		case AST::ExprKind::Ref:
			type = ctx.types.Pointer(visitExpression(((AST::UnaryExprNode*)node)->value.get()));
			break;

		case AST::ExprKind::Deref: {
			Type* valueType = visitExpression(((AST::UnaryExprNode*)node)->value.get());
			if (valueType->kind != TypeKind::Pointer)
				ctx.diag.error({ fsrc, node->line, node->column }, 3003,
					"cannot dereference non-pointer type '%s'",
					valueType->str().data()
				);
			else
				type = ((PointerType*)valueType)->pointee;
		} break;

		case AST::ExprKind::Not: {
			Type* valueType = visitExpression(((AST::UnaryExprNode*)node)->value.get());
			if (!valueType->isNumeric() && !valueType->isFloat() && valueType->kind != TypeKind::Pointer)
				ctx.diag.error({ fsrc, node->line, node->column }, 3003,
					"operator '!' cannot be applied to type '%s'",
					valueType->str().data()
				);
			type = ctx.types.Bool();
		} break;

		case AST::ExprKind::Cast: {
			Type* rtype = visitExpression(((AST::UnaryExprNode*)node)->value.get());
			if (node->type->kind == TypeKind::Struct)
				ctx.diag.error({ fsrc, node->line, node->column }, 3005,
					"cannot cast '%s' to '%s'",
					rtype->str(), node->type->str()
				);
			else
				type = node->type;
		} break;

		case AST::ExprKind::Index: {
			AST::BinaryExprNode* be = (AST::BinaryExprNode*)node;

			Type* ltype = visitExpression(be->lhs.get());
			Type* rtype = visitExpression(be->rhs.get());

			if (!ltype->isIndexable())
				ctx.diag.error({ fsrc, node->line, node->column }, 3003,
					"cannot index a value of type '%s'",
					ltype->str().data()
				);

			if (!rtype->isNumeric())
				ctx.diag.error({ fsrc, node->line, node->column }, 3003,
					"array indicies must be integers",
					rtype->str().data()
				);

			type = ltype->getElementType();
		} break;

		case AST::ExprKind::Array: {
			AST::ArrayNode* arr = (AST::ArrayNode*)node;

			if (arr->values.empty())
				ctx.diag.error({ fsrc, node->line, node->column }, 3005, "cannot create an empty array");
			else {
				Type* elementType = nullptr;
				for (const auto& val : arr->values) {
					Type* valType = visitExpression(val.get());
					if (!elementType)
						elementType = valType;

					if (valType && !valType->equals(elementType))
						ctx.diag.error({ fsrc, node->line, node->column }, 3003, "array element types don't match");
				}
				type = ctx.types.Array(elementType, arr->values.size);
			}
		} break;

		case AST::ExprKind::Assignment: {
			AST::AssignmentNode* assign = (AST::AssignmentNode*)node;

			type = visitExpression(assign->lhs.get());
			Type* rhsType = visitExpression(assign->rhs.get());
			if (!rhsType->equals(type))
				ctx.diag.error({ fsrc, node->line, node->column }, 3003,
					"can't assign a value of type '%s' to '%s'",
					type->str(), rhsType->str()
				);
		} break;

		case AST::ExprKind::Band:
		case AST::ExprKind::Bor:
		case AST::ExprKind::Bxor:
		case AST::ExprKind::Shr:
		case AST::ExprKind::Shl: {
			AST::BinaryExprNode* be = (AST::BinaryExprNode*)node;

			Type* lhsType = visitExpression(be->lhs.get());
			Type* rhsType = visitExpression(be->rhs.get());

			if (!lhsType->equals(rhsType))
				ctx.diag.error({ fsrc, node->line, node->column }, 3003,
					"type mismatch ('%s' vs '%s') in operator '%s'",
					lhsType->str().data(), rhsType->str().data(),
					bitwiseExprName[(uint32_t)node->getEKind() - (uint32_t)AST::ExprKind::Band]
				);
			else if (!lhsType->isNumeric() && !lhsType->isFloat())
				ctx.diag.error({ fsrc, node->line, node->column }, 3003,
					"operator '%s' cannot be used with non-numeric type '%s'",
					binExprName[(uint32_t)node->getEKind() - (uint32_t)AST::ExprKind::Add],
					lhsType->str().data()
				);
			else
				type = lhsType;
		} break;

		case AST::ExprKind::FieldIndex: {
			AST::BinaryExprNode* be = (AST::BinaryExprNode*)node;

			StructType* ltype = (StructType*)visitExpression(be->lhs.get());
			const tea::string& name = ((AST::LiteralNode*)be->rhs.get())->value;

			if (ltype->kind != TypeKind::Struct)
				ctx.diag.error({ fsrc, node->line, node->column }, 3003,
					"cannot index a value of type '%s'",
					ltype->str().data()
				);

			uint32_t i = 0;

			AST::ObjectNode* obj = *structMap.find(ltype);
			for (const auto& field : obj->fields) {
				if (field->name == name) {
					type = ltype->body[i];
					node->type = type;
					return type;
				}
				i++;
			}
			
			i = 0;
			for (const auto& method : obj->methods) {
				if (method->name == name) {
					tea::vector<Type*> params;
					for (const auto& [ty, _] : method->params)
						params.emplace(ty);

					type = ctx.types.Function(method->returnType, params, method->vararg);
					isMethodCall = true;
					node->type = type;
					return type;
				}
				i++;
			}

			ctx.diag.error({ fsrc, node->line, node->column }, 3006,
				"'%s' is not a valid member of '%s'",
				name.data(), ltype->str().data()
			);
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
			if (!node->type->equals(initType)) {
				ctx.diag.error({ fsrc, node->line, node->column }, 3003,
					"variable initializer type (%s) doesn't match variable type (%s)",
					initType->str().data(), node->type->str().data()
				);
			}
		} else {
			if (initType)
				node->type = initType;
			else {
				ctx.diag.error({ fsrc, node->line, node->column }, 3004,
					"variable '%s' declared without a type or initializer",
					node->name.data()
				);
				return;
			}
		}

		pushsym(node->name, node->type, (bool)node->type->constant, false, false, false, node->type != nullptr);
	}

	AST::ReturnNode* SemanticAnalyzer::findFirstReturn(const AST::Tree& tree) {
		for (const auto& node : tree) {
			switch (node->kind) {
			case AST::NodeKind::Return:
				return (AST::ReturnNode*)node.get();

			case AST::NodeKind::If: {
				AST::IfNode* ifNode = (AST::IfNode*)node.get();
				if (auto* r = findFirstReturn(ifNode->body))
					return r;

				AST::ElseIfNode* elseIf = ifNode->elseIf.get();
				while (elseIf) {
					if (auto* r = findFirstReturn(elseIf->body))
						return r;
					elseIf = elseIf->next.get();
				}

				if (ifNode->otherwise)
					if (auto* r = findFirstReturn(ifNode->otherwise->body))
						return r;
			} break;

			case AST::NodeKind::WhileLoop:
				if (auto* r = findFirstReturn(((AST::WhileLoopNode*)node.get())->body))
					return r;
				break;

			case AST::NodeKind::ForLoop:
				if (auto* r = findFirstReturn(((AST::ForLoopNode*)node.get())->body))
					return r;
				break;

			default:
				break;
			}
		}
		return nullptr;
	}

}
