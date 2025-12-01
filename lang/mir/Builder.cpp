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

		insn->result = std::make_unique<Value>(ValueKind::Instruction, tea::Type::Pointer(type));
		insn->result->name = block->scope.add(name);

		return insn->result.get();
	}

	Instruction* Builder::store(Value* ptr, Value* val, bool volat) {
		Instruction* insn = block->body.emplace();
		insn->op = OpCode::Store;
		insn->operands.emplace(ptr);
		insn->operands.emplace(val);

		insn->extra = volat;

		return insn;
	}

	Value* Builder::load(Value* ptr, const char* name, bool volat) {
		Instruction* insn = block->body.emplace();
		insn->op = OpCode::Load;
		insn->operands.emplace(ptr);

		insn->extra = volat;

		insn->result = std::make_unique<Value>(ValueKind::Instruction, ((PointerType*)ptr->type)->pointee);
		insn->result->name = block->scope.add(name);

		return insn->result.get();
	}

	Value* Builder::cast(Value* val, Type* targetType, const char* name) {
		if (val->type == targetType)
			return val;

		Instruction* insn = block->body.emplace();
		insn->op = OpCode::Cast;
		insn->operands.emplace(val);

		insn->result = std::make_unique<Value>(ValueKind::Instruction, targetType);
		insn->result->name = block->scope.add(name);
		insn->result->type = targetType;

		return insn->result.get();
	}

	Value* Builder::globalString(const tea::string& val) {
		ConstantString* str = ConstantString::get(val);
		return cast(block->parent->parent->addGlobal("", str->type, str), Type::Pointer(Type::Char(true)), "");
	}

	Value* Builder::binop(OpCode op, Value* lhs, Value* rhs, const char* name) {
		if (op < OpCode::Not || op > OpCode::Shr || (rhs && lhs->type != rhs->type))
			return nullptr;

		Instruction* insn = block->body.emplace();
		insn->op = op;
		insn->operands.emplace(lhs);
		insn->operands.emplace(rhs);

		insn->result = std::make_unique<Value>(ValueKind::Instruction, lhs->type);
		insn->result->name = block->scope.add(name);

		return insn->result.get();
	}

	Value* Builder::arithm(OpCode op, Value* lhs, Value* rhs, const char* name) {
		if (op < OpCode::Add || op > OpCode::Mod || lhs->type != rhs->type)
			return nullptr;

		// TODO fuckass
		if (lhs->kind == ValueKind::Constant && rhs->kind == ValueKind::Constant) {
			uint8_t width = ((ConstantNumber*)lhs)->getBitwidth();
			if (lhs->type->isNumeric() && rhs->type->isNumeric()) {
				uint64_t lnum = ((ConstantNumber*)lhs)->getInteger();
				uint64_t rnum = ((ConstantNumber*)rhs)->getInteger();
				switch (op) {
				case OpCode::Add: return ConstantNumber::get(lnum + rnum, width, lhs->type->sign);
				case OpCode::Sub: return ConstantNumber::get(lnum - rnum, width, lhs->type->sign);
				case OpCode::Mul: return ConstantNumber::get(lnum * rnum, width, lhs->type->sign);
				case OpCode::Div: return ConstantNumber::get(lnum / rnum, width, lhs->type->sign);
				case OpCode::Mod: return ConstantNumber::get(lnum % rnum, width, lhs->type->sign);
				default: __assume(0);
				}
			} else if (lhs->type->isFloat() && rhs->type->isFloat()) {
				double lnum = ((ConstantNumber*)lhs)->getDouble();
				double rnum = ((ConstantNumber*)rhs)->getDouble();
				switch (op) {
				case OpCode::Add: return ConstantNumber::get<double>(lnum + rnum, width, lhs->type->sign);
				case OpCode::Sub: return ConstantNumber::get<double>(lnum - rnum, width, lhs->type->sign);
				case OpCode::Mul: return ConstantNumber::get<double>(lnum * rnum, width, lhs->type->sign);
				case OpCode::Div: return ConstantNumber::get<double>(lnum / rnum, width, lhs->type->sign);
				case OpCode::Mod: return ConstantNumber::get<double>(fmod(lnum, rnum), width, lhs->type->sign);
				default: __assume(0);
				}
			}
			return nullptr;
		} else {
			Instruction* insn = block->body.emplace();
			insn->op = op;
			insn->operands.emplace(lhs);
			insn->operands.emplace(rhs);

			insn->result = std::make_unique<Value>(ValueKind::Instruction, lhs->type);
			insn->result->name = block->scope.add(name);

			return insn->result.get();
		}
	}

	Instruction* Builder::unreachable() {
		Instruction* insn = block->body.emplace();
		insn->op = OpCode::Unreachable;

		return insn;
	}

	Value* Builder::gep(Value* ptr, Value* idx, const char* name) {
		if (
			idx->kind == ValueKind::Constant &&
			idx->type->isNumeric() &&
			ptr->type->kind == TypeKind::Pointer &&
			((PointerType*)ptr->type)->pointee->kind == TypeKind::Struct
		) {
			Instruction* insn = block->body.emplace();
			insn->op = OpCode::GetElementPtr;
			insn->operands.emplace(ptr);
			insn->operands.emplace(ConstantNumber::get(0, 32));
			insn->operands.emplace(idx);

			StructType* st = (StructType*)(((PointerType*)ptr->type)->pointee);

			insn->result = std::make_unique<Value>(ValueKind::Instruction, Type::Pointer(st->body[(uint32_t)((ConstantNumber*)idx)->getInteger()], false));
			insn->result->name = block->scope.add(name);

			return insn->result.get();
		}

		return gep(ptr, &idx, 1, name);
	}

	Value* Builder::gep(Value* ptr, Value** indicies, uint32_t n, const char* name) {
		if (!ptr->type->isIndexable() || !indicies || !n || !name)
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

		insn->result = std::make_unique<Value>(ValueKind::Instruction, ty);
		insn->result->name = block->scope.add(name);

		return insn->result.get();
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
			insn->result = std::make_unique<Value>(ValueKind::Instruction, retType);
			insn->result->name = block->scope.add(name);
		}

		return insn->result.get();
	}

	Value* Builder::icmp(ICmpPredicate pred, Value* lhs, Value* rhs, const char* name) {
		if (!lhs->type->isNumeric() || !rhs->type->isNumeric())
			return nullptr;

		if (lhs->kind == ValueKind::Constant && rhs->kind == ValueKind::Constant) {
			if (lhs->type->sign) {
				int64_t lnum = ((ConstantNumber*)lhs)->getSInteger();
				int64_t rnum = ((ConstantNumber*)rhs)->getSInteger();
				switch (pred) {
				case ICmpPredicate::EQ: return ConstantNumber::get(lnum == rnum, 1);
				case ICmpPredicate::NEQ: return ConstantNumber::get(lnum != rnum, 1);
				case ICmpPredicate::SGT: return ConstantNumber::get(lnum > rnum, 1);
				case ICmpPredicate::SGE: return ConstantNumber::get(lnum >= rnum, 1);
				case ICmpPredicate::SLT: return ConstantNumber::get(lnum < rnum, 1);
				case ICmpPredicate::SLE: return ConstantNumber::get(lnum <= rnum, 1);
				default: break;
				}
			}

			uint64_t lnum = ((ConstantNumber*)lhs)->getInteger();
			uint64_t rnum = ((ConstantNumber*)rhs)->getInteger();
			switch (pred) {
			case ICmpPredicate::EQ: return ConstantNumber::get(lnum == rnum, 1);
			case ICmpPredicate::NEQ: return ConstantNumber::get(lnum != rnum, 1);
			case ICmpPredicate::UGT: return ConstantNumber::get(lnum > rnum, 1);
			case ICmpPredicate::UGE: return ConstantNumber::get(lnum >= rnum, 1);
			case ICmpPredicate::ULT: return ConstantNumber::get(lnum < rnum, 1);
			case ICmpPredicate::ULE: return ConstantNumber::get(lnum <= rnum, 1);
			default: return nullptr;
			}
		} else {
			Instruction* insn = block->body.emplace();
			insn->op = OpCode::ICmp;
			insn->operands.emplace(lhs);
			insn->operands.emplace(rhs);
			insn->extra = (uint32_t)pred;

			insn->result = std::make_unique<Value>(ValueKind::Instruction, Type::Bool());
			insn->result->name = block->scope.add(name);

			return insn->result.get();
		}
		return nullptr;
	}

	Value* Builder::fcmp(FCmpPredicate pred, Value* lhs, Value* rhs, const char* name) {
		if (!lhs->type->isFloat() || !rhs->type->isFloat())
			return nullptr;

		if (lhs->kind == ValueKind::Constant && rhs->kind == ValueKind::Constant) {
			double lnum = ((ConstantNumber*)lhs)->getDouble();
			double rnum = ((ConstantNumber*)rhs)->getDouble();
			switch (pred) {
			case FCmpPredicate::OEQ: return ConstantNumber::get(lnum == rnum, 1);
			case FCmpPredicate::ONEQ: return ConstantNumber::get(lnum != rnum, 1);
			case FCmpPredicate::OGT: return ConstantNumber::get(lnum > rnum, 1);
			case FCmpPredicate::OGE: return ConstantNumber::get(lnum >= rnum, 1);
			case FCmpPredicate::OLT: return ConstantNumber::get(lnum < rnum, 1);
			case FCmpPredicate::OLE: return ConstantNumber::get(lnum <= rnum, 1);
			case FCmpPredicate::TRUE: return ConstantNumber::get(1, 1);
			case FCmpPredicate::FALSE: return ConstantNumber::get(0, 1);
			default: return nullptr;
			}
		} else {
			Instruction* insn = block->body.emplace();
			insn->op = OpCode::FCmp;
			insn->operands.emplace(lhs);
			insn->operands.emplace(rhs);
			insn->extra = (uint32_t)pred;

			insn->result = std::make_unique<Value>(ValueKind::Instruction, Type::Bool());
			insn->result->name = block->scope.add(name);

			return insn->result.get();
		}
		return nullptr;
	}

	Instruction* Builder::cbr(Value* pred, BasicBlock* truthy, BasicBlock* falsy) {
		if (pred->type->kind != TypeKind::Bool)
			return nullptr;

		Instruction* insn = block->body.emplace();
		insn->op = OpCode::CondBr;
		insn->operands.emplace(pred);
		insn->operands.emplace((Value*)truthy);
		insn->operands.emplace((Value*)falsy);

		return insn;
	}

} // namespace tea::mir
