#include "Type.h"

#include <sstream>
#include <unordered_map>

#include "common/vector.h"

#include "mir/Context.h"

namespace tea {
	static std::unordered_map<tea::string, TypeKind> name2kind = {
		{"void", TypeKind::Void},
		{"bool", TypeKind::Bool},
		{"char", TypeKind::Char},
		{"short", TypeKind::Short},
		{"int", TypeKind::Int},
		{"float", TypeKind::Float},
		{"long", TypeKind::Long},
		{"double", TypeKind::Double},

		{"string", TypeKind::String},
	};

	static std::unordered_map<tea::string, Type*> usertypes;

	static int fpRank(TypeKind k) {
		switch (k) {
		case TypeKind::Float:return 1;
		case TypeKind::Double: return 2;
		default: return 0;
		}
	}

	static inline bool notSpace(int ch) {
		return !isspace(ch);
	}

	Type* Type::get(const tea::string& name) {
		mir::Context* ctx = mir::getGlobalContext();

		std::string s(name.data(), name.length());

		s.erase(s.begin(), std::find_if(s.begin(), s.end(), notSpace));
		s.erase(std::find_if(s.rbegin(), s.rend(), notSpace).base(), s.end());

		{
			std::string out;
			bool prevSpace = false;
			for (char c : s) {
				if (std::isspace((unsigned char)c)) {
					if (!prevSpace) out.push_back(' ');
					prevSpace = true;
				}
				else {
					out.push_back(c);
					prevSpace = false;
				}
			}
			s.swap(out);
		}

		if (!s.rfind("func(", 0)) {
			size_t firstClose = s.find(')');
			if (firstClose == std::string::npos || firstClose + 1 >= s.size() || s[firstClose + 1] != '(')
				return nullptr;

			size_t secondClose = s.find(')', firstClose + 2);
			if (secondClose == std::string::npos)
				return nullptr;

			std::string retTypeName = s.substr(5, firstClose - 5);
			std::string params = s.substr(firstClose + 2, secondClose - (firstClose + 2));

			Type* retType = Type::get({ retTypeName.data(), (uint32_t)retTypeName.size() });
			if (!retType)
				return nullptr;

			bool vararg = false;
			tea::vector<Type*> argTypes;

			{
				std::stringstream ss(params);
				std::string tok;
				while (std::getline(ss, tok, ',')) {
					s.erase(s.begin(), std::find_if(s.begin(), s.end(), notSpace));
					s.erase(std::find_if(s.rbegin(), s.rend(), notSpace).base(), s.end());

					if (tok.empty())
						continue;

					Type* arg = Type::get({ tok.data(), (uint32_t)tok.size() });
					if (!arg)
						return nullptr;
					argTypes.push(arg);
				}
			}

			return Type::Pointer(Type::Function(retType, argTypes, vararg));
		}

		for (char sym : {'*', '[', ']', '(', ')', ','}) {
			std::string r;
			for (char c : s) {
				if (c == sym) {
					r.push_back(' ');
					r.push_back(c);
					r.push_back(' ');
				} else
					r.push_back(c);
			}
			s.swap(r);
		}

		std::vector<std::string> tokens; {
			std::stringstream ss(s);
			std::string tok;
			while (ss >> tok)
				tokens.push_back(tok);
		}

		tea::vector<size_t> dims;
		for (size_t i = 0; i + 2 < tokens.size();) {
			if (tokens[i] == "[" && tokens[i + 2] == "]") {
				dims.push(std::stoull(tokens[i + 1]));
				tokens.erase(tokens.begin() + i, tokens.begin() + i + 3);
			} else
				i++;
		}

		bool sign = true;
		bool constant = false;
		Type* userBaseType = nullptr;
		TypeKind kind = TypeKind::Void;

		for (size_t i = 0; i < tokens.size(); i++) {
			const auto& tok = tokens[i];

			if (tok == "const")
				constant = true;
			else if (tok == "unsigned")
				sign = false;
			else if (tok == "signed")
				sign = true;
			else if (tok != "*") {
				tea::string t{ tok.data(), tok.size() };

				if (auto it = name2kind.find(t); it != name2kind.end())
					kind = it->second;
				else if (auto it = usertypes.find(t); it != usertypes.end())
					userBaseType = it->second;
				else
					return nullptr;
			}
		}

		Type* current = nullptr;
		if (userBaseType) {
			StructType* st = (StructType*)userBaseType;

			const char* c = st->name;
			size_t h = 1469598103934665603ull;
			while (*c)
				h = (h ^ (uint8_t)*c++) * 1099511628211ull;
			for (const auto& el : st->body)
				h = (h ^ (uintptr_t)el) * 1099511628211ull;
			h = (h ^ (uint8_t)st->extra) * 1099511628211ull;

			auto& entry = ctx->structTypes[h];
			if (!entry)
				entry.reset(new StructType(st->body.data, st->body.size, st->name, st->extra, constant));
			current = entry.get();
		} else
			current = ctx->getType(kind, constant, sign);

		for (size_t i = 0; i < tokens.size(); ++i) {
			if (tokens[i] == "*") {
				bool constp = false;
				if (i + 1 < tokens.size() && tokens[i + 1] == "const")
					constp = true;

				current = ctx->getType<PointerType>(current, constp);
			}
		}

		for (const auto& dim : dims)
			current = ctx->getType<ArrayType>(current, (uint32_t)dim);

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
		size_t h = 1469598103934665603ull;
		h = (h ^ (uintptr_t)returnType) * 1099511628211ull;
		for (const auto& param : params)
			h = (h ^ (uintptr_t)param) * 1099511628211ull;
		h = (h ^ (size_t)vararg) * 1099511628211ull;

		auto& entry = mir::getGlobalContext()->funcTypes[h];
		if (!entry)
			entry.reset(new FunctionType(returnType, params, vararg));
		return entry.get();
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
			h = (h ^ (uintptr_t)body[i]) * 1099511628211ull;
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
				result += "func(";
				result += ftype->returnType->str().data();
				result += ')'; result += '(';
			}
			else
				result += "void)(";

