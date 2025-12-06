#include "LuauLowering.h"

#include <fstream>

#include "common/tea.h"
#include "mir/dump/dump.h"

#include "Bytecode.h"

#define min(a,b) (((a) < (b)) ? (a) : (b))
#define max(a,b) (((a) > (b)) ? (a) : (b))

namespace tea::backend {

	static void writeVarInt(tea::vector<uint8_t>& buffer, uint32_t value) {
		do {
			uint8_t byte = value & 0x7F;
			value >>= 7;
			if (value)
				byte |= 0x80;
			buffer.emplace(byte);
		} while (value);
	}

	static uint32_t strhash(const char* name) {
		uint32_t len = (uint32_t)strlen(name);
		uint32_t h = len;

		for (size_t i = len; i > 0; i--)
			h ^= (h << 5) + (h >> 2) + (uint8_t)name[i - 1];
		return h;
	}

	uint32_t ProtoBuilder::addString(const tea::string& str) {
		if (auto* it = strRemap->find(str))
			return *it;
		uint32_t idx = strtab->size;
		strtab->emplace(str);
		strRemap->operator[](str) = idx;
		return idx;
	}

	uint32_t ProtoBuilder::addConstantNil() {
		uint32_t idx = sizek++;
		k.emplace(LBC_CONSTANT_NIL);
		return idx;
	}

	uint32_t ProtoBuilder::addConstantBool(bool value) {
		uint32_t idx = sizek++;
		k.emplace(LBC_CONSTANT_BOOLEAN);
		k.emplace(value ? 1 : 0);
		return idx;
	}

	uint32_t ProtoBuilder::addConstantNumber(double value) {
		uint32_t idx = sizek++;
		k.emplace(LBC_CONSTANT_NUMBER);
		uint8_t* bytes = (uint8_t*)&value;
		for (int i = 0; i < 8; i++)
			k.emplace(bytes[i]);
		return idx;
	}

	uint32_t ProtoBuilder::addConstantString(const tea::string& str) {
		uint32_t idx = sizek++;
		k.emplace(LBC_CONSTANT_STRING);
		uint32_t stringIdx = addString(str);
		writeVarInt(k, stringIdx);
		return idx;
	}

	uint32_t ProtoBuilder::emitABC(LuauOpcode op, uint8_t a, uint8_t b, uint8_t c) {
		if (op == LOP_MOVE && a == b)
			return code.size;

		return (uint32_t)(code.emplace(op | (a << 8) | (b << 16) | (c << 24)) - code.data);
	}

	uint32_t ProtoBuilder::emitAD(LuauOpcode op, uint8_t a, int16_t d) {
		return (uint32_t)(code.emplace(op | (a << 8) | (((uint32_t)d & 0xFFFF) << 16)) - code.data);
	}

	uint32_t ProtoBuilder::emitE(LuauOpcode op, int32_t e) {
		return (uint32_t)(code.emplace(op | ((e & 0xFFFFFF) << 8)) - code.data);
	}

	void ProtoBuilder::emitAux(uint32_t aux) {
		code.emplace(aux);
	}

