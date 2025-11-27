#pragma once

#include "common/map.h"
#include "common/Type.h"
#include "frontend/parser/AST.h"

namespace tea::frontend::analysis {

	 class SemanticAnalyzer {
		 struct Symbol {
			 tea::string name;
			 Type* type;

			 bool isConst;
			 bool isFunction;
			 bool isMember;
			 bool isPublic;
			 bool isInitialized;

			 Symbol(
				 const tea::string& name,
				 Type* type,
				 bool isConst, bool isFunction,
				 bool isMember, bool isPublic,
				 bool isInitialized) :
				 name(name), type(type),
				 isConst(isConst), isFunction(isFunction),
				 isMember(isMember), isPublic(isPublic), isInitialized(isInitialized) {
			 }
		 };

		 typedef tea::vector<Symbol> Scope;

		Scope* scope = nullptr;
		tea::vector<Scope> scopeHistory;

		tea::vector<tea::string> errors;

		AST::FunctionNode* func = nullptr;

	public:
		tea::vector<tea::string> visit(const frontend::AST::Tree& root);

	private:
		Symbol* lookup(const tea::string& name);

		void visitStat(const frontend::AST::Node* node);
		void visitBlock(const frontend::AST::Tree& tree);

		Type* visitExpression(frontend::AST::ExpressionNode* node);
	};

} // namespace tea::analysis
