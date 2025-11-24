#include "mir.h"

#include "frontend/parser/AST.h"

namespace tea::mir {

	Instruction* Builder::ret(Value* val) {
		Instruction* insn = block->body.emplace();
		insn->op = OpCode::Ret;
		if (val)
			insn->operands.push(val);
		return insn;
	}

	Value* Builder::alloca_(Type* type, const char* name) {
		Instruction* insn = block->body.emplace();
		insn->op = OpCode::Alloca;

		insn->result.type = tea::Type::Pointer(type);
		insn->result.kind = ValueKind::Instruction;
		insn->result.name = block->scope.add(name);

		return &insn->result;
	}

	Instruction* Builder::store(Value* ptr, Value* val) {
		Instruction* insn = block->body.emplace();
		insn->op = OpCode::Store;
		insn->operands.emplace(ptr);
		insn->operands.emplace(val);

		return insn;
	}

	Value* Builder::load(Value* ptr, const char* name) {
		Instruction* insn = block->body.emplace();
		insn->op = OpCode::Load;
		insn->operands.emplace(ptr);

		insn->result.type = ((PointerType*)ptr->type)->pointee;
		insn->result.kind = ValueKind::Instruction;
		insn->result.name = block->scope.add(name);

		return &insn->result;
	}

	Value* Builder::cast(Value* ptr, Type* targetType, const char* name) {
		if (ptr->type->kind != TypeKind::Pointer || targetType->kind != TypeKind::Pointer)
			return nullptr;
		if (((PointerType*)ptr->type)->pointee == targetType)
			return ptr;

		Instruction* insn = block->body.emplace();
		insn->op = OpCode::Cast;
		insn->operands.emplace(ptr);

		insn->result.type = targetType;
		insn->result.kind = ValueKind::Instruction;
		insn->result.name = block->scope.add(name);

		return &insn->result;
	}

	Value* Builder::globalString(const tea::string& val) {
		ConstantString* str = ConstantString::get(val);
		return cast(block->parent->parent->addGlobal("", str->type, str), Type::Pointer(Type::Char(true)), "");
	}

	Value* Builder::binop(OpCode op, Value* lhs, Value* rhs, const char* name) {
		if (op < OpCode::Not || op > OpCode::Shr || lhs->type != rhs->type)
			return nullptr;

		Instruction* insn = block->body.emplace();
		insn->op = op;
		insn->operands.emplace(lhs);
		insn->operands.emplace(rhs);

		insn->result.type = lhs->type;
		insn->result.kind = ValueKind::Instruction;
		insn->result.name = block->scope.add(name);

		return &insn->result;
	}

	Value* Builder::arithm(OpCode op, Value* lhs, Value* rhs, const char* name) {
		if (op < OpCode::Add || op > OpCode::Mod || lhs->type != rhs->type)
			return nullptr;

		Instruction* insn = block->body.emplace();
		insn->op = op;
		insn->operands.emplace(lhs);
		insn->operands.emplace(rhs);

		insn->result.type = lhs->type;
		insn->result.kind = ValueKind::Instruction;
		insn->result.name = block->scope.add(name);

		return &insn->result;
	}

	Instruction* Builder::unreachable() {
		Instruction* insn = block->body.emplace();
		insn->op = OpCode::Unreachable;

		return insn;
	}

	Value* Builder::gep(Value* ptr, Value* idx, const char* name) {
		return gep(ptr, &idx, 1, name);
	}

	Value* Builder::gep(Value* ptr, Value** indicies, uint32_t n, const char* name) {
		if (!ptr->type->isIndexable() || !indicies || !n)
			return nullptr;

		Instruction* insn = block->body.emplace();
		insn->op = OpCode::GetElementPtr;
		insn->operands.emplace(ptr);

		Type* ty = ptr->type;
		for (uint32_t i = 0; i < n; i++) {
			Value* idx = indicies[i];
			if (!idx->type->isNumeric())
				return nullptr;

			insn->operands.emplace(idx);
			ty = ty->getElementType();
		}

		insn->result.type = ty;
		insn->result.kind = ValueKind::Instruction;
		insn->result.name = block->scope.add(name);

		return &insn->result;
	}

	Instruction* Builder::br(BasicBlock* b, bool change) {
		Instruction* insn = block->body.emplace();
		insn->op = OpCode::Br;
		insn->operands.emplace((Value*)b);

		if (change)
			block = b;

		return insn;
	}

	Value* Builder::call(Value* func, Value* arg, const char* name) {
		return call(func, &arg, 1, name);
	}

	Value* Builder::call(Value* func, Value** args, uint32_t n, const char* name) {
		Instruction* insn = block->body.emplace();
		insn->op = OpCode::Call;

		insn->operands.emplace(func);
		for (uint32_t i = 0; i < n; i++)
			insn->operands.emplace(args[i]);

		Type* retType = ((FunctionType*)func->type)->returnType;
		if (retType->kind != TypeKind::Void) {
			insn->result.type = retType;
			insn->result.kind = ValueKind::Instruction;
			insn->result.name = name;
		}

		return &insn->result;
	}

} // namespace tea::mir
