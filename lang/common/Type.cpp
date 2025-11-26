#include "Type.h"

#include <sstream>
#include <unordered_map>

#include "common/vector.h"

#include "mir/Context.h"

namespace tea {
	static std::unordered_map<tea::string, TypeKind> name2kind = {
		{"int", TypeKind::Int},
		{"float", TypeKind::Float},
		{"double", TypeKind::Double},
		{"char", TypeKind::Char},
		{"string", TypeKind::String},
		{"void", TypeKind::Void},
		{"bool", TypeKind::Bool},
		{"long", TypeKind::Long}
	};

	Type* Type::get(const tea::string& name) {
		std::string s = { name.data(), name.length() };
		s.erase(std::remove_if(s.begin(), s.end(), isspace), s.end());

		size_t nptr = std::count(s.begin(), s.end(), '*');
		s.erase(std::remove(s.begin(), s.end(), '*'), s.end());

		tea::vector<tea::string> tokens; {
			std::stringstream ss(s);
			std::string tok;
			while (ss >> tok)
				tokens.push({ tok.data(), tok.size() });
		}

		bool sign = true;
		bool constant = false;
		TypeKind kind = TypeKind::Void;

		for (const auto& tok : tokens) {
			if (tok == "const")
				constant = true;
			else if (tok == "unsigned")
				sign = false;
			else if (tok == "signed")
				sign = true;
			else if (tok == "*")
				continue;
			else {
				auto it = name2kind.find(tok);
				if (it != name2kind.end())
					kind = it->second;
				else
					return nullptr;
			}
		}

		Type* current = mir::getGlobalContext()->getType(kind, constant, sign);

		for (size_t i = 0; i < nptr; i++) {
			bool constp = false;
			if (s.find("* const") != std::string::npos && i == nptr - 1)
				constp = true;
			
			current = mir::getGlobalContext()->getType<PointerType>(current, constp);
		}

		return current;
	}

	Type* Type::Void(bool constant, mir::Context* ctx) {
		if (!ctx) ctx = mir::getGlobalContext();
		return ctx->getType(TypeKind::Void, constant);
	}
	Type* Type::Bool(bool constant, mir::Context* ctx) {
		if (!ctx) ctx = mir::getGlobalContext();
		return ctx->getType(TypeKind::Bool, constant);
	}
	Type* Type::String(bool constant, mir::Context* ctx) {
		if (!ctx) ctx = mir::getGlobalContext();
		return ctx->getType(TypeKind::String, constant);
	}
	Type* Type::Int(bool constant, bool sign, mir::Context* ctx) {
		if (!ctx) ctx = mir::getGlobalContext();
		return ctx->getType(TypeKind::Int, constant, sign);
	}
	Type* Type::Long(bool constant, bool sign, mir::Context* ctx) {
		if (!ctx) ctx = mir::getGlobalContext();
		return ctx->getType(TypeKind::Long, constant, sign);
	}
	Type* Type::Char(bool constant, bool sign, mir::Context* ctx) {
		if (!ctx) ctx = mir::getGlobalContext();
		return ctx->getType(TypeKind::Char, constant, sign);
	}
	Type* Type::Short(bool constant, bool sign, mir::Context* ctx) {
		if (!ctx) ctx = mir::getGlobalContext();
		return ctx->getType(TypeKind::Short, constant, sign);
	}
	Type* Type::Float(bool constant, bool sign, mir::Context* ctx) {
		if (!ctx) ctx = mir::getGlobalContext();
		return ctx->getType(TypeKind::Float, constant, sign);
	}
	Type* Type::Double(bool constant, bool sign, mir::Context* ctx) {
		if (!ctx) ctx = mir::getGlobalContext();
		return ctx->getType(TypeKind::Double, constant, sign);
	}

	PointerType* Type::Pointer(Type* pointee, bool constant, mir::Context* ctx) {
		if (!ctx) ctx = mir::getGlobalContext();
		return ctx->getType<PointerType>(pointee, constant);
	}
	FunctionType* Type::Function(Type* returnType, const tea::vector<Type*>& params, bool vararg, mir::Context* ctx) {
		if (!ctx) ctx = mir::getGlobalContext();
		return ctx->getType<FunctionType>(returnType, params, vararg);
	}

	ArrayType* Type::Array(Type* elementType, uint32_t size, bool constant, mir::Context* ctx) {
		if (!ctx) ctx = mir::getGlobalContext();
		return ctx->getType<ArrayType>(elementType, size, constant);
	}

	StructType* Type::Struct(Type** body, uint32_t n, const char* name, bool packed, mir::Context* ctx) {
		if (!ctx) ctx = mir::getGlobalContext();

		size_t h = 1469598103934665603ull;
		for (size_t i = 0; i < strlen(name); i++)
			h = (h ^ (uint8_t)name[i]) * 1099511628211ull;
		for (size_t i = 0; i < n; i++)
			h = (h ^ (uintptr_t)body[n]) * 1099511628211ull;
		h = (h ^ (uint8_t)packed) * 1099511628211ull;

		auto& entry = ctx->structTypes[h];
		if (!entry)
			entry.reset(new StructType(body, n, name, packed));
		return entry.get();
	}

	tea::string Type::str() {
		tea::string result;

		switch (kind) {
		case TypeKind::Pointer:
			result += ((PointerType*)this)->pointee->str();
			result += '*';
			if (constant)
				result += " const";
			break;

		case TypeKind::Function: {
			FunctionType* ftype = (FunctionType*)this;

			if (ftype->returnType) {
				result += ftype->returnType->str().data();
				result += ')'; result += '(';
			}
			else
				result += "void)(";

			for (uint32_t i = 0; i < ftype->params.size; ++i) {
				if (i > 0) result += ", ";
				result += ftype->params[i]->str().data();
			}

			if (ftype->vararg) {
				if (ftype->params.size > 0)
					result += ", ";
				result += "...";
			}

			result += ')';
		} break;

		case TypeKind::Array: {
			ArrayType* arr = (ArrayType*)this;
			result += arr->elementType->str() + "[" + std::to_string(arr->size).c_str() + "]";
		} break;

		case TypeKind::Struct:
			result = ((StructType*)this)->name;
			break;

		default: {
			if (constant)
				result = "const ";
			if (!sign)
				result = "unsigned ";

			switch (kind) {
			case TypeKind::Void:
				result += "void";
				break;
			case TypeKind::Bool:
				result += "bool";
				break;
			case TypeKind::Char:
				result += "char";
				break;
			case TypeKind::Short:
				result += "short";
				break;
			case TypeKind::Float:
				result += "float";
				break;
			case TypeKind::Int:
				result += "int";
				break;
			case TypeKind::Double:
				result += "double";
				break;
			case TypeKind::Long:
				result += "long";
				break;
			case TypeKind::String:
				result += "string";
				break;
			default:
				result += "unk";
				break;
			}
		} break;
		}

		return result;
	}

} // namespace tea
