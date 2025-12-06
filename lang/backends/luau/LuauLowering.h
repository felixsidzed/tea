#pragma once

#include "mir/mir.h"

enum LuauOpcode;

namespace tea::backend {

	class ProtoBuilder {
	public:
		tea::vector<uint8_t> k;
		tea::vector<uint32_t> code;
		tea::vector<tea::string>* strtab = nullptr;
		tea::map<tea::string, uint32_t>* strRemap = nullptr;

		uint8_t maxstacksize = 0;
		uint8_t numparams = 0;
		uint8_t nups = 0;
		uint8_t is_vararg = 0;
		uint8_t flags = 0;
		uint8_t sizek = 0;

		ProtoBuilder(tea::vector<tea::string>* strtab, tea::map<tea::string, uint32_t>* strRemap) : strtab(strtab), strRemap(strRemap) {
		}

		uint32_t addConstantNil();
		void emitAux(uint32_t aux);
		uint32_t addConstantBool(bool value);
		uint32_t emitE(LuauOpcode op, int32_t e);
		uint32_t addConstantNumber(double value);
		uint32_t addString(const tea::string& str);
		uint32_t emitAD(LuauOpcode op, uint8_t a, int16_t d);
		uint32_t addConstantString(const tea::string& str);
		uint32_t emitABC(LuauOpcode op, uint8_t a, uint8_t b, uint8_t c);
	};

	class LuauLowering {
		tea::vector<uint8_t> M;
		std::unique_ptr<ProtoBuilder> proto = nullptr;

		uint8_t nextReg = 0;
		tea::map<const mir::Value*, uint8_t> valueMap;
		tea::map<const mir::BasicBlock*, uint32_t> blockLabels;
		tea::vector<std::pair<const mir::BasicBlock*, uint32_t>> jmpReloc;
		tea::map<const mir::Value*, std::pair<uint32_t, uint32_t>> globalMap;

	public:
		struct Options {
			const char* OutputFile = nullptr;
			bool DumpBytecode = false;
		};

		Options options;

		static void dump(uint8_t* data, size_t size);
		void lower(const mir::Module* module, Options options = {});

	private:
		void lowerFunction(const mir::Function* f);
		void lowerBlock(const mir::BasicBlock* block);
		void lowerInstruction(const mir::Instruction& insn);

		uint8_t lowerValue(const mir::Value* val, uint8_t dest);
	};

} // namespace tea::backend
