#pragma once

#include "core/map.h"
#include "core/Type.h"
#include "core/context.h"
#include "frontend/parser/AST.h"

namespace tea::frontend {

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

		uint32_t fsrc = 0;
		tea::Context& ctx;

		Scope* scope = nullptr;
		tea::vector<Scope> scopeHistory;

		bool isMethodCall = false;
		AST::FunctionNode* func = nullptr;
		tea::umap<tea::StructType*, AST::ObjectNode*> structMap;

	public:
		SemanticAnalyzer(tea::Context& ctx) : ctx(ctx) {}

		void visit(const frontend::AST::Tree& root, uint32_t fsrc);

	private:
		Symbol* lookup(const tea::string& name);

		void visitVariable(AST::VariableNode* node);
		AST::ReturnNode* findFirstReturn(const AST::Tree& tree);
		void visitStat(const frontend::AST::Node* node, bool inLoop = false);
		void visitBlock(const frontend::AST::Tree& tree, bool inLoop = false);

		Type* visitExpression(frontend::AST::ExpressionNode* node, bool isCallee = false);
	};

} // namespace tea::analysis
