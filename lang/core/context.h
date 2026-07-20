#pragma once

#include <cstdio>
#include <cstdint>
#include <stdexcept>

#include "core/tea.h"
#include "core/map.h"
#include "core/Type.h"
#include "core/string.h"
#include "core/vector.h"

#define TEA_NO_SOURCELOC { tea::badid, 0, 0 }

namespace tea {
	struct SourceLoc {
		uint32_t fsrc, line, column;
	};

	enum class DiagSeverity : uint32_t { Note, Warning, Error, Fatal };
	struct Diagnostic {
		SourceLoc loc;
		uint32_t ecode : 30;
		DiagSeverity sev : 2;

		tea::string emsg;
	};

	class SourceManager {
	public:
		uint32_t load(const tea::string& path);
		uint32_t loadVirtual(const tea::string& path, tea::string contents);

		const tea::string& pathof(uint32_t id) const { return files[id].first; }
		const tea::string& contentsof(uint32_t id) const { return files[id].second; }

	private:
		tea::vector<std::pair<tea::string, tea::string>> files;
	};

	class Diagnostics {
	public:
		bool hasError = false; // :(
		const SourceManager& sources;
		tea::vector<Diagnostic> diags;

		explicit Diagnostics(const SourceManager& sources) : sources(sources) {}

		template <typename... Args> void note(SourceLoc loc, tea::string msg, Args&&... args) {
			addFormatted(loc, 0, msg, DiagSeverity::Note, std::forward<Args>(args)...);
		}
		template <typename... Args> void warn(SourceLoc loc, tea::string msg, Args&&... args) {
			addFormatted(loc, 0, msg, DiagSeverity::Warning, std::forward<Args>(args)...);
		}
		template <typename... Args> void error(SourceLoc loc, uint32_t ecode, tea::string msg, Args&&... args) {
			hasError = true;
			addFormatted(loc, ecode, msg, DiagSeverity::Error, std::forward<Args>(args)...);
		}
		template <typename... Args> void fatal(SourceLoc loc, uint32_t ecode, tea::string msg, Args&&... args) {
			hasError = true;
			addFormatted(loc, ecode, msg, DiagSeverity::Fatal, std::forward<Args>(args)...);
			throw std::exception();
		}

		void print() const;

	private:
		void addFormatted(const SourceLoc& loc, uint32_t ecode, const tea::string& msg, const DiagSeverity sev, ...);
	};

	struct Context {
		Context() : diag(sources) {}

		TypeTable types;
		Diagnostics diag;
		SourceManager sources;

		tea::vector<tea::string> importLookup;
	};
}
