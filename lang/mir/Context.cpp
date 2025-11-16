#include "Context.h"

namespace tea::mir {

	Context* getGlobalContext() {
		static Context globalContext;
		return &globalContext;
	}

} // namespace tea
