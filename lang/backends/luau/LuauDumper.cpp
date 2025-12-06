#include "LuauLowering.h"

#include "Bytecode.h"
#include "BytecodeUtils.h"

struct OpInfo {
	const char* name;
	enum { ABC, AB, AD, A, E, NONE } format;
};

static const OpInfo luau_opcode[] = {
	{ "NOP", OpInfo::NONE },
	{ "BREAK", OpInfo::NONE },
	{ "LOADNIL", OpInfo::A },
	{ "LOADB", OpInfo::ABC },
	{ "LOADN", OpInfo::AD },
	{ "LOADK", OpInfo::AD },
	{ "MOVE", OpInfo::AB },
	{ "GETGLOBAL", OpInfo::ABC },
	{ "SETGLOBAL", OpInfo::ABC },
	{ "GETUPVAL", OpInfo::AB },
	{ "SETUPVAL", OpInfo::AB },
	{ "CLOSEUPVALS", OpInfo::A },
	{ "GETIMPORT", OpInfo::AD },
	{ "GETTABLE", OpInfo::ABC },
	{ "SETTABLE", OpInfo::ABC },
	{ "GETTABLEKS", OpInfo::ABC },
	{ "SETTABLEKS", OpInfo::ABC },
	{ "GETTABLEN", OpInfo::ABC },
	{ "SETTABLEN", OpInfo::ABC },
	{ "NEWCLOSURE", OpInfo::AD },
	{ "NAMECALL", OpInfo::ABC },
	{ "CALL", OpInfo::ABC },
	{ "RETURN", OpInfo::AB },
	{ "JUMP", OpInfo::AD },
	{ "JUMPBACK", OpInfo::AD },
	{ "JUMPIF", OpInfo::AD },
	{ "JUMPIFNOT", OpInfo::AD },
	{ "JUMPIFEQ", OpInfo::AD },
	{ "JUMPIFLE", OpInfo::AD },
	{ "JUMPIFLT", OpInfo::AD },
	{ "JUMPIFNOTEQ", OpInfo::AD },
	{ "JUMPIFNOTLE", OpInfo::AD },
	{ "JUMPIFNOTLT", OpInfo::AD },
	{ "ADD", OpInfo::ABC },
	{ "SUB", OpInfo::ABC },
	{ "MUL", OpInfo::ABC },
	{ "DIV", OpInfo::ABC },
	{ "MOD", OpInfo::ABC },
	{ "POW", OpInfo::ABC },
	{ "ADDK", OpInfo::ABC },
	{ "SUBK", OpInfo::ABC },
	{ "MULK", OpInfo::ABC },
	{ "DIVK", OpInfo::ABC },
	{ "MODK", OpInfo::ABC },
	{ "POWK", OpInfo::ABC },
	{ "AND", OpInfo::ABC },
	{ "OR", OpInfo::ABC },
	{ "ANDK", OpInfo::ABC },
	{ "ORK", OpInfo::ABC },
	{ "CONCAT", OpInfo::ABC },
	{ "NOT", OpInfo::AB },
	{ "MINUS", OpInfo::AB },
	{ "LENGTH", OpInfo::AB },
	{ "NEWTABLE", OpInfo::AB },
	{ "DUPTABLE", OpInfo::AD },
	{ "SETLIST", OpInfo::ABC },
	{ "FORNPREP", OpInfo::AD },
	{ "FORNLOOP", OpInfo::AD },
	{ "FORGLOOP", OpInfo::AD },
	{ "FORGPREP_INEXT", OpInfo::AD },
	{ "FASTCALL3", OpInfo::ABC },
	{ "FORGPREP_NEXT", OpInfo::AD },
	{ "NATIVECALL", OpInfo::NONE },
	{ "GETVARARGS", OpInfo::AB },
	{ "DUPCLOSURE", OpInfo::AD },
	{ "PREPVARARGS", OpInfo::A },
	{ "LOADKX", OpInfo::A },
	{ "JUMPX", OpInfo::E },
	{ "FASTCALL", OpInfo::ABC },
	{ "COVERAGE", OpInfo::E },
	{ "CAPTURE", OpInfo::AB },
	{ "SUBRK", OpInfo::ABC },
	{ "DIVRK", OpInfo::ABC },
	{ "FASTCALL1", OpInfo::ABC },
	{ "FASTCALL2", OpInfo::ABC },
	{ "FASTCALL2K", OpInfo::ABC },
	{ "FORGPREP", OpInfo::AD },
	{ "JUMPXEQKNIL", OpInfo::AD },
	{ "JUMPXEQKB", OpInfo::AD },
	{ "JUMPXEQKN", OpInfo::AD },
	{ "JUMPXEQKS", OpInfo::AD },
	{ "IDIV", OpInfo::ABC },
	{ "IDIVK", OpInfo::ABC }
};

template<typename T>
static T read(const uint8_t* data, size_t& offset) {
	T value = *(T*)(data + offset);
	offset += sizeof(T);
	return value;
}

static uint32_t readVarInt(const uint8_t* data, size_t& offset) {
	uint32_t result = 0;
	uint32_t shift = 0;

	uint8_t byte;
	do {
		byte = data[offset++];
		result |= (uint32_t)(byte & 0x7F) << shift;
		shift += 7;
	} while (byte & 0x80);
	return result;
}

