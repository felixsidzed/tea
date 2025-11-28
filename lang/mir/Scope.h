#pragma once

#include <string>
#include <memory>

#include "common/map.h"
#include "common/string.h"
#include "common/vector.h"

namespace tea::mir {

    class Scope {
    public:
        Scope() {
            used.emplace(new tea::string(""));
        }

        ~Scope() {
            for (const auto& s : used)
                delete s;
        }

        bool isUsed(const tea::string& name) const {
            for (const auto& s : used)
                if (*s == name)
                    return true;
            return false;
        }

        const char* add(const tea::string& name, bool dedup = true) {
            if (dedup) {
                if (!isUsed(name)) {
                    tea::string* s = new tea::string(name);
                    used.emplace(s);
                    return s->data();
                }

                tea::string base = name;
                tea::string candidate = base;
                int counter = 1;
                do
                    candidate = base + "." + std::to_string(counter++).c_str();
                while (isUsed(candidate));

                tea::string* s = new tea::string(candidate);
                used.emplace(s);
                return s->data();
            } else {
                if (isUsed(name))
                    return nullptr;

                tea::string* s = new tea::string(name);
                used.emplace(s);
                return s->data();
            }
        }

    private:
        tea::vector<tea::string*> used;
    };

} // namespace tea::mir
