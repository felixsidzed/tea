#pragma once

#include "common/string.h"
#include "common/vector.h"

namespace tea::frontend {
	enum class TokenKind {
		// Literals
		String, Int, Float, Double, Char,

		// Identifiers
		Identf, Keyword,

		// Parentheses
		Lpar, Rpar, Lbrac, Rbrac,

		// Punctuation
		Semicolon, Comma, Colon, Dot,

		// Symbols
		Scope, At, Assign, Star, Arrow, Tilde, Amp,

		// Arithmetical Operators
		Sub, Add, Div,

		// Logical Operators
		Not, And, Or,

		// Comparison Operators
		Eq, Neq, Lt, Gt, Le, Ge,

		// Bitwise Operators
		Bor, Bxor, Shr, Shl,

		// EOF
		EndOfFile
	};

	enum class KeywordKind {
		//Using, Import,
		//Macro,
		Public, Private,
		//If, Else, ElseIf, Do, While, For, Break, Continue,
		Func, Return, End,
		//Var,
		//StdCC, FastCC, CCC, AutoCC,
		//Class, New
	};
	
	struct Token {
		TokenKind kind;
		uint32_t extra; // extra data (keyword kind, if `kind` == TokenKind::Keyword)

		// text including length
		const tea::string text;

		// source location (line and column of the first character)
		uint32_t line, column;

		Token(TokenKind kind, uint32_t extra, const tea::string& text, uint32_t line, uint32_t column)
			: kind(kind), extra(extra), text(text), line(line), column(column) {
		}
	};

	/// <summary>
	/// Generates a `tea::vector` of tokens from the provided `source`
	/// </summary>
	/// <param name="source">The source code to tokeize</param>
	/// <returns>The generated vector of tokens</returns>
	tea::vector<Token> lex(const tea::string& source);

} // namespace tea::frontend
