import os
import sys

import lark

from parser.ast import AST

MODULE_LOOKUP = [
	"stdlib",
	"."
]


def resource(rel):
	return os.path.join(getattr(sys, "_MEIPASS", os.path.abspath(".")), rel)


class Parser:
	def __init__(self) -> None:
		grammar: str = ""
		with open(resource("parser/grammar.lark"), "r") as f:
			grammar = f.read()
			f.close()
		self.lark = lark.Lark(grammar, propagate_positions=True, start="module")

	def parse(self, src: str, filename: str | None = None):
		return AST(filename=filename).transform(self.lark.parse(src))
