#include "Format/Formatter.h"

#include "Clang/TranslationUnit.h"
#include "Format/LineDelta.h"
#include "Lua/Script.h"
#include "Rules/Design.h"
#include "Rules/Layout.h"
#include "Rules/Naming.h"
#include "System/Text.h"

#include <algorithm>
#include <cwchar>

static wchar_t const* g_SourceExtensions[] =
{
	L".c", L".cc", L".cpp", L".cxx", L".h", L".hh", L".hpp", L".hxx", L".inl", L".ixx"
};

static std::string label(Node const& node)
{
	if (node.type != "variable")
	{
		return node.type;
	}

	return node.attributes.at("storage");
}

static bool groupable(SyntaxNode const& n)
{
	std::string const& storage = n.node.attributes.at("storage");

	if (storage == "parameter" or storage == "template")
	{
		return false;
	}

	if (not n.parent)
	{
		return true;
	}

	std::string const& type = n.parent->node.type;

	return type == "statement" or type == "type" or type == "namespace";
}

static bool switchBlock(SyntaxNode const& n)
{
	if (n.node.type != "block" or not n.parent)
	{
		return false;
	}

	std::map<std::string, std::string>::const_iterator it = n.parent->node.attributes.find("keyword");

	return it != n.parent->node.attributes.end() and it->second == "switch";
}

static std::string joined(std::vector<std::string> const& words)
{
	std::string result;

	for (std::string const& w : words)
	{
		result += (result.empty() ? "" : " ") + w;
	}

	return result;
}

static int number(Properties const& props, char const* key)
{
	Properties::const_iterator it = props.find(key);

	return it == props.end() ? -1 : std::stoi(it->second[0]);
}

Formatter::Formatter(Toolchain const& tc, std::filesystem::path const& buildDir)
	: clangFormat(tc.clangFormat())
	, buildDir(buildDir)
{
}

bool Formatter::supports(std::filesystem::path const& file) const
{
	std::filesystem::path ext = file.extension();

	for (wchar_t const* e : g_SourceExtensions)
	{
		if (_wcsicmp(ext.c_str(), e) == 0)
		{
			return true;
		}
	}

	return false;
}

Formatter::~Formatter() = default;

FormatResult Formatter::format(std::filesystem::path const& file,
                              std::string const& text,
                              std::optional<std::string> const& base)
{
	std::filesystem::path abs = std::filesystem::absolute(file);
	CompileCommand cmd = command(abs);
	Cascade const* cascade = styles(abs);
	FormatResult r;
	std::vector<Edit> late;

	if (base and LineDelta(*base, text).empty())
	{
		r.text = text;
		return r;
	}

	r.text = reformat(abs, text, base);

	for (int pass = 0; pass < 3; pass++)
	{
		TranslationUnit tu(abs, r.text, cmd);

		if (not cascade)
		{
			return r;
		}

		SyntaxTree tree = tu.tree();
		SourceText src(r.text);
		std::vector<Edit> edits;
		r.diagnostics.clear();
		late.clear();
		check(abs, tree, *cascade, src, &edits, &late, &r.diagnostics);

		if (base)
		{
			LineDelta d(*base, r.text);
			d.filter(src, &edits);
			d.filter(src, &late);
			d.filter(&r.diagnostics);
		}

		if (edits.empty() or pass == 2)
		{
			r.text = src.apply(late);
			break;
		}

		r.text = reformat(abs, src.apply(edits), base);
	}

	std::stable_sort(r.diagnostics.begin(), r.diagnostics.end(), [](Diagnostic const& a, Diagnostic const& b)
	{
		return a.line != b.line ? a.line < b.line : a.column < b.column;
	});

	return r;
}

std::string Formatter::reformat(std::filesystem::path const& file,
                                std::string const& text,
                                std::optional<std::string> const& base) const
{
	if (not base)
	{
		return clangFormat.format(file, text);
	}

	LineDelta d(*base, text);

	if (d.empty())
	{
		return text;
	}

	return clangFormat.format(file, text, d.ranges());
}

