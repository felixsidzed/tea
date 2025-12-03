#pragma once

#include "common/map.h"
#include "common/Type.h"
#include "frontend/parser/AST.h"

namespace tea::frontend::analysis {

	 class SemanticAnalyzer {
		 struct Symbol {
			 tea::string name;
			 Type* type;

			 bool isConst : 1;
			 bool isMember : 1;
			 bool isPublic : 1;
			 bool isFunction : 1;
			 bool isInitialized : 1;

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
		tea::vector<const char*> importLookup;

		tea::vector<tea::string> visit(const frontend::AST::Tree& root);

	private:
		Symbol* lookup(const tea::string& name);

		void visitVariable(AST::VariableNode* node);
		void visitStat(const frontend::AST::Node* node, bool inLoop = false);
		void visitBlock(const frontend::AST::Tree& tree, bool inLoop = false);

		Type* visitExpression(frontend::AST::ExpressionNode* node, bool isCallee = false);
	};

} // namespace tea::analysis
