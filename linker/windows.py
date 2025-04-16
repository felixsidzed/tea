import os
import sys
import shlex
import subprocess

assert sys.platform == "win32", "Windows linker cannot be used on a non-win32 system"

DEFAULT_ARGS = "/entry:%s /out:%s /subsystem:console %s"


def defaultPanic(fmt, *args):
	print(fmt % args)
	exit(1)


def findLinker():
	vsDirs = [
		"C:\\Program Files (x86)\\Microsoft Visual Studio\\2022\\Community\\VC\\Tools\\MSVC",
		"C:\\Program Files\\Microsoft Visual Studio\\2022\\Community\\VC\\Tools\\MSVC",
	]

	for base in vsDirs:
		if os.path.exists(base):
			for version in os.listdir(base):
				linker = os.path.join(
					base, version, "bin", "Hostx86", "x86", "link.exe")
				if os.path.exists(linker):
					return linker
	return None

import re

def parseErrors(data: str, panic=defaultPanic):
	lines = data.splitlines()
	for line in lines:
		lnk2019 = re.match(r"(.*)\.(obj|o)\s*:\s*error LNK2019:\s*unresolved external symbol (.+?) referenced in function (.+)", line)
		if lnk2019:
			symbol = lnk2019.group(3)
			function = lnk2019.group(4)
			panic("[ERR] undefined symbol '%s' referenced in function '%s'", symbol, function)
			return

def link(
	*modules: tuple[str],
	linkerPath: str | None = None,
	entryPointName: str = "main",
	args: tuple[str, ...] = DEFAULT_ARGS,
	outputPath: str | None = None,
	verbose: bool = False,
	panic = defaultPanic
):
	linker = findLinker() if linkerPath is None else linkerPath
	if linker is None or not os.path.exists(linker):
		panic("unable to find linker")
		return

	formattedArgs = args % (entryPointName, outputPath, " ".join(modules))
	result = subprocess.run([linker, *shlex.split(formattedArgs)], capture_output=True, text=True)

	if result.returncode != 0:
		if verbose:
			print(result.stdout)
		else:
			parseErrors(result.stdout.replace("Microsoft (R) Incremental Linker Version 14.42.34436.0\nCopyright (C) Microsoft Corporation.  All rights reserved.\n", "").strip())
	return result.returncode
