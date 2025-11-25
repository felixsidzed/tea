#include "dump.h"

namespace tea::mir {

	// note: this table should only be used by the instruction dumper
	// NULL entries are handled separately so they have no reason to be here
	static const char* opcodeName[] = {
		"add", "sub", "mul", "div", "mod",
		"not", "and", "or", "xor", "shl", "shr",
		NULL, NULL,
		"load", "store", NULL, NULL,
		NULL, NULL, "ret", "phi",
		NULL,
		"nop", NULL, "unreachable"
	};

	static const char* icmpPredName[] = {
		"eq", "neq",
		"sgt", "ugt", "sge", "uge",
		"slt", "ult", "sle", "ule"
	};

	static const char* fcmpPredName[] = {
		"oeq", "oneq",
		"ogt", "oge", "olt", "ole",
		"true", "false"
	};

	void dump(const tea::mir::Module* module) {
		printf(
			"// Source: %s\n// Data layout: \"%c-%u\"\n\n",
			module->source.data(),
			module->dl.endian == (uint8_t)std::endian::big ? 'E' : 'e',
			module->dl.maxNativeBytes
		);

		for (const auto& g : module->body) {
			switch (g->kind) {
			case ValueKind::Function:
				dump((Function*)g.get());
				putchar('\n');
				break;

			case ValueKind::Global: {
				Global* global = (Global*)g.get();
				printf("%s var @\"%s\": %s",
					(StorageClass)global->storage == StorageClass::Public ? "public" : "private",
					global->name,
					global->type->getElementType()->str().data()
				);
				if (global->initializer) {
					putchar(' '); putchar('='); putchar(' ');
					dump(global->initializer);
				}
				putchar('\n');
				break;
			}

			default:
				printf("unk\n");
				break;
			}

			putchar('\n');
		}
	}

	void dump(const tea::mir::Function* func) {
		printf("%s func %s(",
			(StorageClass)func->subclassData == StorageClass::Public ? "public" : "private",
			func->name
		);
		for (const auto& param : func->params)
			dump(param.get());
		printf(") -> %s\n", ((FunctionType*)func->type)->returnType->str().data());

		for (const auto& block : func->blocks) {
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
		case OpCode::Alloca:
			printf("%%\"%s\" = alloca %s", insn->result.name, ((PointerType*)insn->result.type)->pointee->str().data());
			break;

		case OpCode::Cast:
			printf("%%\"%s\" = cast ", insn->result.name);
			dump(insn->operands[0]);
			printf(", %s", insn->result.type->str().data());
			break;

		case OpCode::Ret:
			if (insn->operands.size > 0)
				goto _default;
			else
				fputs("ret void", stdout);
			break;

		case OpCode::GetElementPtr: {
			printf("%%\"%s\" = gep ", insn->result.name);

			// TODO: genuinely
			Type* oldType = insn->operands[0]->type;
			insn->operands[0]->type = insn->result.type;
			dump(insn->operands[0]);
			insn->operands[0]->type = oldType;

			for (uint32_t i = 1; i < insn->operands.size; i++) {
				putchar(','); putchar(' ');
				dump(insn->operands[i]);
			}
		} break;

		case OpCode::Br:
			printf("br %%\"%s\"", ((BasicBlock*)insn->operands[0])->name);
			break;

		case OpCode::Call: {
			if (insn->result.kind != ValueKind::Null)
				printf("%%\"%s\" = ", insn->result.name);

			fputs("call ", stdout);
			dump(insn->operands[0]);

			putchar(' '); putchar('(');
			if (insn->operands.size > 1) {
				dump(insn->operands[1]);

				for (uint32_t i = 2; i < insn->operands.size; i++) {
					putchar(','); putchar(' ');
					dump(insn->operands[i]);
				}
			}
			putchar(')');
		} break;

		case OpCode::ICmp:
			printf("%%\"%s\" = icmp %s ", insn->result.name, icmpPredName[(uint32_t)(uintptr_t)insn->operands[0]]);
			dump(insn->operands[1]);
			putchar(','); putchar(' ');
			dump(insn->operands[2]);
			break;

		case OpCode::FCmp:
			printf("%%\"%s\" = fcmp %s ", insn->result.name, fcmpPredName[(uint32_t)(uintptr_t)insn->operands[0]]);
			dump(insn->operands[1]);
			putchar(','); putchar(' ');
			dump(insn->operands[2]);
			break;

		case OpCode::CondBr:
			fputs("cbr ", stdout);
			dump(insn->operands[0]);
			printf(", %%\"%s\", %%\"%s\"", ((BasicBlock*)insn->operands[1])->name, ((BasicBlock*)insn->operands[2])->name);
			break;

		default:
		_default:
			if (insn->result.kind != ValueKind::Null)
				printf("%%\"%s\" = ", insn->result.name);
			printf("%s ", opcodeName[(uint32_t)insn->op]);
		
			if (insn->operands.size > 0) {
				dump(insn->operands[0]);

				for (uint32_t i = 1; i < insn->operands.size; i++) {
					putchar(','); putchar(' ');
					dump(insn->operands[i]);
				}
			}
			break;
		}
	}

	void dump(const tea::mir::Value* value) {
		switch (value->kind) {
		case ValueKind::Null:
			fputs("null", stdout);
			break;

		case ValueKind::Constant: {
			switch ((ConstantKind)value->subclassData) {
			case ConstantKind::Number: {
				ConstantNumber* num = (ConstantNumber*)value;
				if (num->type->kind == TypeKind::Bool) {
					fputs(num->getInteger() ? "true" : "false", stdout);
				} else if (num->type->isNumeric())
					if (num->type->sign)
						printf("%s %lld", num->type->str().data(), num->getSInteger());
					else
						printf("%s %llu", num->type->str().data(), num->getInteger());
				else
					printf("%s %g", num->type->str().data(), num->getDouble());
			} break;

			case ConstantKind::String: {
				ConstantString* str = (ConstantString*)value;

				putchar('"');
				for (size_t i = 0; i < str->value.length(); i++) {
					uint8_t c = str->value[i];
					if (isprint(c))
						putchar(c);
					else
						printf("\\%02X", c);
				}
				putchar('"');
			} break;

			case ConstantKind::Array: {
				ConstantArray* arr = (ConstantArray*)value;

				putchar('[');
				if (arr->values.size > 0) {
					dump(arr->values[0]);

					for (uint32_t i = 1; i < arr->values.size; i++) {
						putchar(','); putchar(' ');
						dump(arr->values[i]);
					}
				}
				putchar(']');
			} break;

			default:
				break;
			}
		} break;

		case ValueKind::Function:
		case ValueKind::Global:
			printf("%s @\"%s\"", value->type->str().data(), value->name);
			break;
		
		case ValueKind::Parameter:
		case ValueKind::Instruction:
			printf("%s %%\"%s\"", value->type->str().data(), value->name);
			break;

		default:
			printf("%s %s", value->type->str().data(), value->name);
			break;
		}
	}

	void dump(const tea::StructType* ty) {
		fputs("struct { ", stdout);
		if (ty->body.size > 0) {
			fputs(ty->body[0]->str().data(), stdout);

			for (uint32_t i = 1; i < ty->body.size; i++) {
				putchar(','); putchar(' ');
				fputs(ty->body[i]->str().data(), stdout);
			}
		}
		printf(" } %s", ty->name);
	}

} // namespace tea::mir
