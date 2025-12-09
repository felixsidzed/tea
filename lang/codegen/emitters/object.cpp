#include "codegen/codegen.h"

#include "common/tea.h"

namespace tea {

	void CodeGen::emitObject(const AST::ObjectNode* node) {
		structMap[node->type] = node;

		Type* pstruct = Type::Pointer(node->type);
		
		for (const auto& method : node->methods) {
			tea::vector<Type*> argTypes;
			for (const auto& arg : method->params)
				argTypes.emplace(arg.first);

			mir::Function* f = module->addFunction(std::format("_{}__{}", node->type->name, method->name).c_str(), Type::Function(method->returnType, argTypes, method->vararg));
			f->cc = method->cc;
			f->storage = method->vis;
				
			builder.insertInto(f->appendBlock("entry"));
			curParams = &method->params;

			for (const auto& var : method->variables)
				emitVariable(var.get());

			emitBlock(&method->body);

			if (!builder.getInsertBlock()->getTerminator()) {
				if (method->returnType->kind != TypeKind::Void)
					TEA_PANIC("control reaches end of non-void function '%s.%s'. line %d, column %d", node->type->name, method->name.data(), method->line, method->column);
				else {
					if (f->hasAttribute(AST::FunctionAttribute::NoReturn)) builder.unreachable();
					else builder.ret(nullptr);
				}
			}

			curParams = nullptr;
			builder.insertInto(nullptr);
		}
	}

} // namespace tea
