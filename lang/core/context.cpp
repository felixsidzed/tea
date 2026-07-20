
#include "core/context.h"

#include <cstdio>
#include <cstdarg>

namespace tea {
    uint32_t SourceManager::load(const tea::string& path) {
        FILE* file;
        fopen_s(&file, path.data(), "rb");
        if (!file) return tea::badid;

        fseek(file, 0, SEEK_END);
        long size = ftell(file);
        rewind(file);

        tea::string buf(size, 0);
        fread(buf.data(), 1, size, file);
        fclose(file);

        files.emplace(path, buf);
        return (uint32_t)(files.size - 1);
    }

    uint32_t SourceManager::loadVirtual(const tea::string& path, tea::string contents) {
        files.emplace(path, contents);
        return (uint32_t)(files.size - 1);
    }

    static const char* strsev(DiagSeverity s) {
        switch (s) {
        case DiagSeverity::Note: return "note";
        case DiagSeverity::Warning: return "warning";
        case DiagSeverity::Error: return "error";
        case DiagSeverity::Fatal: return "fatal error";
        default: return "";
        }
    }

    void Diagnostics::print() const {
        for (const auto& diag : diags) {
            if (diag.loc.fsrc != tea::badid) {
                fprintf(stderr, "%s:%u:%u: ", sources.pathof(diag.loc.fsrc).data(), diag.loc.line, diag.loc.column);
            }

            if (diag.sev == DiagSeverity::Error || diag.sev == DiagSeverity::Fatal)
                fprintf(stderr, "%s (E%04u): %s\n", strsev(diag.sev), diag.ecode, diag.emsg.data());
            else
                fprintf(stderr, "%s: %s\n", strsev(diag.sev), diag.emsg.data());

            // TODO: pithon's ~~~~^ thingy in errors
        }
    }

    void Diagnostics::addFormatted(const SourceLoc& loc, uint32_t ecode, const tea::string& msg, const DiagSeverity sev, ...) {
        va_list va;
        va_start(va, sev);

        va_list vacopy;
        va_copy(vacopy, va);
        int size = vsnprintf(nullptr, 0, msg.data(), vacopy);
        va_end(vacopy);

        if (size < 0) {
            va_end(va);
            diags.emplace(loc, ecode, sev, "");
        } else {
            char* buffer = new char[size + 1];
            vsnprintf(buffer, size + 1, msg.data(), va);
            diags.emplace(loc, ecode, sev, buffer);
            va_end(va);
        }
    }
}
