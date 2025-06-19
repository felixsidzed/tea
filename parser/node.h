#pragma once

#include <vector>
#include <string>
#include <memory>

namespace tea {
	enum NodeType : uint8_t {
		NODE_UsingNode,
		NODE_FunctionNode,
	};

	struct Node {
		enum NodeType type = static_cast<enum NodeType>(0);

		Node() = default;
		virtual ~Node() = default;
	};

	typedef std::vector<std::unique_ptr<Node>> Tree;

	struct UsingNode : Node {
		std::string name;

		UsingNode(const std::string& name) : name(name) {}
	};

	struct FunctionNode : Node {
		Tree body;

		uint8_t storage;

		FunctionNode(uint8_t storage) : storage(storage) {};
	};
}
