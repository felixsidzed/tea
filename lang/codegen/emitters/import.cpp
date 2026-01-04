#include "codegen/codegen.h"

#include <fstream>
#include <filesystem>

#include "common/tea.h"
#include "frontend/lexer/Lexer.h"
#include "frontend/parser/Parser.h"

namespace fs = std::filesystem;

namespace tea {

	mir::Function* CodeGen::emitFunctionImport(const AST::FunctionImportNode* node) {
		tea::vector<Type*> params;
		for (const auto& [ty, _] : node->params)
			params.emplace(ty);

		mir::Function* func = module->addFunction(node->name, Type::Function(node->returnType, params, node->vararg));
		func->storage = AST::StorageClass::Public;
		func->subclassData = node->extra;
		return func;
	}

	void CodeGen::emitModuleImport(const AST::ModuleImportNode* node) {
		std::string fullModuleName(node->path + ".itea");

		fs::path path;
		std::ifstream file;
		for (const auto& lookup : importLookup) {
			path = fs::path(lookup) / fullModuleName;
			file.open(path);
			if (file.is_open())
				goto cont;
		}

		if (!file.is_open())
			TEA_PANIC("Failed to import module '%s': failed to open file", node->path);

	cont:
		std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());

		const auto& tokens = tea::frontend::lex({ content.data(), content.size() });
		tea::frontend::Parser parser(tokens);
		tea::frontend::AST::Tree tree = parser.parse();
		const tea::string& moduleName = path.stem().string().c_str();

		ImportedModule importedModule;
		for (const auto& node_ : tree) {
			if (node_->kind == AST::NodeKind::FunctionImport) {
				AST::FunctionImportNode* fi = (AST::FunctionImportNode*)node_.get();
				tea::string unprefixed = fi->name;
				if (fi->hasAttribute(mir::FunctionAttribute::NoNamespace))
					hasNoNamespaceFunctions = true;

				if (!fi->hasAttribute(mir::FunctionAttribute::NoMangle))
					fi->name = std::format("{}_{}", moduleName, fi->name).c_str();
				importedModule[unprefixed] = emitFunctionImport(fi);
			} else
				TEA_PANIC("invalid root statement in module. line %d, column %d", node_->line, node_->column);
		}
		importedModules[std::hash<tea::string>()(moduleName)] = importedModule;

		file.close();
	}

} // namespace tea
