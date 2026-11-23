#include "Clang/SyntaxTree.h"

SyntaxNode* SyntaxTree::add(SyntaxNode* parent, std::string const& type)
{
	std::unique_ptr<SyntaxNode> n = std::make_unique<SyntaxNode>();
	n->node.type = type;
	n->parent = parent;

	if (parent)
	{
		n->node.parent = &parent->node;
		parent->children.push_back(n.get());
	}

	all.push_back(std::move(n));

	return all.back().get();
}

std::vector<std::unique_ptr<SyntaxNode>> const& SyntaxTree::nodes() const
{
	return all;
}
