#include "Parser.h"

#include "common/map.h"

#define next() (++cur)

#define mknodei(T, i, ...) std::make_unique<T>(__VA_ARGS__, _line##i, _column##i)
#define mknode(T, ...) std::make_unique<T>(__VA_ARGS__, _line, _column)

#define pushtree(T, ...) { \
	auto node = std::make_unique<T>(__VA_ARGS__, _line, _column); \
	auto body = &node->body; \
	treeHistory.emplace(tree); \
	tree->emplace(std::move(node)); \
	tree = body; \
}

#define poptree() { \
	tree = treeHistory[treeHistory.size-1]; \
	treeHistory.pop(); \
}

namespace tea::frontend {

	static inline bool isCC(KeywordKind kind) {
		return kind == KeywordKind::StdCC || kind == KeywordKind::FastCC || kind == KeywordKind::CCC || kind == KeywordKind::AutoCC;
	}

	static const tea::map<tea::string, AST::FunctionAttribute> name2fnAttr = {
		{"inline", AST::FunctionAttribute::Inline},
		{"noreturn", AST::FunctionAttribute::NoReturn},
		{"nonamespace", AST::FunctionAttribute::NoNamespace},
	};

	static const tea::map<tea::string, AST::GlobalAttribute> name2globalAttr = {
		{"threadlocal", AST::GlobalAttribute::ThreadLocal}
	};

	AST::Tree Parser::parse() {
		treeHistory.clear();

		AST::Tree root;
		tree = &root;
		treeHistory.emplace(tree);

		while (!match(TokenKind::EndOfFile)) {
			uint32_t _line = cur->line, _column = cur->column;

			if (cur->kind == TokenKind::Keyword) {
				switch ((KeywordKind)cur->extra) {

				case KeywordKind::Public:
				case KeywordKind::Private: {
					AST::StorageClass vis = (AST::StorageClass)cur->extra;
					next();

					if (cur->kind == TokenKind::Keyword && (isCC((KeywordKind)cur->extra) || (KeywordKind)cur->extra == KeywordKind::Func))
						parseFunc(vis, _line, _column);
					else if (match(KeywordKind::Var)) {
						const tea::string& name = consume(TokenKind::Identf).text;

						Type* type = nullptr;
						if (match(TokenKind::Colon)) {
							const tea::string& typeName = parseType();
							type = Type::get(typeName);
							if (!type)
								TEA_PANIC("undefined type '%s'. line %d, column %d", typeName.data(), cur->line, cur->column);
						}

						std::unique_ptr<AST::ExpressionNode> initializer = nullptr;
						if (match(TokenKind::Assign))
							initializer = parseExpression();
						consume(TokenKind::Semicolon);

						if (!type && !initializer)
							TEA_PANIC("can't deduce a type without an initializer");

						tree->emplace(mknode(AST::GlobalVariableNode, name, type, std::move(initializer), vis));
					} else
						unexpected();
				} break;

				case KeywordKind::Import: {
					next();
				
					if (isCC((KeywordKind)cur->extra) || (KeywordKind)cur->extra == KeywordKind::Func)
						parseFuncImport(_line, _column);
					else
						unexpected();
				} break;

				case KeywordKind::Using: {
					next();

					const tea::string& path = consume(TokenKind::String).text;
					consume(TokenKind::Semicolon);
					tree->emplace(mknode(AST::ModuleImportNode, path));
				} break;

				default:
					unexpected();
					break;
				}

			} else if (cur->kind == TokenKind::At) {
				next();

				uint32_t fnAttr = 0, globalAttr = 0;
				while (true) {
					const tea::string& attrName = consume(TokenKind::Identf).text;

					if (auto it = name2fnAttr.find(attrName))
						fnAttr |= (uint32_t)*it;
					else if (auto it2 = name2globalAttr.find(attrName))
						globalAttr |= (uint32_t)*it2;
					else
						TEA_PANIC("'%s' is not a valid attribute. line %d, column %d", attrName.data(), cur->line, cur->column);

					if ((cur + 1)->kind == TokenKind::Comma) {
						consume(TokenKind::At);
						continue;
					}
					break;
				}

				uint32_t _line = cur->line, _column = cur->column;

				if (cur->kind == TokenKind::Keyword && (cur->extra == (uint32_t)KeywordKind::Public || cur->extra == (uint32_t)KeywordKind::Private)) {
					AST::StorageClass vis = (AST::StorageClass)cur->extra;
					next();

					if (match(KeywordKind::Var)) {
						const tea::string& name = consume(TokenKind::Identf).text;

						Type* type = nullptr;
						if (match(TokenKind::Colon)) {
							const tea::string& typeName = parseType();
							type = Type::get(typeName);
							if (!type)
								TEA_PANIC("undefined type '%s'. line %d, column %d", typeName.data(), cur->line, cur->column);
						}

						std::unique_ptr<AST::ExpressionNode> initializer = nullptr;
						if (match(TokenKind::Assign))
							initializer = parseExpression();
						consume(TokenKind::Semicolon);

						if (!type && !initializer)
							TEA_PANIC("can't deduce a type without an initializer");

						auto node = mknode(AST::GlobalVariableNode, name, type,
										   std::move(initializer), vis);
						node->extra = globalAttr;
						tree->emplace(std::move(node));

					} else if (isCC((KeywordKind)cur->extra) || (KeywordKind)cur->extra == KeywordKind::Func) {
						parseFunc(vis, _line, _column);
						tree->data[tree->size - 1]->extra = fnAttr;

					} else
						unexpected();

				} else if (match(KeywordKind::Import)) {
					parseFuncImport(_line, _column);
					tree->data[tree->size - 1]->extra = fnAttr;

				} else unexpected();
			} else
				unexpected();
		}

		return root;
	}

