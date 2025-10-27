#include "types.h"

#include <regex>
#include <sstream>

#include "tea/tea.h"

namespace tea {
	vector<LLVMTypeRef> Type::convert;
	map<string, enum Type::Kind> Type::name2kind = {
		{"int", Type::INT},
		{"float", Type::FLOAT},
		{"double", Type::DOUBLE},
		{"char", Type::CHAR},
		{"string", Type::STRING},
		{"void", Type::VOID_},
		{"bool", Type::BOOL},
		{"long", Type::LONG}
	};

	static bool typesInitialized = false;
	static inline void initializeTypes() {
		if (!typesInitialized) {
			typesInitialized = true;

			LLVMContextSetOpaquePointers(LLVMGetGlobalContext(), false);

			/* ORDER BASIC_TYPE */
			Type::convert.push(LLVMVoidType());
			Type::convert.push(LLVMInt1Type());
			Type::convert.push(LLVMInt8Type());
			Type::convert.push(LLVMFloatType());
			Type::convert.push(LLVMInt32Type());
			Type::convert.push(LLVMPointerType(LLVMInt8Type(), 0));
			Type::convert.push(LLVMDoubleType());
			Type::convert.push(LLVMInt64Type());
		}
	}

	std::pair<Type, bool> Type::get(const string& name) {
		initializeTypes();
		Type result;
		std::string tmp(name.data, name.size);

		if (name.empty())
			return { result, false };

		result.constant = tmp.find("const") != std::string::npos;
		tmp = std::regex_replace(tmp, std::regex("const"), "");
		tmp = std::regex_replace(tmp, std::regex("^\\s+|\\s+$"), "");

		std::regex funcRe(R"(func\s*\(\s*([^)]+)\s*\)\s*\(\s*([^\)]*)\s*\))");
		std::smatch funcMatch;
		if (std::regex_match(tmp, funcMatch, funcRe)) {
			std::string argsStr = funcMatch[2];
			std::string retTypeStr = funcMatch[1];

			auto [retType, okRet] = Type::get({ retTypeStr.c_str(), (uint32_t)retTypeStr.size() });
			if (!okRet)
				return { result, false };

			vector<LLVMTypeRef> argTypes;
			bool vararg = false;

			if (!argsStr.empty()) {
				std::stringstream ss(argsStr);
				std::string arg;
				while (std::getline(ss, arg, ',')) {
					arg = std::regex_replace(arg, std::regex("^\\s+|\\s+$"), "");
					if (arg == "...") {
						vararg = true;
						continue;
					}
					auto [argType, okArg] = Type::get({ arg.c_str(), (uint32_t)arg.size() });
					if (!okArg)
						return { result, false };
					argTypes.push(argType.llvm);
				}
			}

			result.llvm = LLVMPointerType(LLVMFunctionType(retType.llvm, argTypes.empty() ? nullptr : argTypes.data, argTypes.size, vararg), 0);
			int nptr = 0;
			for (char c : tmp)
				if (c == '*') nptr++;
			tmp.erase(std::remove(tmp.begin(), tmp.end(), '*'), tmp.end());
			tmp = std::regex_replace(tmp, std::regex("^\\s+|\\s+$"), "");
			while (--nptr)
				result.llvm = LLVMPointerType(result.llvm, 0);
			return { result, true };
		}

		int nptr = 0;
		for (char c : tmp)
			if (c == '*') nptr++;
		tmp.erase(std::remove(tmp.begin(), tmp.end(), '*'), tmp.end());
		tmp = std::regex_replace(tmp, std::regex("^\\s+|\\s+$"), "");

		if (tmp.find("unsigned") != std::string::npos) {
			result.sign = false;
			tmp = std::regex_replace(tmp, std::regex("unsigned"), "");
		} else if (tmp.find("signed") != std::string::npos) {
			result.sign = true;
			tmp = std::regex_replace(tmp, std::regex("signed"), "");
		}

		tmp = std::regex_replace(tmp, std::regex("^\\s+|\\s+$"), "");

		std::regex re(R"(([a-zA-Z_]+)(\s*(\[[^\]]*\])*)?)");
		std::smatch match;
		if (!std::regex_match(tmp, match, re))
			return { result, false };

		const std::string& basename = match[1];
		const std::string& array = match[2];

		auto it = name2kind.find(basename.c_str());
		if (!it)
			return { result, false };

		enum Kind kind = *it;
		if (kind == Type::VOID_ && nptr > 0)
			kind = Type::CHAR;

		LLVMTypeRef llvmType = convert[kind];
		if (!array.empty()) {
			std::regex reDims(R"(\[(\d*)\])");
			auto dimStart = std::sregex_iterator(array.begin(), array.end(), reDims);
			auto dimsEnd = std::sregex_iterator();

			for (auto it = dimStart; it != dimsEnd; ++it) {
				const std::string& dim = (*it)[1];
				if (dim.empty())
					return { result, false };
				llvmType = LLVMArrayType(llvmType, std::stoi(dim));
			}
		}

		for (int i = 0; i < nptr; i++)
			llvmType = LLVMPointerType(llvmType, 0);

		result.llvm = llvmType;
		return { result, true };
	}

	void Type::create(const string& name, LLVMTypeRef type) {
		initializeTypes();
		convert.push(type);
		name2kind[name] = (Kind)(convert.size - 1);
	}
}
