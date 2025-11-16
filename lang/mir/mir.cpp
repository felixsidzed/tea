#include "mir.h"

#include "frontend/parser/AST.h"

namespace tea::mir {

	Function* Module::addFunction(const tea::string& name, tea::FunctionType* ftype) {
		if (name.empty() || ftype->kind != TypeKind::Function)
			return nullptr;

		Function* f = (Function*)body.emplace(std::make_unique<Function>(StorageClass::Public, ftype))->get();
		f->name = name;

		uint32_t i = 1;
		for (tea::Type* paramType : ftype->params) {
			auto param = std::make_unique<Value>(ValueKind::Parameter, paramType);
			param->name = std::format("arg{}", i).c_str();
			f->params.emplace(std::move(param));

			i++;
		}

		return f;
	}

	BasicBlock* Function::appendBlock(const tea::string& name) {
		BasicBlock* block = body.emplace();
		block->name = scope.add(name);
		return block;
	}

	ConstantNumber* ConstantNumber::get(uint64_t val, uint8_t width, bool sign, Context* ctx) {
		if (!ctx) ctx = getGlobalContext();

		Type* type = nullptr;
		switch (width) {
		case 1:
			type = Type::Bool(false, ctx);
			break;
		case 8:
			type = Type::Char(false, sign, ctx);
			break;
		case 16:
			type = Type::Short(false, sign, ctx);
			break;
		case 32:
			type = Type::Int(false, sign, ctx);
			break;
		case 64:
			type = Type::Long(false, sign, ctx);
			break;
		default:
			return nullptr;
		}

		std::unique_ptr<ConstantNumber>* entry = nullptr;
		if (val == 0 || val == 1) {
			auto& map = val == 0 ? ctx->num0Const : ctx->num1Const;
			auto& entry = map[width];
			if (!entry)
				entry.reset(new ConstantNumber(type, val));
			return entry.get();
		} else {
			auto& entry = ctx->numConst[val];
			if (!entry)
				entry.reset(new ConstantNumber(type, val));
			return entry.get();
		}
	}

	Instruction* Builder::ret(Value* val) {
		Instruction* insn = block->body.emplace();
		insn->op = OpCode::Ret;
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

} // namespace tea::mir
