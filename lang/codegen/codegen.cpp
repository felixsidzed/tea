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

				FunctionType* ftype = Type::Function(func->returnType, params, func->vararg);

				mir::Function* f = module->addFunction(func->name, ftype);
				f->subclassData = func->extra;
				f->storage = func->vis;
				f->cc = func->cc;

				builder.insertInto(f->appendBlock("entry"));
				curParams = &func->params;

				for (const auto& var : func->variables)
					emitVariable(var.get());

				emitBlock(&func->body);

				curParams = nullptr;
				builder.insertInto(nullptr);
			} break;

			case AST::NodeKind::FunctionImport:
				emitFunctionImport((const AST::FunctionImportNode*)node.get());
				break;

			case AST::NodeKind::ModuleImport:
				emitModuleImport((const AST::ModuleImportNode*)node.get());
				break;

			case AST::NodeKind::GlobalVariable: {
				AST::GlobalVariableNode* gv = (AST::GlobalVariableNode*)node.get();

				mir::Value* init = gv->initializer ? emitExpression(gv->initializer.get(), EmissionFlags::Constant) : nullptr;
				if (init && init->kind == mir::ValueKind::Global) {
					init->name = gv->name;
					((mir::Global*)init)->storage = gv->vis;
					((mir::Global*)init)->subclassData = gv->extra;
				}
				else {
					mir::Global* global = module->addGlobal(gv->name, gv->type, init);
					global->storage = gv->vis;
					((mir::Value*)global)->subclassData = gv->extra;
				}
			} break;

			default:
				TEA_PANIC("unknown root statement %d", node->kind);
			}
		}

		//builder.insertInto(nullptr);
		return std::move(module);
	}

} // namespace tea
