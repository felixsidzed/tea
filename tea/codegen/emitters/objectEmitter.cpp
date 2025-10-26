#include "../codegen.h"

#include <functional>

#include "tea/tea.h"

#include <llvm-c/Target.h>

namespace tea {
	extern LLVMCallConv cc2llvm[CC__COUNT];
	extern LLVMAttributeKind attr2llvm[ATTR__LLVM_COUNT];

	void CodeGen::emitObject(ObjectNode* node) {
		LLVMTypeRef objType = LLVMGetTypeByName(module, node->name.data);
		{
			vector<LLVMTypeRef> elements;
			for (const auto& field : node->fields)
				elements.push(field->dataType.llvm);
			LLVMStructSetBody(objType, elements.data, elements.size, false);
		}
		objects[objType] = node;

		LLVMTypeRef pstruct = LLVMPointerType(objType, 0);
		uint64_t size = LLVMStoreSizeOfType(LLVMGetModuleDataLayout(module), objType);

		bool ctorVarArg = false;
		vector<LLVMTypeRef> ctorArgTypes;
		ctorArgTypes.emplace(pstruct);
		{
			for (const auto& method : node->methods) {
				if (method->name == ".`ctor") {
					for (const auto& arg : method->args)
						ctorArgTypes.emplace(arg.first.llvm);
					ctorVarArg = method->vararg;
					break;
				}
			}
		}
		LLVMValueRef ctor = LLVMAddFunction(module, ".ctor`" + node->name, LLVMFunctionType(LLVMVoidType(), ctorArgTypes.data, ctorArgTypes.size, ctorVarArg));
		LLVMAppendBasicBlock(ctor, "entry");
		LLVMSetValueName(LLVMGetParam(ctor, 0), "this");

		auto createMethodContext = [this, node, pstruct](LLVMValueRef func, FunctionNode* method, vector<std::pair<Type, string>>& args) {
			if (!block || LLVMGetBasicBlockParent(LLVMGetInsertBlock(block)) != func)
				LLVMPositionBuilderAtEnd(block, LLVMAppendBasicBlock(func, "entry"));

			this->func = func;
			curFunc = method;
			args.emplace(pstruct, "this");
			for (const auto& arg : method->args)
				args.emplace(arg);
			curArgs = &args;

			locals.clear();
			int i = 0;
			for (const auto& field : node->fields) {
				locals.push({
					.type = field->dataType,
					.name = field->name,
					.allocated = LLVMBuildStructGEP(block, LLVMGetParam(func, 0), i++, "")
				});
			}
		};

		auto delMethodContext = [this]() {
			func = nullptr;
			curFunc = nullptr;
			curArgs = nullptr;
			locals.clear();
		};
		
		for (const auto& method : node->methods) {
			if (method->name == ".`ctor") {
				vector<std::pair<Type, string>> args;
				block = LLVMCreateBuilder();
				LLVMPositionBuilderAtEnd(block, LLVMGetLastBasicBlock(ctor));
				createMethodContext(ctor, method.get(), args);
				emitBlock(method->body, method->name.data, nullptr);
				delMethodContext();
				if (!LLVMGetBasicBlockTerminator(LLVMGetInsertBlock(block)))
					LLVMBuildRetVoid(block);
				LLVMDisposeBuilder(block);
			}
		}
	}
}