	void LuauLowering::lower(const mir::Module* module, Options options) {
		this->options = options;
		M.clear();
		M.reserve(4);

		tea::vector<tea::string> strtab;
		tea::map<tea::string, uint32_t> strRemap;

		tea::vector<std::unique_ptr<ProtoBuilder>> protos;

		{
			proto = std::make_unique<ProtoBuilder>(&strtab, &strRemap);

			proto->emitABC(LOP_GETGLOBAL, 0, 0, (uint8_t)strhash("main"));
			proto->emitAux(proto->addConstantString("main"));
			proto->emitABC(LOP_CALL, 0, 0, 0);

			nextReg = 0;
			protos.emplace(std::move(proto));
		}

		for (const auto& value : module->body) {
			switch (value->kind) {
			case mir::ValueKind::Function: {
				const mir::Function* f = (const mir::Function*)value.get();
				if (f->blocks.empty())
					continue;

				proto = std::make_unique<ProtoBuilder>(&strtab, &strRemap);

				lowerFunction(f);
				protos.emplace(std::move(proto));
			} break;

			case mir::ValueKind::Global: {
				const mir::Global* g = (const mir::Global*)value.get();
				if (g->initializer && g->initializer->kind == mir::ValueKind::Constant && g->initializer->subclassData == (uint32_t)mir::ConstantKind::String) {
					const tea::string& str = ((mir::ConstantString*)g->initializer)->value;
	
					if (auto* it = strRemap.find(str)) {
						globalMap[value.get()] = { 0, *it };
					} else {
						uint32_t idx = strtab.size;
						strtab.emplace(str);
						strRemap[str] = idx;
						globalMap[value.get()] = { 0, idx };
					}
					break;
				}
			} TEA_FALLTHROUGH;

			default:
				fputs("unable to lower value:\n  ", stderr);
				tea::mir::dump(value.get());
				putchar('\n');
				TEA_PANIC("");
				TEA_UNREACHABLE();
			}
		}

		M.emplace(LBC_VERSION_TARGET);
		M.emplace(LBC_TYPE_VERSION_TARGET);

		writeVarInt(M, strtab.size);
		for (const auto& str : strtab) {
			writeVarInt(M, (uint32_t)str.length());
			for (uint32_t i = 0; i < str.length(); i++)
				M.emplace(str[i]);
		}

		writeVarInt(M, protos.size);
		for (const auto& proto : protos) {
			M.reserve(8);

			M.emplace(proto->maxstacksize);
			M.emplace(proto->numparams);
			M.emplace(proto->nups);
			M.emplace(proto->is_vararg);
			M.emplace(proto->flags);

			// TODO: provide type info
			writeVarInt(M, 0);

			M.reserve(proto->code.size);
			writeVarInt(M, proto->code.size);

			for (uint32_t insn : proto->code) {
				M.emplace((insn >> 0) & 0xFF);
				M.emplace((insn >> 8) & 0xFF);
				M.emplace((insn >> 16) & 0xFF);
				M.emplace((insn >> 24) & 0xFF);
			}
			
			M.reserve(proto->k.size);
			writeVarInt(M, proto->sizek);
			if (proto->sizek > 0) {
				for (const auto& k : proto->k)
					M.emplace(k);
			}

			writeVarInt(M, 0);
			// TODO: provide linedefined
			writeVarInt(M, 0);
			// TODO: provide debugname
			writeVarInt(M, 0);
			// TODO: provide lineinfo
			M.emplace(0);
			// TODO: provide debuginfo
			M.emplace(0);
		}

		writeVarInt(M, 0);

		if (options.DumpBytecode) dump(M.data, M.size);

		std::ofstream stream(options.OutputFile, std::ios::binary);
		stream.write((char*)M.data, M.size);
		stream.close();
		M.clear();
	}

	void LuauLowering::lowerFunction(const mir::Function* f) {
		proto->numparams = f->params.size;
		proto->is_vararg = 0;
		proto->flags = 0;

		for (const auto& param : f->params)
			valueMap[param.get()] = nextReg++;

		for (const auto& block : f->blocks) {
			for (const auto& insn : block->body) {
				if (insn.op == mir::OpCode::Alloca)
					valueMap[insn.result.get()] = nextReg++;
			}
		}

		for (const auto& block : f->blocks) {
			blockLabels[block.get()] = proto->code.size;
			lowerBlock(block.get());
		}

		for (const auto& [bb, target] : jmpReloc) {
			int offset = (int)blockLabels[bb] - (int)(target + 1);

			uint32_t& insn = proto->code[target];
			insn = (insn & 0xFF) | (((insn >> 8) & 0xFF) << 8) | (((uint32_t)offset & 0xFFFF) << 16);
		}

		proto->maxstacksize = max(proto->maxstacksize, (uint32_t)nextReg);

		nextReg = 0;
		jmpReloc.clear();
		valueMap.data.clear();
		blockLabels.data.clear();
	}

	void LuauLowering::lowerBlock(const mir::BasicBlock* block) {
		for (const auto& insn : block->body)
			lowerInstruction(insn);
	}

