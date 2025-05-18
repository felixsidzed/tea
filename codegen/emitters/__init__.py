import pkgutil
import importlib

__all__ = []

for loader, module_name, is_pkg in pkgutil.iter_modules(__path__):
	if module_name != "__init__":
		module = importlib.import_module(f".{module_name}", package=__name__)
		globals()[module_name] = module
		__all__.append(module_name)