	void Parser::unexpected() {
		TEA_PANIC("unexpected token '%s'. line %d, column %d", cur->text.data(), cur->line, cur->column);
	}

	bool Parser::match(TokenKind kind) {
		if (cur->kind == kind) {
			next();
			return true;
		}
		return false;
	};
	bool Parser::match(KeywordKind kind) {
		if (cur->kind == TokenKind::Keyword && cur->extra == (uint32_t)kind) {
			next();
			return true;
		}
		return false;
	};

	const Token& Parser::consume(TokenKind kind) {
		if (cur->kind != kind)
			unexpected();
		return *cur++;
	};
	const Token& Parser::consume(KeywordKind kind) {
		if (cur->kind != TokenKind::Keyword || cur->extra != (uint32_t)kind)
			unexpected();
		return *cur++;
	};

	void Parser::parseBlock(const tea::vector<KeywordKind>& extraTerminators) {
		while (true) {
			if (cur->kind == TokenKind::Keyword && (cur->extra == (uint32_t)KeywordKind::End || extraTerminators.find((KeywordKind)cur->extra))) {
				next();
				poptree();
				return;
			}

			if (auto node = parseStat())
				tree->emplace(std::move(node));

			if (match(TokenKind::EndOfFile))
				TEA_PANIC("unexpected EOF (did you forget to close a function?). line %d, column %d", cur->line, cur->column);
		}
	};
	std::unique_ptr<AST::Node> Parser::parseStat() {
		uint32_t _line = cur->line, _column = cur->column;

		switch (cur->kind) {
		case TokenKind::Keyword: {
			switch ((KeywordKind)cur->extra) {
			case KeywordKind::Return: {
				next();
				auto node = mknode(AST::ReturnNode, parseExpression());
				consume(TokenKind::Semicolon);
				return std::move(node);
			} break;

			case KeywordKind::Var:
				func->variables.emplace(parseVariable(_line, _column));
				consume(TokenKind::Semicolon);
				break;

			case KeywordKind::If: {
				next();

				consume(TokenKind::Lpar);
				auto pred = parseExpression();
				consume(TokenKind::Rpar);

				consume(KeywordKind::Do);
				
				auto ifNode = mknode(AST::IfNode, std::move(pred));
				treeHistory.emplace(tree);
				tree = &ifNode->body;

				parseBlock({ KeywordKind::Else, KeywordKind::ElseIf });

				AST::ElseIfNode* lastElseIf = nullptr;
				while ((cur - 1)->kind == TokenKind::Keyword && (cur  - 1)->extra == (uint32_t)KeywordKind::ElseIf) {
					consume(TokenKind::Lpar);
					auto elseIfPred = parseExpression();
					consume(TokenKind::Rpar);

					consume(KeywordKind::Do);

					auto elseIfNode = std::make_unique<AST::ElseIfNode>(std::move(elseIfPred), nullptr, cur->line, cur->column);
					treeHistory.emplace(tree);
					tree = &elseIfNode->body;

					parseBlock({ KeywordKind::Else, KeywordKind::ElseIf });

					if (!lastElseIf) {
						ifNode->elseIf = std::move(elseIfNode);
						lastElseIf = ifNode->elseIf.get();
					} else {
						lastElseIf->next = std::move(elseIfNode);
						lastElseIf = lastElseIf->next.get();
					}
				}

				if ((cur - 1)->kind == TokenKind::Keyword && (cur - 1)->extra == (uint32_t)KeywordKind::Else) {
					auto otherwise = std::make_unique<AST::ElseNode>(_line, _column);
					treeHistory.emplace(tree);
					tree = &otherwise->body;

					parseBlock();

					ifNode->otherwise = std::move(otherwise);
				}

				tree->push(std::move(ifNode));
			} break;

			case KeywordKind::While: {
				next();

				consume(TokenKind::Lpar);
				auto pred = parseExpression();
				consume(TokenKind::Rpar);
				consume(KeywordKind::Do);

				pushtree(AST::WhileLoopNode, std::move(pred));
				parseBlock();
			} break;

			case KeywordKind::Break:
				next();
				consume(TokenKind::Semicolon);
				return std::make_unique<AST::Node>(AST::NodeKind::LoopInterrupt, _line, _column, 0);

			case KeywordKind::Continue:
				next();
				consume(TokenKind::Semicolon);
				return std::make_unique<AST::Node>(AST::NodeKind::LoopInterrupt, _line, _column, 1);

			case KeywordKind::For: {
				next();
				
				std::unique_ptr<AST::VariableNode> var = nullptr;
				std::unique_ptr<AST::ExpressionNode> pred = nullptr;
				std::unique_ptr<AST::ExpressionNode> step = nullptr;

				consume(TokenKind::Lpar);
				if (!match(TokenKind::Semicolon)) {
					var = parseVariable(cur->line, cur->column);
					consume(TokenKind::Semicolon);
				}
				if (!match(TokenKind::Semicolon)) {
					pred = parseExpression();
					consume(TokenKind::Semicolon);
				}
				if (!match(TokenKind::Semicolon))
					step = parseExpression();
				consume(TokenKind::Rpar);
				consume(KeywordKind::Do);

				pushtree(AST::ForLoopNode, std::move(var), std::move(pred), std::move(step));
				parseBlock();
			} break;

			default:
				unexpected();
				TEA_UNREACHABLE();
			}
		} break;

		default: {
			auto node = parseExpression();
			consume(TokenKind::Semicolon);
			return node;
		}
		}
		return nullptr;
	};