			for (uint32_t i = 0; i < ftype->params.size; ++i) {
				if (i > 0) result += ", ";
				result += ftype->params[i]->str().data();
			}

			if (ftype->extra) {
				if (ftype->params.size > 0)
					result += ", ";
				result += "...";
			}

			result += ')';
		} break;

		case TypeKind::Array: {
			ArrayType* arr = (ArrayType*)this;
			result += arr->elementType->str() + "[" + std::to_string(arr->extra).c_str() + "]";
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

	Type* Type::getElementType() const {
		switch (kind) {
		case TypeKind::Array:
			return ((ArrayType*)this)->elementType;
		case TypeKind::Pointer:
			return ((PointerType*)this)->pointee;
		default:
			return nullptr;
		}
	}

	bool Type::equals(const Type* other) const {
		if (!other)
			return false;

		if (this == other)
			return true;

		if (isNumeric() && other->isNumeric())
			return true;

		if (isFloat() && other->isFloat())
			return fpRank(kind) <= fpRank(other->kind);

		if (kind == TypeKind::Pointer && other->kind == TypeKind::Pointer) {
			if (!constant)
				return getElementType()->equals(other->getElementType());
			return false;
		}

		if (kind == TypeKind::Array && other->kind == TypeKind::Array) {
			ArrayType* b = (ArrayType*)other;
			return extra == b->extra && getElementType()->equals(b->getElementType());
		}

		if (kind == TypeKind::Function && other->kind == TypeKind::Function) {
			FunctionType* a = (FunctionType*)this;
			FunctionType* b = (FunctionType*)other;
			if (!a->returnType->equals(b))
				return false;

			if (a->params.size != b->params.size)
				return false;

			for (uint32_t i = 0; i < a->params.size; i++)
				if (!a->params[i]->equals(b->params[i]))
					return false;
			return a->extra == b->extra;
		}

		if (kind == TypeKind::Struct && other->kind == TypeKind::Struct)
			return false;
		return kind == other->kind;
	}

	void Type::create(const tea::string& name, Type* baseType) {
		if (name.empty() || !baseType)
			return;

		auto it = usertypes.find(name);
		if (it == usertypes.end())
			usertypes[name] = baseType;
	}

} // namespace tea
