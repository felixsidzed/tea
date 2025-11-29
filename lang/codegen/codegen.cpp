#include "codegen.h"

#include "common/tea.h"

namespace tea {

	using namespace frontend;

	std::unique_ptr<mir::Module> CodeGen::emit(const AST::Tree& tree, const Options& options) {
		if (module || builder.getInsertBlock())
			TEA_PANIC("cannot generate MIR while active");

		module = std::make_unique<mir::Module>("[module]");
		module->triple = options.triple;
		module->dl = options.dl;

		for (const auto& node : tree) {
			switch (node->kind) {
			case AST::NodeKind::Function: {
				AST::FunctionNode* func = (AST::FunctionNode*)node.get();

				tea::vector<Type*> params;
				for (const auto& [ty, _] : func->params)
					params.emplace(ty);

				FunctionType* ftype = Type::Function(func->returnType, params, func->isVarArg());

				mir::Function* f = module->addFunction(func->name, ftype);
				f->storage = func->getVisibility();
				f->cc = func->getCC();

				builder.insertInto(f->appendBlock("entry"));
				curParams = &func->params;

				emitBlock(func->body);

				curParams = nullptr;
				builder.insertInto(nullptr);
			} break;

			case AST::NodeKind::FunctionImport:
				emitFunctionImport((const AST::FunctionImportNode*)node.get());
				break;

			case AST::NodeKind::ModuleImport:
				emitModuleImport((const AST::ModuleImportNode*)node.get());
				break;

			default:
				TEA_PANIC("unknown root statement %d", node->kind);
			}
		}

		//builder.insertInto(nullptr);
		return std::move(module);
	}

} // namespace tea