	void Parser::parseFunc(AST::StorageClass vis, uint32_t _line, uint32_t _column) {
		AST::CallingConvention cc = AST::CallingConvention::Auto;
		if (cur->extra != (uint32_t)KeywordKind::Func) {
			cc = (AST::CallingConvention)(cur->extra - (uint32_t)KeywordKind::StdCC);
			next();
			consume(KeywordKind::Func);
		}
		else
			next();

		const tea::string& name = consume(TokenKind::Identf).text;
		const auto& [vararg, params] = parseParams();

		consume(TokenKind::Arrow);

		const tea::string& typeName = parseType();
		Type* returnType = Type::get(typeName);
		if (!returnType)
			TEA_PANIC("undefined type '%s'. line %d, column %d", typeName.data(), cur->line, cur->column);

		auto node = std::make_unique<AST::FunctionNode>(vis, cc, name, params, returnType, vararg, _line, _column);
		func = node.get();
		auto body = &node->body;
		treeHistory.emplace(tree);
		tree->emplace(std::move(node));
		tree = body;

		parseBlock();
		func = nullptr;
	}

	void Parser::parseFuncImport(uint32_t _line, uint32_t _column) {
		AST::CallingConvention cc = AST::CallingConvention::Auto;
		if (cur->extra != (uint32_t)KeywordKind::Func) {
			cc = (AST::CallingConvention)(cur->extra - (uint32_t)KeywordKind::StdCC);
			next();
			consume(KeywordKind::Func);
		} else
			next();

		const tea::string& name = consume(TokenKind::Identf).text;
		const auto& [vararg, params] = parseParams();

		consume(TokenKind::Arrow);

		const tea::string& typeName = parseType();
		Type* returnType = Type::get(typeName);
		if (!returnType)
			TEA_PANIC("undefined type '%s'. line %d, column %d", typeName.data(), cur->line, cur->column);

		auto node = mknode(AST::FunctionImportNode, cc, name, params, returnType, vararg);
		consume(TokenKind::Semicolon);
		tree->emplace(std::move(node));
	}

