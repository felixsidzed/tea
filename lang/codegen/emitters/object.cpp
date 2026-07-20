#include "codegen/codegen.h"

#include "core/tea.h"

namespace tea {

	void CodeGen::emitObject(const AST::ObjectNode* node) {
		structMap[node->type] = node;

		Type* pstruct = ctx.types.Pointer(node->type);
		
		for (const auto& method : node->methods) {
			tea::vector<Type*> argTypes;
			for (const auto& arg : method->params)
				argTypes.emplace(arg.first);

			mir::Function* f = module->addFunction(std::format("{}_{}", node->type->name, method->name).c_str(), ctx.types.Function(method->returnType, argTypes, method->vararg));
			f->cc = method->cc;
			f->storage = method->vis;
				
			builder.block = f->appendBlock("entry");
			curParams = &method->params;

			for (const auto& var : method->variables)
				emitVariable(var.get());

			emitBlock(&method->body);

			if (!builder.block->getTerminator()) {
				if (method->returnType->kind != TypeKind::Void)
					ctx.diag.fatal({ fsrc, method->line, method->column }, 4001, "control reaches end of non-void function '%s'", method->name.data());
				else {
					if (f->hasAttribute(AST::FunctionAttribute::NoReturn)) builder.unreachable();
					else builder.ret(nullptr);
				}
			}

			curParams = nullptr;
			builder.block = nullptr;
		}
	}

} // namespace tea