static const char* ttname(LuauBytecodeTag tag) {
	switch (tag) {
	case LBC_CONSTANT_NIL:		return "nil";
	case LBC_CONSTANT_BOOLEAN:	return "boolean";
	case LBC_CONSTANT_NUMBER:	return "number";
	case LBC_CONSTANT_STRING:	return "string";
	case LBC_CONSTANT_IMPORT:	return "import";
	case LBC_CONSTANT_TABLE:	return "table";
	case LBC_CONSTANT_CLOSURE:	return "closure";
	case LBC_CONSTANT_VECTOR:	return "vector";
	default:					return "unk";
	}
}

namespace tea::backend {

	void LuauLowering::dump(uint8_t* data, size_t size) {
		if (!data || size < 2)
			return;

		size_t offset = 0;

		uint8_t version = data[offset++];
		uint8_t typeversion = (version >= 4) ? data[offset++] : 0;

		printf("; bytecode version: %u.%u\n", version, typeversion);

		uint32_t nstrings = readVarInt(data, offset);
		tea::vector<tea::string> strtab(nstrings);

		if (nstrings) {
			printf("; string table (size: %u):\n", nstrings);
			for (uint32_t i = 0; i < nstrings; i++) {
				uint32_t len = readVarInt(data, offset);
				const char* str = (char*)data + offset;
				strtab.emplace(str, len);
				offset += len;

				printf(";    [%u] = \"", i);
				for (uint32_t j = 0; j < len; j++) {
					uint8_t c = str[j];
					if (isprint(c)) putchar(c);
					else printf("\\%02X", c);
				}
				fputs("\"\n", stdout);
			}
		}
		else
			printf("; empty string table\n");

		uint32_t nprotos = readVarInt(data, offset);
		printf("; proto count: %u\n\n", nprotos);

		for (uint32_t i = 0; i < nprotos; i++) {
			uint8_t maxstacksize = data[offset++];
			uint8_t numparams = data[offset++];
			uint8_t nups = data[offset++];
			uint8_t is_vararg = data[offset++];

			if (version >= 4) {
				offset += 1;
				offset += readVarInt(data, offset);
			}

			printf("; maxstacksize = %u, numparams = %u, nups = %u, is_varag = %s\nfunction anon_%u(??)\n",
				maxstacksize, numparams, nups, is_vararg ? "true" : "false", i);

			uint32_t sizecode = readVarInt(data, offset);
			for (uint32_t j = 0; j < sizecode; j++) {
				uint32_t insn = read<uint32_t>(data, offset);
				uint8_t op = LUAU_INSN_OP(insn);
				if (op > LOP__COUNT) {
					printf("  [%u] INVALID", j);
					continue;
				}

				#pragma warning(push)
				#pragma warning(disable : 6385)
				const OpInfo& info = luau_opcode[op];
				#pragma warning(pop)
				printf("  [%u] %s ", j, info.name);

				switch (info.format) {
				case OpInfo::ABC:
					printf("%u %u %u", LUAU_INSN_A(insn), LUAU_INSN_B(insn), LUAU_INSN_C(insn));
					break;
				case OpInfo::AB:
					printf("%u %u", LUAU_INSN_A(insn), LUAU_INSN_B(insn));
					break;
				case OpInfo::AD:
					printf("%u %d", LUAU_INSN_A(insn), LUAU_INSN_D(insn));
					break;
				case OpInfo::A:
					printf("%u", LUAU_INSN_A(insn));
					break;
				case OpInfo::E:
					printf("%d", LUAU_INSN_E(insn));
					break;
				default:
					break;
				}

				if (Luau::getOpLength((LuauOpcode)op) - 1) {
					printf(" ; aux = %u", read<uint32_t>(data, offset));
					j++;
				}

				putchar('\n');
			}

			uint32_t sizek = readVarInt(data, offset);
			if (sizek) {
				printf("\n; %u constant%c:\n", sizek, sizek - 1 ? 's' : '\0');

				for (uint32_t k = 0; k < sizek; k++) {
					LuauBytecodeTag tag = (LuauBytecodeTag)data[offset++];
					printf(";  [%u] %s: ", k, ttname(tag));

					switch (tag) {
					case LBC_CONSTANT_NIL:
						fputs("nil", stdout);
						break;

					case LBC_CONSTANT_BOOLEAN:
						printf("%s", data[offset++] ? "true" : "false");
						break;

					case LBC_CONSTANT_NUMBER:
						printf("%g", read<double>(data, offset));
						break;

					case LBC_CONSTANT_STRING: {
						putchar('"');
						const tea::string& str = strtab[readVarInt(data, offset)];
						for (size_t ij = 0; ij < str.length(); ij++) {
							uint8_t c = str[ij];
							if (isprint(c))
								putchar(c);
							else
								printf("\\%02X", c);
						}
						putchar('"');
					} break;

					case LBC_CONSTANT_IMPORT:
						printf("import(%u)", read<uint32_t>(data, offset));
						break;

					case LBC_CONSTANT_TABLE: {
						uint32_t n = readVarInt(data, offset);
						printf("table(keys=%u)", n);
						for (uint32_t ij = 0; ij < n; ij++)
							readVarInt(data, offset);
					} break;

					case LBC_CONSTANT_CLOSURE:
						printf("closure(%u)", readVarInt(data, offset));
						break;

					case LBC_CONSTANT_VECTOR:
						printf("vector(%.3f, %.3f, %.3f)", read<float>(data, offset), read<float>(data, offset), read<float>(data, offset));
						break;

					default:
						fputs("unk", stdout);
						break;
					}

					if (k != sizek)
						putchar('\n');
				}
			}

			readVarInt(data, offset); // sizep
			readVarInt(data, offset); // linedefined
			readVarInt(data, offset); // debugname
			// lineinfo & debuginfo
			offset += 2;

			puts("end\n");
		}
	}

} // namespace tea::backend