	std::pair<bool, tea::vector<std::pair<Type*, tea::string>>> Parser::parseParams() {
		bool vararg = false;
		tea::vector<std::pair<Type*, tea::string>> result;

		consume(TokenKind::Lpar);
		if (match(TokenKind::Rpar))
			return { vararg, result };

		while (true) {
			if (cur->kind == TokenKind::Dot && (cur + 1)->kind == TokenKind::Dot && (cur + 2)->kind == TokenKind::Dot) {
				cur += 3;
				vararg = true;
				consume(TokenKind::Rpar);
				break;
			}

			const tea::string& typeName = parseType();
			Type* type = Type::get(typeName);
			if (!type)
				TEA_PANIC("undefined type '%s'. line %d, column %d", typeName.data(), cur->line, cur->column);

			result.emplace(type, consume(TokenKind::Identf).text);
			if (match(TokenKind::Rpar))
				break;

			consume(TokenKind::Comma);
		}

		return { vararg, result };
	}

	tea::string Parser::parseType() {
		uint32_t line = cur->line;

		tea::string typeName;
		tea::string first;

		if (cur->kind == TokenKind::Identf || cur->kind == TokenKind::Keyword)
			first = cur->text;
		else
			unexpected();
		next();

		if (first == "const") {
			typeName = "const ";
			tea::string nextToken = consume(TokenKind::Identf).text;
			if (nextToken == "signed" || nextToken == "unsigned") {
				typeName += nextToken + ' ';
				typeName += consume(TokenKind::Identf).text;
			} else
				typeName += nextToken;
		} else if (first == "signed" || first == "unsigned") {
			typeName = first + ' ';
			tea::string nextToken = consume(TokenKind::Identf).text;
			if (nextToken == "const") {
				typeName += "const ";
				typeName += consume(TokenKind::Identf).text;
			} else
				typeName += nextToken;
		} else
			typeName = first;

		while (cur->kind == TokenKind::Star && cur->line == line) {
			typeName += '*';
			next();
			if (cur->kind == TokenKind::Identf && cur->text == "const") {
				typeName += " const";
				next();
			}
		}

		// TODO: array size deduction
		while (cur->kind == TokenKind::Lbrac && cur->line == line) {
			next();

			if (cur->kind != TokenKind::Int)
				unexpected();
			tea::string dim = cur->text;
			next();

			consume(TokenKind::Rbrac);

			typeName += '[';
			typeName += dim;
			typeName += ']';
		}

		while (cur->kind == TokenKind::Lpar) {
			next();

			tea::string funcPart = "(";
			if (cur->kind == TokenKind::Rpar) {
				next();
				funcPart += ")";
				typeName += funcPart;
				continue;
			}

			while (true) {
				if (cur->kind == TokenKind::Dot && (cur + 1)->kind == TokenKind::Dot && (cur + 2)->kind == TokenKind::Dot) {
					cur += 3;
					funcPart += "...";
					next();
				} else
					funcPart += parseType();

				if (cur->kind == TokenKind::Comma) {
					funcPart += ", ";
					next();
					continue;
				}

				break;
			}

			consume(TokenKind::Rpar);
			funcPart += ")";

			typeName += funcPart;
		}

		return typeName;
	}