	void LuauLowering::lowerInstruction(const mir::Instruction& insn) {
		uint8_t dest = 0;
		if (insn.result && insn.op != mir::OpCode::Alloca) {
			dest = nextReg++;
			valueMap[insn.result.get()] = dest;
		}

		switch (insn.op) {
		case mir::OpCode::Add:
		case mir::OpCode::Sub:
		case mir::OpCode::Mul:
		case mir::OpCode::Div:
		case mir::OpCode::Mod:
			proto->emitABC((LuauOpcode)((uint32_t)LOP_ADD + (uint32_t)insn.op), dest, lowerValue(insn.operands[0], nextReg++), lowerValue(insn.operands[1], nextReg++));
			break;

		case mir::OpCode::And:
			proto->emitABC(LOP_AND, dest, lowerValue(insn.operands[0], nextReg++), lowerValue(insn.operands[1], nextReg++));
			break;

		case mir::OpCode::Or:
			proto->emitABC(LOP_OR, dest, lowerValue(insn.operands[0], nextReg++), lowerValue(insn.operands[1], nextReg++));
			break;

		case mir::OpCode::Not:
			proto->emitABC(LOP_NOT, dest, lowerValue(insn.operands[0], nextReg++), 0);
			break;

		case mir::OpCode::Xor: {
			uint8_t funcReg = nextReg++;
			proto->emitAD(LOP_GETIMPORT, funcReg, proto->addConstantString("bxor"));
			proto->emitAux((proto->addString("bit32")) | (proto->addString("bxor") << 10) | (2 << 30));

			lowerValue(insn.operands[0], funcReg + 1);
			lowerValue(insn.operands[0], funcReg + 2);

			proto->emitABC(LOP_CALL, funcReg, 3, 2);
			proto->emitABC(LOP_MOVE, dest, funcReg, 0);
			break;
		}

		case mir::OpCode::Shl:
		case mir::OpCode::Shr: {
			const char* name = insn.op == mir::OpCode::Shl ? "lshift" : "rshift";

			uint8_t funcReg = nextReg++;
			proto->emitAD(LOP_GETIMPORT, funcReg, proto->addConstantString(name));
			proto->emitAux((proto->addString("bit32")) | (proto->addString(name) << 10) | (2 << 30));

			lowerValue(insn.operands[0], funcReg + 1);
			lowerValue(insn.operands[0], funcReg + 2);

			proto->emitABC(LOP_CALL, funcReg, 3, 2);
			proto->emitABC(LOP_MOVE, dest, funcReg, 0);
			break;
		}

		case mir::OpCode::Load: {
			const mir::Value* ptr = insn.operands[0];

			if (auto* slot = valueMap.find(ptr))
				proto->emitABC(LOP_MOVE, dest, *slot, 0);
			else {
				uint8_t funcReg = nextReg++;
				proto->emitAD(LOP_GETIMPORT, funcReg, proto->addConstantString("__builtin_memread"));
				proto->emitAux(proto->addString("__builtin_memread") | (1 << 30));

				lowerValue(ptr, funcReg + 1);
				proto->emitABC(LOP_CALL, funcReg, 2, 2);
				proto->emitABC(LOP_MOVE, dest, funcReg, 0);
			}
			break;
		}

		case mir::OpCode::Store: {
			const mir::Value* ptr = insn.operands[0];
			uint8_t valReg = lowerValue(insn.operands[1], nextReg++);

			if (auto* slot = valueMap.find(ptr))
				proto->emitABC(LOP_MOVE, *slot, valReg, 0);
			else {
				uint8_t funcReg = nextReg++;
				proto->emitAD(LOP_GETIMPORT, funcReg, proto->addConstantString("__builtin_memwrite"));
				proto->emitAux(proto->addString("__builtin_memwrite") | (1 << 30));

				lowerValue(ptr, funcReg + 1);
				proto->emitABC(LOP_MOVE, funcReg + 2, valReg, 0);
				proto->emitABC(LOP_CALL, funcReg, 3, 1);
			}
			break;
		}

		case mir::OpCode::Alloca: {
			if (auto* slot = valueMap.find(insn.result.get()))
				valueMap[insn.result.get()] = *slot;
			break;
		}

		case mir::OpCode::FCmp:
		case mir::OpCode::ICmp: {
			uint8_t lhs = lowerValue(insn.operands[0], nextReg++);
			uint8_t rhs = lowerValue(insn.operands[1], nextReg++);

			if (insn.op == mir::OpCode::ICmp) {
				switch ((mir::ICmpPredicate)insn.extra) {
				case mir::ICmpPredicate::EQ:
					proto->emitAD(LOP_JUMPIFEQ, lhs, 2);
					proto->emitAux(rhs);
					proto->emitABC(LOP_LOADB, dest, 0, 1);
					proto->emitABC(LOP_LOADB, dest, 1, 0);
					break;
				case mir::ICmpPredicate::NEQ:
					proto->emitAD(LOP_JUMPIFNOTEQ, lhs, 2);
					proto->emitAux(rhs);
					proto->emitABC(LOP_LOADB, dest, 0, 1);
					proto->emitABC(LOP_LOADB, dest, 1, 0);
					break;
				case mir::ICmpPredicate::SGT:
				case mir::ICmpPredicate::UGT:
					proto->emitAD(LOP_JUMPIFLT, rhs, 2);
					proto->emitAux(lhs);
					proto->emitABC(LOP_LOADB, dest, 0, 1);
					proto->emitABC(LOP_LOADB, dest, 1, 0);
					break;
				case mir::ICmpPredicate::SGE:
				case mir::ICmpPredicate::UGE:
					proto->emitAD(LOP_JUMPIFLE, rhs, 2);
					proto->emitAux(lhs);
					proto->emitABC(LOP_LOADB, dest, 0, 1);
					proto->emitABC(LOP_LOADB, dest, 1, 0);
					break;
				case mir::ICmpPredicate::SLT:
				case mir::ICmpPredicate::ULT:
					proto->emitAD(LOP_JUMPIFLT, lhs, 2);
					proto->emitAux(rhs);
					proto->emitABC(LOP_LOADB, dest, 0, 1);
					proto->emitABC(LOP_LOADB, dest, 1, 0);
					break;
				case mir::ICmpPredicate::SLE:
				case mir::ICmpPredicate::ULE:
					proto->emitAD(LOP_JUMPIFLE, lhs, 2);
					proto->emitAux(rhs);
					proto->emitABC(LOP_LOADB, dest, 0, 1);
					proto->emitABC(LOP_LOADB, dest, 1, 0);
					break;
				default:
					break;
				}
			} else if (insn.op == mir::OpCode::FCmp) {
				switch ((mir::FCmpPredicate)insn.extra) {
				case mir::FCmpPredicate::OEQ:
					proto->emitAD(LOP_JUMPIFEQ, lhs, 2);
					proto->emitAux(rhs);
					proto->emitABC(LOP_LOADB, dest, 0, 1);
					proto->emitABC(LOP_LOADB, dest, 1, 0);
					break;
				case mir::FCmpPredicate::ONEQ:
					proto->emitAD(LOP_JUMPIFNOTEQ, lhs, 2);
					proto->emitAux(rhs);
					proto->emitABC(LOP_LOADB, dest, 0, 1);
					proto->emitABC(LOP_LOADB, dest, 1, 0);
					break;
				case mir::FCmpPredicate::OGT:
					proto->emitAD(LOP_JUMPIFLT, rhs, 2);
					proto->emitAux(lhs);
					proto->emitABC(LOP_LOADB, dest, 0, 1);
					proto->emitABC(LOP_LOADB, dest, 1, 0);
					break;
				case mir::FCmpPredicate::OGE:
					proto->emitAD(LOP_JUMPIFLE, rhs, 2);
					proto->emitAux(lhs);
					proto->emitABC(LOP_LOADB, dest, 0, 1);
					proto->emitABC(LOP_LOADB, dest, 1, 0);
					break;
				case mir::FCmpPredicate::OLT:
					proto->emitAD(LOP_JUMPIFLT, lhs, 2);
					proto->emitAux(rhs);
					proto->emitABC(LOP_LOADB, dest, 0, 1);
					proto->emitABC(LOP_LOADB, dest, 1, 0);
					break;
				case mir::FCmpPredicate::OLE:
					proto->emitAD(LOP_JUMPIFLE, lhs, 2);
					proto->emitAux(rhs);
					proto->emitABC(LOP_LOADB, dest, 0, 1);
					proto->emitABC(LOP_LOADB, dest, 1, 0);
					break;
				default:
					break;
				}
			}
		} break;

		case mir::OpCode::Br: {
			const mir::BasicBlock* target = (const mir::BasicBlock*)insn.operands[0];
			uint32_t pc = proto->emitAD(LOP_JUMP, 0, 0);
			jmpReloc.push({ target, pc });
		} break;

		case mir::OpCode::CondBr: {
			const mir::BasicBlock* truthy = (const mir::BasicBlock*)insn.operands[1];
			const mir::BasicBlock* falsy = (const mir::BasicBlock*)insn.operands[2];

			uint32_t pc = proto->emitAD(LOP_JUMPIF, lowerValue(insn.operands[0], nextReg++), 0);
			jmpReloc.push({ truthy, pc });

			pc = proto->emitAD(LOP_JUMP, 0, 0);
			jmpReloc.push({ falsy, pc });
		} break;

		case mir::OpCode::Ret:
			if (insn.operands.size == 0)
				proto->emitABC(LOP_RETURN, 0, 1, 0);
			else
				proto->emitABC(LOP_RETURN, lowerValue(insn.operands[0], nextReg++), 2, 0);
			break;

		case mir::OpCode::Call: {
			uint8_t callee = lowerValue(insn.operands[0], nextReg++);

			for (uint32_t i = 1; i < insn.operands.size; i++)
				proto->emitABC(LOP_MOVE, callee + i, lowerValue(insn.operands[i], callee + i), 0);

			proto->emitABC(LOP_CALL, callee, insn.operands.size, insn.result ? 2 : 1);
			if (insn.result)
				proto->emitABC(LOP_MOVE, dest, callee, 0);
		} break;

		case mir::OpCode::Cast:
			lowerValue(insn.operands[0], dest);
			break;

		case mir::OpCode::Unreachable:
			proto->emitABC(LOP_RETURN, 0, 1, 0);
			break;

		default:
			break;
		}
	}

