import json

def parseJSON(module: str) -> dict:
	module: dict = json.loads(module)
	fmt: int = module["format"]
	parsed: dict = module.copy()
	for k, v in module["functions"].items():
		if fmt >= 3:
			parsed["functions"][k] = v
		else:
			return (None, "invalid format")
	return parsed