	std::unique_ptr<AST::ExpressionNode> Parser::parsePrimary(bool allowAssignments) {
		uint32_t _line = cur->line, _column = cur->column;
		std::unique_ptr<AST::ExpressionNode> node = nullptr;

		switch (cur->kind) {
		case TokenKind::Int: {
			node = mknode(AST::LiteralNode, AST::ExprKind::Int, cur->text);
			next();
		} break;
		case TokenKind::Float: {
			node = mknode(AST::LiteralNode, AST::ExprKind::Float, cur->text);
			next();
		} break;
		case TokenKind::Double: {
			node = mknode(AST::LiteralNode, AST::ExprKind::Double, cur->text);
			next();
		} break;
		case TokenKind::String: {
			node = mknode(AST::LiteralNode, AST::ExprKind::String, cur->text);
			next();
		} break;
		case TokenKind::Char: {
			node = mknode(AST::LiteralNode, AST::ExprKind::Char, cur->text);
			next();
		} break;
		case TokenKind::Identf: {
			tea::string text;
			while (cur->kind == TokenKind::Identf || cur->kind == TokenKind::Scope)
				text += (cur++)->text;
			node = mknode(AST::LiteralNode, AST::ExprKind::Identf, text);
		} break;
		case TokenKind::Lpar: {
			next();

			if (cur->kind == TokenKind::Identf) {
				const tea::string& typeName = parseType();
				Type* type = Type::get(typeName);
				if (type) {
					consume(TokenKind::Rpar);
					auto expr = parseExpression();
					node = mknode(AST::UnaryExprNode, AST::ExprKind::Cast, std::move(expr));
					node->type = type;
					break;
				}
			}

			node = parseExpression();
			consume(TokenKind::Rpar);
		} break;
		case TokenKind::Amp: {
			next();
			node = mknode(AST::UnaryExprNode, AST::ExprKind::Ref, parsePrimary());
		} break;
		case TokenKind::Star: {
			next();
			node = mknode(AST::UnaryExprNode, AST::ExprKind::Deref, parseExpression(false));
		} break;
		case TokenKind::Not: {
			next();
			node = mknode(AST::UnaryExprNode, AST::ExprKind::Not, parseExpression());
		} break;
		case TokenKind::Lbrac: {
			next();
			tea::vector<std::unique_ptr<AST::ExpressionNode>> values;
			while (true) {
				values.emplace(parseExpression());
				if (cur->kind == TokenKind::Comma)
					next();
				else if (cur->kind == TokenKind::Rbrac)
					break;
			}
			node = mknode(AST::ArrayNode, std::move(values));
			consume(TokenKind::Rbrac);
		} break;
		default:
			unexpected();
		}

		while (true) {
			uint32_t _line2 = cur->line, _column2 = cur->column;
			if (match(TokenKind::Lpar)) {
				tea::vector<std::unique_ptr<AST::ExpressionNode>> args;
				if (cur->kind != TokenKind::Rpar) {
					while (true) {
						args.push(parseExpression());
						if (cur->kind == TokenKind::Comma) {
							next();
							continue;
						}
						break;
					}
				}

				consume(TokenKind::Rpar);
				node = mknodei(AST::CallNode, 2, std::move(node), std::move(args));
				continue;
			} else if (match(TokenKind::Lbrac)) {
				node = mknodei(AST::BinaryExprNode, 2, AST::ExprKind::Index, std::move(node), parseExpression());
				consume(TokenKind::Rbrac);
				continue;
			} else if (
				allowAssignments &&
				(cur->kind == TokenKind::Assign || (cur->kind >= TokenKind::Star && cur->kind <= TokenKind::Div && (cur + 1)->kind == TokenKind::Assign))
			) {
				if (node->getEKind() != AST::ExprKind::Identf && node->getEKind() != AST::ExprKind::Deref && node->getEKind() != AST::ExprKind::Index)
					break;

				uint32_t extra = 0;
				if (cur->kind != TokenKind::Assign) {
					extra = (uint32_t)cur->kind;
					next();
				}
				next();

				node = mknode(AST::AssignmentNode, std::move(node), parseExpression(), extra);
				continue;
			}
			
			break;
		}

		return node;
	}