void Formatter::check(std::filesystem::path const& file,
                      SyntaxTree const& tree,
                      Cascade const& cascade,
                      SourceText const& src,
                      std::vector<Edit>* edits,
                      std::vector<Edit>* late,
                      std::vector<Diagnostic>* diagnostics)
{
	RuleContext ctx(src, diagnostics, edits);

	for (std::unique_ptr<SyntaxNode> const& n : tree.nodes())
	{
		Properties props = cascade.compute(n->node);
		std::map<std::string, std::string> const& attrs = n->node.attributes;

		if (n->checkName)
		{
			Identifier id = {label(n->node), n->name, n->line, n->column};
			checkNaming(id, props, diagnostics);
		}

		if (n->checkName and n->node.type == "function" and attrs.at("scope") == "free")
		{
			FreeFunction f;
			f.name = n->name;
			f.line = n->line;
			f.column = n->column;
			f.owner = n->owner;
			f.staticInClassFile = attrs.at("linkage") == "internal" and attrs.at("file") == "class";
			f.oneClass = attrs.at("callers") == "one-class";
			f.path = file;
			checkHelper(f, props, diagnostics);
		}

		if (n->node.type == "block")
		{
			std::vector<Spacing> statements;

			for (SyntaxNode const* child : n->children)
			{
				Properties cp = cascade.compute(child->node);
				Spacing s;
				s.first = child->firstLine;
				s.last = child->lastLine;
				s.before = number(cp, "blank-before");
				s.after = number(cp, "blank-after");
				statements.push_back(s);
			}

			spaceStatements(src, statements, edits);
		}

		if (switchBlock(*n))
		{
			std::vector<CaseBody> bodies;

			for (std::size_t i = 0; i < n->children.size(); i++)
			{
				SyntaxNode const* child = n->children[i];

				if (child->node.type != "case" or child->bodyStart == 0)
				{
					continue;
				}

				Properties cp = cascade.compute(child->node);

				if (not cp.count("braces") or cp["braces"][0] != "insert")
				{
					continue;
				}

				CaseBody b = {child->bodyStart, child->bodyEnd, child->bodyBraced};

				for (std::size_t j = i + 1; j < n->children.size() and n->children[j]->node.type != "case"; j++)
				{
					b.end = n->children[j]->end;
				}

				bodies.push_back(b);
			}

			braceCases(src, bodies, edits);
		}

		if (props.count("report") and (n->name.empty() or n->checkName))
		{
			diagnostics->push_back({n->line, n->column, "report", joined(props["report"])});
		}

		if (n->node.type == "condition" and props.count("max-clauses"))
		{
			checkClauses(n->line, n->column, n->clauses, number(props, "max-clauses"), diagnostics);
		}

		if (n->node.type == "expression" and props.count("logical-operators"))
		{
			std::string const& mode = props["logical-operators"][0];
			convertOperator(src, n->operatorOffset, n->operatorSpelling, mode, edits);

			if (mode == "words")
			{
				spaceNot(src, n->operatorOffset, n->operatorSpelling, late);
			}
		}

		for (std::string const& path : props["script"])
		{
			script(utf8Path(path)).run(*n, &ctx);
		}
	}

	std::map<std::pair<SyntaxNode const*, std::size_t>, std::vector<SyntaxNode const*>> groups;

	for (std::unique_ptr<SyntaxNode> const& n : tree.nodes())
	{
		if (n->node.type != "variable" or not groupable(*n))
		{
			continue;
		}

		bool local = n->parent and n->parent->node.type == "statement";
		std::pair<SyntaxNode const*, std::size_t> key = local ? std::make_pair(n->parent, (std::size_t)0) : std::make_pair((SyntaxNode const*)nullptr, n->start);
		groups[key].push_back(n.get());
	}

	for (auto const& [key, group] : groups)
	{
		if (group.size() < 2)
		{
			continue;
		}

		Properties gp = cascade.compute(group[0]->node);

		if (not gp.count("declarators") or gp["declarators"][0] != "split")
		{
			continue;
		}

		std::vector<Declarator> ds;

		for (SyntaxNode const* v : group)
		{
			ds.push_back({group[0]->start, v->nameOffset, v->end, v->line, v->column});
		}

		splitDeclarators(src, ds, edits, diagnostics);
	}
}

Script& Formatter::script(std::filesystem::path const& file)
{
	std::unique_ptr<Script>& s = scripts[file];

	if (not s)
	{
		s = std::make_unique<Script>(file);
	}

	return *s;
}

Cascade const* Formatter::styles(std::filesystem::path const& file)
{
	std::filesystem::path dir = file.parent_path();
	std::unique_ptr<Cascade>& cascade = cascades[dir];

	if (cascade)
	{
		return cascade.get();
	}

	std::vector<std::filesystem::path> sheets;

	for (std::filesystem::path d = dir; not d.empty(); d = d.parent_path())
	{
		if (std::filesystem::exists(d / ".strictor"))
		{
			sheets.push_back(d / ".strictor");
		}

		if (d == d.parent_path())
		{
			break;
		}
	}

	if (sheets.empty())
	{
		return nullptr;
	}

	std::unique_ptr<Cascade> c = std::make_unique<Cascade>();

	for (std::size_t i = sheets.size(); i-- > 0;)
	{
		c->append(Stylesheet(sheets[i]));
	}

	cascade = std::move(c);

	return cascade.get();
}

CompileDatabase* Formatter::database(std::filesystem::path const& file)
{
	std::filesystem::path dir = buildDir;

	if (dir.empty())
	{
		for (std::filesystem::path d = file.parent_path(); not d.empty(); d = d.parent_path())
		{
			if (std::filesystem::exists(d / "compile_commands.json"))
			{
				dir = d;
				break;
			}

			if (d == d.parent_path())
			{
				break;
			}
		}
	}

	if (dir.empty())
	{
		return nullptr;
	}

	std::unique_ptr<CompileDatabase>& db = databases[dir];

	if (not db)
	{
		db = std::make_unique<CompileDatabase>(dir);
	}

	return db.get();
}

CompileCommand Formatter::command(std::filesystem::path const& file)
{
	CompileDatabase* db = database(file);

	if (db)
	{
		CompileCommand cc = db->command(file);

		if (not cc.args.empty())
		{
			return cc;
		}
	}

	CompileCommand cc;
	cc.dir = std::filesystem::current_path();

	if (_wcsicmp(file.extension().c_str(), L".c") == 0)
	{
		cc.args = {"-xc"};
	}
	else
	{
		cc.args = {"-xc++", "-std=c++2c"};
	}

	return cc;
}
