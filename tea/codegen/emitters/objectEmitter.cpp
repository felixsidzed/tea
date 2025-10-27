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

		LLVMValueRef dtor = LLVMAddFunction(module, ".dtor`" + node->name, LLVMFunctionType(LLVMVoidType(), &pstruct, 1, false));
		LLVMAppendBasicBlock(dtor, "entry");
		LLVMSetValueName(LLVMGetParam(dtor, 0), "this");

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
			for (const auto& field : node->fields)
				locals.push({
					.type = field->dataType,
					.name = field->name,
					.allocated = LLVMBuildStructGEP(block, LLVMGetParam(func, 0), i++, "")
				});
		};

		auto delMethodContext = [this]() {
			func = nullptr;
			curFunc = nullptr;
			curArgs = nullptr;
			locals.clear();
		};

		block = LLVMCreateBuilder();
		inClassContext = true;
		for (const auto& method : node->methods) {
			if (method->name == ".`ctor") {
				vector<std::pair<Type, string>> args;
				block = LLVMCreateBuilder();
				LLVMPositionBuilderAtEnd(block, LLVMGetLastBasicBlock(ctor));
				createMethodContext(ctor, method.get(), args);
				if (!method->prealloc.empty()) {
					LLVMPositionBuilderAtEnd(block, LLVMGetEntryBasicBlock(func));

					for (auto& [paNode, prealloc] : method->prealloc) {
						if (paNode->dataType.llvm)
							prealloc = LLVMBuildAlloca(block, paNode->dataType.llvm, "prealloc." + paNode->name);
					}

					fnPrealloc = &method->prealloc;
					emitBlock(method->body, "entry", nullptr);
					fnPrealloc = nullptr;
				} else
					emitBlock(method->body, "entry", nullptr);
				delMethodContext();
				if (!LLVMGetBasicBlockTerminator(LLVMGetInsertBlock(block)))
					LLVMBuildRetVoid(block);
			} else if (method->name == ".`dtor") {
				vector<std::pair<Type, string>> args;
				LLVMPositionBuilderAtEnd(block, LLVMGetLastBasicBlock(dtor));
				createMethodContext(dtor, method.get(), args);
				if (!method->prealloc.empty()) {
					LLVMPositionBuilderAtEnd(block, LLVMGetEntryBasicBlock(func));

					for (auto& [paNode, prealloc] : method->prealloc)
						if (paNode->dataType.llvm)
							prealloc = LLVMBuildAlloca(block, paNode->dataType.llvm, "prealloc." + paNode->name);

					fnPrealloc = &method->prealloc;
					emitBlock(method->body, "entry", nullptr);
					fnPrealloc = nullptr;
				}
				else
					emitBlock(method->body, "entry", nullptr);
				delMethodContext();
				if (!LLVMGetBasicBlockTerminator(LLVMGetInsertBlock(block)))
					LLVMBuildRetVoid(block);
			} else {
				vector<LLVMTypeRef> argTypes;
				argTypes.emplace(pstruct);
				for (const auto& arg : method->args)
					argTypes.emplace(arg.first.llvm);
				LLVMValueRef fn = LLVMAddFunction(module, method->name + "`" + node->name, LLVMFunctionType(method->returnType.llvm, argTypes.data, argTypes.size, method->vararg));
				if (method->cc != CC_AUTO)
					LLVMSetFunctionCallConv(func, cc2llvm[method->cc]);

				if (method->storage == STORAGE_PRIVATE)
					LLVMSetLinkage(func, LLVMPrivateLinkage);

				int i = 0;
				for (const auto& arg : method->args)
					LLVMSetValueName(LLVMGetParam(func, i++), arg.second);

				bool noreturn = false;
				for (const auto& attr : method->attrs) {
					if (attr < ATTR__LLVM_COUNT) {
						LLVMAddAttributeAtIndex(func, LLVMAttributeFunctionIndex, LLVMCreateEnumAttribute(LLVMGetGlobalContext(), attr2llvm[attr], 0));
						if (attr == LLVMNoReturnAttributeKind)
							noreturn = true;
					}
					else if (attr == ATTR_NONAMESPACE)
						hasNoNamespaceFunctions = true;
				}

				vector<std::pair<Type, string>> args;
				block = LLVMCreateBuilder();
				LLVMPositionBuilderAtEnd(block, LLVMAppendBasicBlock(fn, "entry"));
				createMethodContext(fn, method.get(), args);
				if (!method->prealloc.empty()) {
					{
						LLVMBasicBlockRef _ = LLVMAppendBasicBlock(func, "entry");
						block = LLVMCreateBuilder();
						LLVMPositionBuilderAtEnd(block, _);
					}

					for (auto& [paNode, prealloc] : method->prealloc) {
						if (paNode->dataType.llvm)
							prealloc = LLVMBuildAlloca(block, paNode->dataType.llvm, "prealloc." + paNode->name);
					}

					fnPrealloc = &method->prealloc;
					emitBlock(method->body, "entry", nullptr);
					fnPrealloc = nullptr;
				}
				else
					emitBlock(method->body, "entry", nullptr);
				delMethodContext();
				if (!LLVMGetBasicBlockTerminator(LLVMGetInsertBlock(block))) {
					if (LLVMGetTypeKind(method->returnType.llvm) != LLVMVoidTypeKind)
						TEA_PANIC("control reaches end of non-void function");
					else {
						if (noreturn) LLVMBuildUnreachable(block);
						else LLVMBuildRetVoid(block);
					}
				}
			}
		}
		LLVMDisposeBuilder(block);
		inClassContext = false;
	}
}
