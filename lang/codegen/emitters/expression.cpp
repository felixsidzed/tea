#include "codegen/codegen.h"

#include "common/map.h"
#include "common/tea.h"

#include "frontend/lexer/Lexer.h"

namespace tea {

	extern mir::Value* expr2bool(mir::Builder* builder, mir::Module* module, mir::Value* pred);

	mir::Value* CodeGen::emitExpression(const AST::ExpressionNode* node, EmissionFlags flags, bool* asRef) {
		switch (node->getEKind()) {
		case AST::ExprKind::String: {
			const AST::LiteralNode* literal = (const AST::LiteralNode*)node;
			if (flags.has(EmissionFlags::Constant)) {
				mir::ConstantString* str = mir::ConstantString::get(literal->value);
				return module->addGlobal("", str->type, str);
			} else
				return builder.globalString(literal->value);
		}

		case AST::ExprKind::Char: {
			const AST::LiteralNode* literal = (const AST::LiteralNode*)node;
			return mir::ConstantNumber::get(literal->value[0], 8);
		}

		case AST::ExprKind::Int: {
			const AST::LiteralNode* literal = (const AST::LiteralNode*)node;
			uint64_t val = std::stoull(std::string(literal->value.data(), literal->value.length()), nullptr, 0);
			return mir::ConstantNumber::get(val, 32, node->type->sign);
		}

		case AST::ExprKind::Double:
		case AST::ExprKind::Float: {
			const AST::LiteralNode* literal = (const AST::LiteralNode*)node;
			return mir::ConstantNumber::get<double>(
				std::stod(std::string(literal->value.data(), literal->value.length()), nullptr),
				node->getEKind() == AST::ExprKind::Float ? 32 : 64,
				node->type->sign
			);
		}

		case AST::ExprKind::Identf: {
			const AST::LiteralNode* literal = (const AST::LiteralNode*)node;

			if (strchr(literal->value, ':')) {
				bool first = true;
				std::string str(literal->value.data(), literal->value.length());
				tea::vector<tea::string> parts;

				size_t start = 0, pos = 0;
				while ((pos = str.find("::", start)) != std::string::npos) {
					parts.emplace(str.data() + start, pos - start);
					start = pos + 2;
				}
				parts.emplace(str.data() + start, str.length() - start);

				for (uint32_t i = 0; i < parts.size; i++) {
					const tea::string& part = parts[i];

					if (!first)
						TEA_PANIC("deep scopes are not yet implemented. line %d, column %d", node->line, node->column);
					else {
						if (auto* modIt = importedModules.find(part)) {
							if (i + 1 < parts.size) {
								const tea::string& next = parts[i + 1];
								if (auto* it = modIt->find(next))
									return *it;
								else
									TEA_PANIC("'%s' is not a valid member of module '%s'. line %d, column %d", next.data(), part.data(), node->line, node->column);
							}
						} else
							TEA_PANIC("'%s' does not reference a valid scope. line %d, column %d", part.data(), node->line, node->column);
						first = false;
					}
				}
			} else {
				if (literal->value == "true")
					return mir::ConstantNumber::get(1, 1);
				else if (literal->value == "false")
					return mir::ConstantNumber::get(0, 1);
				else if (literal->value == "null")
					return mir::ConstantPointer::get(Type::Void(), 0);

				{
					mir::Function* f = module->getNamedFunction(literal->value);
					if (f) {
						if (!flags.has(EmissionFlags::AllowInlinedFunctions) && f->hasAttribute(mir::FunctionAttribute::Inline))
							TEA_PANIC("ur function grew legs and escaped. line %d, column %d", literal->line, literal->column);
						return f;
					}
				}

				if (flags.has(EmissionFlags::Constant))
					TEA_PANIC("value is not a constant expression. line %d, column %d", literal->line, literal->column);

				{
					mir::Global* g = module->getNamedGlobal(literal->value);
					if (g) {
						if (asRef) {
							*asRef = true;
							return g;
						} else {
							mir::Value* value = nullptr;
							if (g->type->kind == TypeKind::Array) {
								mir::ConstantNumber* zero = mir::ConstantNumber::get(0, 32);
								mir::Value* idx[] = { zero, zero };
								value = builder.gep(g, idx, 2, "");
							} else
								value = builder.load(g, "");

							return value;
						}
					}
				} if (curParams) {
					uint32_t i = 0;
					for (const auto& [_, name] : *curParams) {
						if (literal->value == name)
							return builder.getInsertBlock()->parent->getParam(i);
						i++;
					}
				} {
					if (auto* it = locals.find(std::hash<tea::string>()(literal->value))) {
						if (asRef) {
							*asRef = true;
							return it->allocated;
						} else {
							if (!it->load) {
								if (it->allocated->type->getElementType()->kind == TypeKind::Array) {
									mir::ConstantNumber* zero = mir::ConstantNumber::get(0, 32);
									mir::Value* idx[] = { zero, zero };
									it->load = builder.gep(it->allocated, idx, 2, "");
								} else
									it->load = builder.load(it->allocated, literal->value.data());

							}
							return it->load;
						}
					}
				}
			}

			TEA_PANIC("use of undefined symbol '%s'. line %d, column %d", literal->value.data(), literal->line, literal->column);
		} break;

		case AST::ExprKind::Call: {
			if (flags.has(EmissionFlags::Constant))
				TEA_PANIC("value is not a constant expression. line %d, column %d", node->line, node->column);

			const AST::CallNode* call = (const AST::CallNode*)node;
			mir::Value* callee = emitExpression(call->callee.get(), EmissionFlags::AllowInlinedFunctions);

			tea::vector<mir::Value*> args;
			for (const auto& arg : call->args)
				args.emplace(emitExpression(arg.get()));

			// TODO: improve // <---- maybe not
			if (callee->kind == mir::ValueKind::Function && ((mir::Function*)callee)->hasAttribute(mir::FunctionAttribute::Inline)) {
				mir::Function* func = (mir::Function*)callee;
				mir::Function* curFunc = builder.getInsertBlock()->parent;

				mir::Value* retVal = nullptr;
				tea::map<const mir::Value*, mir::Value*> valueMap;
				tea::map<const mir::BasicBlock*, mir::BasicBlock*> blockMap;

				for (uint32_t i = 0; i < func->params.size; i++)
					valueMap[func->params[i].get()] = args[i];

				for (const auto& block : func->blocks)
					blockMap[block.get()] = curFunc->appendBlock(tea::string("inlined.") + block->name);
				builder.br(blockMap.data.data[0].second);

				for (const auto& block : func->blocks) {
					mir::BasicBlock* bb = blockMap[block.get()];
					bb->body.reserve(block->body.size);

					for (const auto& insn : block->body) {
						mir::Instruction cloned;
						cloned.op = insn.op;
						cloned.extra = insn.extra;
						cloned.operands = insn.operands;

						for (uint32_t i = 0; i < cloned.operands.size; i++) {
							mir::Value* old = cloned.operands[i];

							if (old->kind == mir::ValueKind::Parameter || old->kind == mir::ValueKind::Instruction)
								cloned.operands[i] = valueMap[old];
						}

						if (insn.result && insn.result->kind != mir::ValueKind::Null) {
							auto clonedRes = std::make_unique<mir::Value>(insn.result->kind, insn.result->type);
							clonedRes->name = insn.result->name;
							valueMap[insn.result.get()] = clonedRes.get();
							cloned.result = std::move(clonedRes);
						}

						bb->body.push(std::move(cloned));
					}
				}

				for (const auto& block : func->blocks) {
					mir::BasicBlock* bb = blockMap[block.get()];

					mir::Instruction* term = &bb->body.data[bb->body.size - 1];
					switch (term->op) {
					case mir::OpCode::Br: {
						term->operands[0] = (mir::Value*)blockMap[(const mir::BasicBlock*)term->operands[0]];
						break;
					}
					case mir::OpCode::CondBr: {
						term->operands[1] = (mir::Value*)blockMap[(const mir::BasicBlock*)term->operands[1]];
						term->operands[2] = (mir::Value*)blockMap[(const mir::BasicBlock*)term->operands[2]];
						break;
					}
					case mir::OpCode::Ret: {
						if (!term->operands.empty()) {
							// the hope is that if the return value isnt mapped then its a global/constant
							if (auto* it = valueMap.find(term->operands[0])) retVal = *it;
							else retVal = term->operands[0];
						}

						term->op = mir::OpCode::Nop;
						term->operands.clear();
						break;
					}
					default:
						break;
					}
				}

				return retVal;

			} else {
				mir::Value* call = builder.call(callee, args.data, args.size, "");
				if (callee->kind == mir::ValueKind::Function && ((mir::Function*)callee)->hasAttribute(mir::FunctionAttribute::Inline))
					builder.unreachable();
				return call;
			}
		} break;

		case AST::ExprKind::Add:
		case AST::ExprKind::Sub:
		case AST::ExprKind::Mul:
		case AST::ExprKind::Div: {
			const AST::BinaryExprNode* be = (const AST::BinaryExprNode*)node;

			mir::Value* lhs = emitExpression(be->lhs.get());
			mir::Value* rhs = emitExpression(be->rhs.get());

			if (flags.has(EmissionFlags::Constant) && (lhs->kind != mir::ValueKind::Constant || rhs->kind != mir::ValueKind::Constant))
				TEA_PANIC("value is not a constant expression. line %d, column %d", node->line, node->column);

			switch (node->getEKind()) {
			case AST::ExprKind::Add:
				return builder.arithm(mir::OpCode::Add, lhs, rhs, "");
			case AST::ExprKind::Sub:
				return builder.arithm(mir::OpCode::Sub, lhs, rhs, "");
			case AST::ExprKind::Mul:
				return builder.arithm(mir::OpCode::Mul, lhs, rhs, "");
			case AST::ExprKind::Div:
				return builder.arithm(mir::OpCode::Div, lhs, rhs, "");
			default:
				break;
			}
		} break;

		case AST::ExprKind::Eq:
		case AST::ExprKind::Neq:
		case AST::ExprKind::Lt:
		case AST::ExprKind::Le:
		case AST::ExprKind::Gt:
		case AST::ExprKind::Ge: {
			const AST::BinaryExprNode* be = (const AST::BinaryExprNode*)node;

			mir::Value* lhs = emitExpression(be->lhs.get());
			mir::Value* rhs = emitExpression(be->rhs.get());

			if (flags.has(EmissionFlags::Constant) && (lhs->kind != mir::ValueKind::Constant || rhs->kind != mir::ValueKind::Constant))
				TEA_PANIC("value is not a constant expression. line %d, column %d", node->line, node->column);

			if (lhs->type->isFloat() || rhs->type->isFloat()) {
				mir::FCmpPredicate pred;
				switch (node->getEKind()) {
				case AST::ExprKind::Eq:  pred = mir::FCmpPredicate::OEQ; break;
				case AST::ExprKind::Neq: pred = mir::FCmpPredicate::ONEQ; break;
				case AST::ExprKind::Lt:  pred = mir::FCmpPredicate::OLT; break;
				case AST::ExprKind::Le:  pred = mir::FCmpPredicate::OLE; break;
				case AST::ExprKind::Gt:  pred = mir::FCmpPredicate::OGT; break;
				case AST::ExprKind::Ge:  pred = mir::FCmpPredicate::OGE; break;
				default: TEA_UNREACHABLE();
				}
				return builder.fcmp(pred, lhs, rhs, "");
			} else {
				mir::ICmpPredicate pred;
				switch (node->getEKind()) {
				case AST::ExprKind::Eq:  pred = mir::ICmpPredicate::EQ; break;
				case AST::ExprKind::Neq: pred = mir::ICmpPredicate::NEQ; break;
				case AST::ExprKind::Lt:  pred = node->type->sign ? mir::ICmpPredicate::SLT : mir::ICmpPredicate::ULT; break;
				case AST::ExprKind::Le:  pred = node->type->sign ? mir::ICmpPredicate::SLE : mir::ICmpPredicate::ULE; break;
				case AST::ExprKind::Gt:  pred = node->type->sign ? mir::ICmpPredicate::SGT : mir::ICmpPredicate::UGT; break;
				case AST::ExprKind::Ge:  pred = node->type->sign ? mir::ICmpPredicate::SGE : mir::ICmpPredicate::UGE; break;
				default: TEA_UNREACHABLE();
				}
				return builder.icmp(pred, lhs, rhs, "");
			}
		} break;

		case AST::ExprKind::Ref: {
			bool isRef = false;
			mir::Value* ref = emitExpression(((AST::UnaryExprNode*)node)->value.get(), EmissionFlags::None, &isRef);
			if (!isRef)
				TEA_PANIC("cannot reference value. line %d, column %d", node->line, node->column);

			return ref;
		} break;
		case AST::ExprKind::Deref:
			return builder.load(emitExpression(((AST::UnaryExprNode*)node)->value.get()), "");
		
		case AST::ExprKind::Not:
			return builder.binop(mir::OpCode::Not, expr2bool(&builder, module.get(), emitExpression(((AST::UnaryExprNode*)node)->value.get())), nullptr, "");
		case AST::ExprKind::And:
			return builder.binop(mir::OpCode::And,
				emitExpression(((AST::BinaryExprNode*)node)->lhs.get()),
				emitExpression(((AST::BinaryExprNode*)node)->rhs.get()),
				""
			);
		case AST::ExprKind::Or:
			return builder.binop(mir::OpCode::Or,
				emitExpression(((AST::BinaryExprNode*)node)->lhs.get()),
				emitExpression(((AST::BinaryExprNode*)node)->rhs.get()),
				""
			);

		case AST::ExprKind::Cast:
			return builder.cast(emitExpression(((AST::UnaryExprNode*)node)->value.get()), node->type, "");

		case AST::ExprKind::Index: {
			AST::BinaryExprNode* be = (AST::BinaryExprNode*)node;

			tea::vector<mir::Value*> indices;
			AST::ExpressionNode* lhs = be->lhs.get();

			while (lhs->getEKind() == AST::ExprKind::Index) {
				auto* nested = (AST::BinaryExprNode*)lhs;
				indices.emplace(emitExpression(nested->rhs.get()));
				lhs = nested->lhs.get();
			}

			mir::Value* base = emitExpression(lhs);
			indices.emplace(emitExpression(be->rhs.get()));

			std::reverse(indices.begin(), indices.end());

			mir::Value* el = builder.gep(base, indices.data, indices.size, "");
			if (asRef) {
				*asRef = true;
				return el;
			}
			return builder.load(el, "");
		} break;

		case AST::ExprKind::Array: {
			AST::ArrayNode* arr = (AST::ArrayNode*)node;

			tea::vector<mir::Value*> values;
			for (const auto& val : arr->values)
				values.emplace(emitExpression(val.get()));

			return mir::ConstantArray::get(arr->type->getElementType(), values.data, values.size);
		} break;

		default:
			TEA_PANIC("unknown expression kind %d", node->getEKind());
			TEA_UNREACHABLE();
		}

		return nullptr;
	}

} // namespace tea
