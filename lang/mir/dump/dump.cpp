#include "dump.h"

namespace tea::mir {
	void dump(const tea::mir::Module* module) {
		for (const auto& g : module->body) {
			switch (g->kind) {
			case ValueKind::Function:
				dump((Function*)g.get());
				break;

			case ValueKind::Global: {
				Global* global = (Global*)g.get();
				printf("%s var %s: %s",
					(StorageClass)global->storage == StorageClass::Public ? "public" : "private",
					global->name,
					global->type->str().data()
				);
				if (global->initializer) {
					putchar(' '); putchar('='); putchar(' ');
					dump(global->initializer);
				}
				break;
			}

			default:
				printf("unknown");
				break;
			}
		}
		putchar('\n');
	}

	void dump(const tea::mir::Function* func) {
		printf("%s func %s(",
			(StorageClass)func->subclassData == StorageClass::Public ? "public" : "private",
			func->name
		);
		for (const auto& param : func->params)
			printf("%s %s", param->type->str().data(), param->name);
		printf(") -> %s\n", ((FunctionType*)func->type)->returnType->str().data());

		for (const auto& block : func->body) {
			printf("%s:\n", block.name);
			for (const auto& insn : block.body) {
				putchar(' '); putchar(' '); putchar(' '); putchar(' ');
				dump(&insn);
				putchar('\n');
			};
		}

		printf("end");
	}

	void dump(const tea::mir::Instruction* insn) {
		switch (insn->op) {
		case OpCode::Ret:
			printf("ret ");
			dump(insn->operands[0]);
			break;

		case OpCode::Nop:
			printf("nop");
			break;

		case OpCode::Alloca:
			printf("%%\"%s\" = alloca %s", insn->result.name, ((PointerType*)insn->result.type)->pointee->str().data());
			break;

		case OpCode::Store:
			printf("store ");
			goto dumpOperands; // :P

		case OpCode::Load:
			printf("%%\"%s\" = load ", insn->result.name);
			dump(insn->operands[0]);
			break;

		default:
			printf("unk ");
		dumpOperands:
			if (insn->operands.size > 0)
				dump(insn->operands[0]);

			for (uint32_t i = 1; i < insn->operands.size; i++) {
				putchar(','); putchar(' ');
				dump(insn->operands[i]);
			}
			break;
		}
	}

	void dump(const tea::mir::Value* value) {
		switch (value->kind) {
		case ValueKind::Null:
			printf("null");
			break;

		case ValueKind::Constant: {
			ConstantNumber* num = (ConstantNumber*)value;
			if (num->type->isNumeric()) {
				if (num->type->sign)
					printf("%s %llu", num->type->str().data(), num->getSInteger());
				else
					printf("%s %llu", num->type->str().data(), num->getInteger());
			}
		} break;

		case ValueKind::Instruction:
			printf("%s %%\"%s\"", value->type->str().data(), value->name);
			break;

		default:
			printf("%s %s", value->type->str().data(), value->name);
			break;
		}
	}
}