	uint8_t LuauLowering::lowerValue(const mir::Value* val, uint8_t dest) {
		switch (val->kind) {
		case mir::ValueKind::Function: {
			const mir::Function* f = (const mir::Function*)val;

			proto->emitABC(LOP_GETGLOBAL, dest, 0, (uint8_t)strhash(f->name));
			proto->emitAux(proto->addConstantString(f->name));

			return dest;
		} break;

		case mir::ValueKind::Constant: {
			switch ((mir::ConstantKind)val->subclassData) {
			case mir::ConstantKind::Number: {
				mir::ConstantNumber* num = (mir::ConstantNumber*)val;
				if (val->type->isNumeric()) {
					int64_t intVal = (int64_t)num->getInteger();
					if (intVal >= -32768 && intVal <= 32767)
						proto->emitAD(LOP_LOADN, dest, (int16_t)intVal);
					else
						proto->emitAD(LOP_LOADK, dest, proto->addConstantNumber((double)intVal));
				} else
					proto->emitAD(LOP_LOADK, dest, proto->addConstantNumber(num->getDouble()));
				break;
			}

			case mir::ConstantKind::String: {
				mir::ConstantString* str = (mir::ConstantString*)val;
				proto->emitAD(LOP_LOADK, dest, proto->addConstantString(str->value));
				break;
			}

			default:
				proto->emitABC(LOP_LOADNIL, dest, 0, 0);
				break;
			}

			return dest;
		} break;

		case mir::ValueKind::Global: {
			if (auto* it = globalMap.find(val)) {
				switch (it->first) {
				case 0: {
					uint16_t k = proto->sizek++;
					proto->k.emplace(LBC_CONSTANT_STRING);
					writeVarInt(proto->k, it->second);
					proto->emitAD(LOP_LOADK, dest, k);
					return dest;
				}

				default:
					goto unable;
				}
			}
		} break;

		case mir::ValueKind::Instruction: {
			if (auto* it = valueMap.find(val)) {
				proto->emitABC(LOP_MOVE, dest, *it, 0);
				return dest;
			}
		} break;

		default:
		unable:
			fputs("unable to lower value:\n  ", stderr);
			tea::mir::dump(val);
			putchar('\n');
			TEA_PANIC("");
			TEA_UNREACHABLE();
		}
		TEA_UNREACHABLE();
	}

} // namespace tea::backend
