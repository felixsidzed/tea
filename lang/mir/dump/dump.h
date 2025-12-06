#pragma once

#include "mir/mir.h"

namespace tea::mir {
	/// <summary>
	/// Dump the provided module to stdout
	/// </summary>
	/// <param name="module">The module to dump</param>
	void dump(const tea::mir::Module* module);

	/// <summary>
	/// Dump the provided function to stdout
	/// </summary>
	/// <param name="func">The function to dump</param>
	void dump(const tea::mir::Function* func);

	/// <summary>
	/// Dump the provided instruction to stdout
	/// </summary>
	/// <param name="insn">The instruction to dump</param>
	void dump(const tea::mir::Instruction* insn);

	/// <summary>
	/// Dump the provided value to stdout
	/// </summary>
	/// <param name="value">The value to dump</param>
	void dump(const tea::mir::Value* value);

	/// <summary>
	/// Dump the provided struct to stdout
	/// </summary>
	/// <param name="ty">The struct to dump</param>
	void dump(const struct tea::StructType* ty);

	/// <summary>
	/// Dump the provided global to stdout
	/// </summary>
	/// <param name="ty">The global to dump</param>
	void dump(const Global* global);
}
