#include "type.h"

#include <string>
#include <functional>

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

	Type* TypeTable::primitive(TypeKind kind, bool constant, bool sign) {
		for (auto& type : types) {
			if (type->kind == kind && type->constant == constant && type->sign == sign)
				return type.get();
		}

		return allocate<Type>(kind, constant, sign);
	}

	PointerType* TypeTable::Pointer(Type* pointee, bool constant) {
		return allocate<PointerType>(pointee, constant);
	}

	FunctionType* TypeTable::Function(Type* returnType, const tea::vector<Type*>& params, bool vararg) {
		size_t hash = std::hash<Type*>()(returnType);
		for (auto* param : params)
			hash ^= std::hash<Type*>()(param) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
		hash ^= (size_t)vararg;

		if (auto it = funcTypes.find(hash))
			return it->get();

		auto type = std::make_unique<FunctionType>(returnType, params, vararg);
		auto result = type.get();

		funcTypes[hash] = std::move(type);

		return result;
	}

	ArrayType* TypeTable::Array(Type* elementType, uint32_t size, bool constant) {
		return allocate<ArrayType>(elementType, size, constant);
	}

	StructType* TypeTable::Struct(Type** body, uint32_t n, const char* name, bool packed) {
		size_t hash = std::hash<const char*>()(name);
		if (auto it = structTypes.find(hash))
			return it->get();

		auto type = std::make_unique<StructType>(body, n, name, packed);
		auto result = type.get();

		structTypes[hash] = std::move(type);

		return result;
	}

	Type* TypeTable::Void(bool constant) { return primitive(TypeKind::Void, constant); }
	Type* TypeTable::Bool(bool constant) { return primitive(TypeKind::Bool, constant); }
	Type* TypeTable::Float(bool constant) { return primitive(TypeKind::Float, constant); }
	Type* TypeTable::Double(bool constant) { return primitive(TypeKind::Double, constant); }
	Type* TypeTable::String(bool constant) { return primitive(TypeKind::String, constant); }
	Type* TypeTable::Int(bool constant, bool sign) { return primitive(TypeKind::Int, constant, sign); }
	Type* TypeTable::Long(bool constant, bool sign) { return primitive(TypeKind::Long, constant, sign); }
	Type* TypeTable::Char(bool constant, bool sign) { return primitive(TypeKind::Char, constant, sign); }
	Type* TypeTable::Short(bool constant, bool sign) { return primitive(TypeKind::Short, constant, sign); }

	static int fpRank(TypeKind k) {
		switch (k) {
		case TypeKind::Float:return 1;
		case TypeKind::Double: return 2;
		default: return 0;
		}
	}

	static inline bool notSpace(int ch) { return !isspace(ch); }
	Type* TypeTable::get(const tea::string& name) {
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

		if (s.rfind("func(", 0) == 0) {
			size_t firstClose = s.find(')');
			if (firstClose == std::string::npos || firstClose + 1 >= s.size() || s[firstClose + 1] != '(')
				return nullptr;

			size_t secondClose = s.find(')', firstClose + 2);
			if (secondClose == std::string::npos)
				return nullptr;

			std::string retName = s.substr(5, firstClose - 5);
			Type* ret = get({ retName.data(), (uint32_t)retName.size() });
			if (!ret)
				return nullptr;

			tea::vector<Type*> params;
			std::string_view src(s.data() + firstClose + 2, secondClose - firstClose - 2);
			size_t start = 0;

			while (start < src.size()) {
				size_t end = src.find(',', start);
				std::string_view tok = src.substr(start, end == std::string_view::npos ? std::string_view::npos : end - start);
				start = (end == std::string_view::npos) ? src.size() : end + 1;

				while (!tok.empty() && !notSpace(tok.front())) tok.remove_prefix(1);
				while (!tok.empty() && !notSpace(tok.back())) tok.remove_suffix(1);

				if (tok.empty())
					continue;

				Type* param = get({ tok.data(), (uint32_t)tok.size() });
				if (!param)
					return nullptr;

				params.push(param);
			}

			return Pointer(Function(ret, params));
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
			size_t start = 0;
			std::string_view src(s);
			while ((start = src.find_first_not_of(" \t\n\v\f\r", start)) != std::string_view::npos) {
				size_t end = src.find_first_of(" \t\n\v\f\r", start);
				tokens.emplace_back(src.substr(start, end - start));
				start = end;
			}
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
		Type* userType = nullptr;
		TypeKind kind = TypeKind::Void;

		for (const auto& tok : tokens) {
			if (tok == "const")
				constant = true;
			else if (tok == "unsigned")
				sign = false;
			else if (tok == "signed")
				sign = true;
			else if (tok != "*") {
				tea::string t = { tok.data(), (uint32_t)tok.size() };

				auto builtin = name2kind.find(t);
				if (builtin != name2kind.end())
					kind = builtin->second;
				else {
					auto user = usertypes.find(t);
					if (user == usertypes.end())
						return nullptr;
					userType = user->second;
				}
			}
		}

		Type* current = nullptr;

		if (userType) {
			auto* st = (StructType*)userType;
			size_t hash = 1469598103934665603ull;

			const char* c = st->name;
			while (*c)
				hash = (hash ^ (uint8_t)*c++) * 1099511628211ull;

			for (auto* t : st->body)
				hash = (hash ^ (uintptr_t)t) * 1099511628211ull;

			hash = (hash ^ (uint8_t)st->extra) * 1099511628211ull;

			if (auto it = structTypes.find(hash))
				current = it->get();
			else {
				auto ptr = std::make_unique<StructType>(st->body.data, st->body.size, st->name, st->extra);
				current = ptr.get();
				structTypes[hash] = std::move(ptr);
			}
		} else
			current = primitive(kind, constant, sign);

		for (size_t i = 0; i < tokens.size(); i++) {
			if (tokens[i] == "*") {
				bool constPtr = i + 1 < tokens.size() && tokens[i + 1] == "const";
				current = Pointer(current, constPtr);
			}
		}

		for (auto dim : dims)
			current = Array(current, (uint32_t)dim);

		return current;
	}

	bool Type::isFloat() const { return kind == TypeKind::Float || kind == TypeKind::Double; }
	bool Type::isIndexable() const { return kind == TypeKind::Array || kind == TypeKind::Pointer; }
	bool Type::isNumeric() const {
		return kind == TypeKind::Bool ||
			kind == TypeKind::Char ||
			kind == TypeKind::Short ||
			kind == TypeKind::Int ||
			kind == TypeKind::Long;
	}

	tea::string Type::str() const {
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
			if (constant) result = "const ";
			if (!sign) result = "unsigned ";

			switch (kind) {
			case TypeKind::Void: result += "void"; break;
			case TypeKind::Bool: result += "bool"; break;
			case TypeKind::Char: result += "char"; break;
			case TypeKind::Short: result += "short"; break;
			case TypeKind::Float: result += "float"; break;
			case TypeKind::Int: result += "int"; break;
			case TypeKind::Double: result += "double"; break;
			case TypeKind::Long: result += "long"; break;
			case TypeKind::String: result += "string"; break;
			default: result += "unk"; break;
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

		if (this == other || isNumeric() && other->isNumeric())
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
}
