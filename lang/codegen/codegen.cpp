#include "codegen.h"

#include "core/tea.h"

namespace tea {

	using namespace frontend;

	std::unique_ptr<mir::Module> CodeGen::emit(uint32_t fsrc_, const AST::Tree& tree, const Options& options) {
		if (module || builder.block)
			ctx.diag.fatal(TEA_NO_SOURCELOC, 4000, "code generation attempted to invoke itself recursively");
		fsrc = fsrc_;

		module = std::make_unique<mir::Module>(ctx, "[module]");
		module->triple = options.triple;
		module->dl = options.dl;

		for (const auto& node : tree) {
			switch (node->kind) {
			case AST::NodeKind::Function: {
				AST::FunctionNode* func = (AST::FunctionNode*)node.get();

				tea::vector<Type*> params;
				for (const auto& [ty, _] : func->params)
					params.emplace(ty);

				FunctionType* ftype = ctx.types.Function(func->returnType, params, func->vararg);

				mir::Function* f = module->addFunction(func->name, ftype);
				f->subclassData = func->extra;
				f->storage = func->vis;
				f->cc = func->cc;

				if (func->hasAttribute(AST::FunctionAttribute::Link))
					f->linkName = ((AST::LiteralNode*)(*func->getAttrParams(AST::FunctionAttribute::Link)->data).get())->value;

				builder.block = f->appendBlock("entry");
				curParams = &func->params;

				for (const auto& var : func->variables)
					emitVariable(var.get());

				emitBlock(&func->body);

				if (!builder.block->getTerminator()) {
					if (func->returnType->kind != TypeKind::Void)
						ctx.diag.fatal({ fsrc, func->line, func->column }, 4001, "control reaches end of non-void function '%s'", func->name.data());
					else {
						if (func->hasAttribute(AST::FunctionAttribute::NoReturn)) builder.unreachable();
						else builder.ret(nullptr);
					}
				}

				curParams = nullptr;
				builder.block = nullptr;
			} break;

			case AST::NodeKind::FunctionImport:
				emitFunctionImport((const AST::FunctionImportNode*)node.get());
				break;

			case AST::NodeKind::GlobalVariable: {
				AST::GlobalVariableNode* gv = (AST::GlobalVariableNode*)node.get();

				mir::Value* init = gv->initializer ? emitExpression(gv->initializer.get(), EmissionFlags::Constant) : nullptr;
				if (init && init->kind == mir::ValueKind::Global) {
					init->name = gv->name;
					((mir::Global*)init)->storage = gv->vis;
					((mir::Global*)init)->subclassData = gv->extra;
				} else {
					mir::Global* global = module->addGlobal(gv->name, gv->type, init);
					global->storage = gv->vis;
					((mir::Value*)global)->subclassData = gv->extra;
				}
			} break;

			case AST::NodeKind::Class:
				emitObject((const AST::ObjectNode*)node.get());
				break;

			case AST::NodeKind::Attribute: {
				const AST::AttributeNode* attr = (const AST::AttributeNode*)node.get();
				switch ((AST::GlobalAttribute)attr->extra) {
				case AST::GlobalAttribute::Module:
					if (attr->params.size != 1) {
						ctx.diag.fatal({ fsrc, node->line, node->column }, 2003, "too much or not enough parameters in @!module attribute");
						break;
					}

					if (attr->params[0]->getEKind() != AST::ExprKind::String) {
						ctx.diag.fatal({ fsrc, node->line, node->column }, 2003, "the first parameter must be a constant string");
						break;
					}

					curModuleName = ((AST::LiteralNode*)attr->params[0].get())->value.data();
					break;

				default:
					ctx.diag.fatal({ fsrc, node->line, node->column }, 4002, "unknown root statement %d", node->kind);
				}
			} break;

			default:
				ctx.diag.fatal({ fsrc, node->line, node->column }, 4002, "unknown root statement %d", node->kind);
			}
		}

		if (curModuleName) {
			for (const auto& g : module->body) {
				if (g->kind == mir::ValueKind::Function) {
					mir::Function* f = (mir::Function*)g.get();
					if (f->storage != mir::StorageClass::Public || f->linkName)
						continue;

					// pmo
					f->linkName = module->scope.add(std::format("{}_{}", curModuleName, f->name).c_str());
				}
			}
		}

		builder.block = nullptr;
		return std::move(module);
	}

} // namespace tea
