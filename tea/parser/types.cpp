#include "types.h"

#include <regex>

namespace tea {
	std::vector<LLVMTypeRef> Type::convert;
	std::unordered_map<std::string, enum Type::Kind> Type::name2kind = {
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

			/* ORDER BASIC_TYPE */
			Type::convert.push_back(LLVMInt32Type());
			Type::convert.push_back(LLVMFloatType());
			Type::convert.push_back(LLVMDoubleType());
			Type::convert.push_back(LLVMInt8Type());
			Type::convert.push_back(LLVMPointerType(Type::get(Type::CHAR).llvm, 0));
			Type::convert.push_back(LLVMVoidType());
			Type::convert.push_back(LLVMInt1Type());
			Type::convert.push_back(LLVMInt64Type());
		}
	}

	std::pair<Type, bool> Type::get(const std::string& name) {
		initializeTypes();
		Type result;
		std::string tmp = name;

		result.constant = tmp.find("const") != std::string::npos;
		tmp = std::regex_replace(tmp, std::regex("const"), "");
		tmp = std::regex_replace(tmp, std::regex("^\\s+|\\s+$"), "");

		int nptr = 0;
		for (char c : tmp)
			if (c == '*') nptr++;
		tmp.erase(std::remove(tmp.begin(), tmp.end(), '*'), tmp.end());
		tmp = std::regex_replace(tmp, std::regex("^\\s+|\\s+$"), "");

		std::regex re(R"(([a-zA-Z_]+))"); // ([a-zA-Z_]+)(\s*(\[[^\]]*\])*)?
		std::smatch match;
		if (!std::regex_match(tmp, match, re))
			return { result, false };

		const std::string& basename = match[1];
		const std::string& array = match[2];

		if (name2kind.find(basename) == name2kind.end())
			return { result, false };

		enum Kind kind = name2kind[basename];
		/*if (kind == Type::VOID_ && nptr > 0)
			kind = Type::CHAR;*/

		LLVMTypeRef llvmType = convert[kind];

		/*if (!array.empty()) {
			std::regex reDims(R"(\[(\d*)\])");
			auto dimStart = std::sregex_iterator(array.begin(), array.end(), reDims);
			auto dimsEnd = std::sregex_iterator();

			for (auto it = dimStart; it != dimsEnd; ++it) {
				const std::string& dim = (*it)[1];
				if (dim.empty())
					return { result, false };
				llvmType = LLVMArrayType(llvmType, std::stoi(dim));
			}
		}*/

		for (int i = 0; i < nptr; i++)
			llvmType = LLVMPointerType(llvmType, 0);

		result.llvm = llvmType;
		return { result, true };
	}

	void Type::create(const std::string& name, LLVMTypeRef type) {
		initializeTypes();
		convert.push_back(type);
		name2kind[name] = (Kind)(convert.size() - 1);
	}
}