	static int getPrecedence(TokenKind kind) {
		switch (kind) {
		case TokenKind::Or:
			return 1;

		case TokenKind::And:
			return 2;

		case TokenKind::Bor:
			return 3;

		case TokenKind::Bxor:
			return 4;

		case TokenKind::Amp:
			return 5;

		case TokenKind::Eq:
		case TokenKind::Neq:
			return 6;

		case TokenKind::Lt:
		case TokenKind::Le:
		case TokenKind::Gt:
		case TokenKind::Ge:
			return 7;

		case TokenKind::Shl:
		case TokenKind::Shr:
			return 8;

		case TokenKind::Add:
		case TokenKind::Sub:
			return 9;

		case TokenKind::Star:
		case TokenKind::Div:
			return 10;

		default:
			return -1;
		}
	}

	static const tea::map<TokenKind, AST::ExprKind> tt2et = {
		{TokenKind::Add, AST::ExprKind::Add},
		{TokenKind::Sub, AST::ExprKind::Sub},
		{TokenKind::Star, AST::ExprKind::Mul},
		{TokenKind::Div, AST::ExprKind::Div},

		{TokenKind::Eq, AST::ExprKind::Eq},
		{TokenKind::Neq, AST::ExprKind::Neq},
		{TokenKind::Lt, AST::ExprKind::Lt},
		{TokenKind::Le, AST::ExprKind::Le},
		{TokenKind::Gt, AST::ExprKind::Gt},
		{TokenKind::Ge, AST::ExprKind::Ge},

		{TokenKind::And, AST::ExprKind::And},
		{TokenKind::Or, AST::ExprKind::Or},

		{TokenKind::Bor, AST::ExprKind::Bor},
		{TokenKind::Bxor, AST::ExprKind::Bxor},
		{TokenKind::Amp, AST::ExprKind::Band},
		{TokenKind::Shl, AST::ExprKind::Shl},
		{TokenKind::Shr, AST::ExprKind::Shr},
	};

	std::unique_ptr<AST::ExpressionNode> Parser::parseRhs(int lhsPrec, std::unique_ptr<AST::ExpressionNode> lhs) {
		while (true) {
			int prec = getPrecedence(cur->kind);
			if (prec < lhsPrec)
				return lhs;
			TokenKind tk = cur->kind;
			next();
			auto rhs = parsePrimary();
			int nextPrec = getPrecedence(cur->kind);
			if (prec < nextPrec)
				rhs = parseRhs(prec + 1, std::move(rhs));
			lhs = std::make_unique<AST::BinaryExprNode>(*tt2et.find(tk), std::move(lhs), std::move(rhs), (cur - 1)->line, (cur - 1)->column);
		}
	}

	std::unique_ptr<AST::ExpressionNode> Parser::parseExpression(bool allowAssignments) {
		auto lhs = parsePrimary(allowAssignments);
		return parseRhs(0, std::move(lhs));
	}

	std::unique_ptr<AST::VariableNode> Parser::parseVariable(uint32_t _line, uint32_t _column) {
		next();
		const tea::string& name = consume(TokenKind::Identf).text;

		Type* type = nullptr;
		if (match(TokenKind::Colon)) {
			const tea::string& typeName = parseType();
			type = Type::get(typeName);
			if (!type)
				TEA_PANIC("undefined type '%s'. line %d, column %d", typeName.data(), cur->line, cur->column);
		}

		std::unique_ptr<AST::ExpressionNode> initializer = nullptr;
		if (match(TokenKind::Assign))
			initializer = parseExpression();

		if (!type && !initializer)
			TEA_PANIC("can't deduce a type without an initializer. line %d, column %d", cur->line, cur->column);

		return mknode(AST::VariableNode, name, type, std::move(initializer));
	}

} // tea::frontend
