import os
import importlib

__all__ = []

for file in os.listdir(os.path.dirname(__file__)):
	if file.endswith(".py") and file != "__init__.py":
		name = file[:-3]
		module = importlib.import_module(f"{__name__}.{name}")
		globals()[name] = module
		__all__.append(name)
