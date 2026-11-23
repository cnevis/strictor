#include "Clang/TranslationUnit.h"

#include "Clang/ClangString.h"
#include "System/Text.h"

#include <cwchar>
#include <format>
#include <map>
#include <set>
#include <stdexcept>

static char const* nodeType(CXCursorKind k)
{
	switch (k)
	{
		case CXCursor_ClassDecl:
		case CXCursor_StructDecl:
		case CXCursor_UnionDecl:
		case CXCursor_EnumDecl:
		case CXCursor_TypedefDecl:
		case CXCursor_TypeAliasDecl:
		case CXCursor_TypeAliasTemplateDecl:
		case CXCursor_ClassTemplate:
		case CXCursor_TemplateTypeParameter:
		case CXCursor_TemplateTemplateParameter:
		case CXCursor_ConceptDecl:
		{
			return "type";
		}

		case CXCursor_FunctionDecl:
		case CXCursor_CXXMethod:
		case CXCursor_FunctionTemplate:
		{
			return "function";
		}

		case CXCursor_VarDecl:
		case CXCursor_ParmDecl:
		case CXCursor_FieldDecl:
		case CXCursor_NonTypeTemplateParameter:
		{
			return "variable";
		}

		case CXCursor_EnumConstantDecl:
		{
			return "enumerator";
		}

		case CXCursor_MacroDefinition:
		{
			return "macro";
		}

		case CXCursor_Namespace:
		{
			return "namespace";
		}

		default:
		{
			return nullptr;
		}
	}
}

static char const* storage(CXCursor c)
{
	CXCursorKind k = clang_getCursorKind(c);

	if (k == CXCursor_NonTypeTemplateParameter)
	{
		return "template";
	}

	if (k == CXCursor_ParmDecl)
	{
		return "parameter";
	}

	if (k == CXCursor_FieldDecl)
	{
		return "member";
	}

	switch (clang_getCursorKind(clang_getCursorSemanticParent(c)))
	{
		case CXCursor_ClassDecl:
		case CXCursor_StructDecl:
		case CXCursor_UnionDecl:
		case CXCursor_ClassTemplate:
		case CXCursor_ClassTemplatePartialSpecialization:
		{
			return "member";
		}

		case CXCursor_Namespace:
		case CXCursor_TranslationUnit:
		case CXCursor_LinkageSpec:
		{
			return "global";
		}

		default:
		{
			return "local";
		}
	}
}

static char const* access(CXCursor c)
{
	switch (clang_getCursorKind(c))
	{
		case CXCursor_TemplateTypeParameter:
		case CXCursor_NonTypeTemplateParameter:
		case CXCursor_TemplateTemplateParameter:
		{
			return nullptr;
		}

		default:
		{
			break;
		}
	}

	switch (clang_getCXXAccessSpecifier(c))
	{
		case CX_CXXPublic:
		{
			return "public";
		}

		case CX_CXXProtected:
		{
			return "protected";
		}

		case CX_CXXPrivate:
		{
			return "private";
		}

		default:
		{
			return nullptr;
		}
	}
}

static char const* constness(CXTranslationUnit unit, CXCursor c, std::string const& name)
{
	CXToken* tokens = nullptr;
	unsigned n = 0;
	char const* result = clang_isConstQualifiedType(clang_getCursorType(c)) ? "const" : "none";
	clang_tokenize(unit, clang_getCursorExtent(c), &tokens, &n);

	for (unsigned i = 0; i < n; i++)
	{
		std::string spelling = takeString(clang_getTokenSpelling(unit, tokens[i]));

		if (spelling == name)
		{
			break;
		}

		if (spelling == "constexpr" or spelling == "constinit")
		{
			result = "constexpr";
			break;
		}
	}

	clang_disposeTokens(unit, tokens, n);

	return result;
}

static bool exempt(CXCursor c, std::string const& name)
{
	if (name.empty() or name.starts_with("operator"))
	{
		return true;
	}

	switch (clang_getCursorKind(c))
	{
		case CXCursor_ClassDecl:
		case CXCursor_StructDecl:
		case CXCursor_UnionDecl:
		case CXCursor_EnumDecl:
		case CXCursor_ClassTemplate:
		{
			return not clang_isCursorDefinition(c);
		}

		default:
		{
			break;
		}
	}

	if (clang_isDeclaration(clang_getCursorKind(c)) and not clang_equalCursors(clang_getCanonicalCursor(c), c))
	{
		return true;
	}

	CXCursor* overridden = nullptr;
	unsigned n = 0;
	clang_getOverriddenCursors(c, &overridden, &n);
	clang_disposeOverriddenCursors(overridden);

	return n > 0;
}

static std::vector<CXCursor> kids(CXCursor c)
{
	std::vector<CXCursor> result;
	clang_visitChildren(c, [](CXCursor k, CXCursor, CXClientData data)
	{
		static_cast<std::vector<CXCursor>*>(data)->push_back(k);
		return CXChildVisit_Continue;
	}, &result);

	return result;
}

static int clauses(CXCursor c)
{
	CXCursorKind k = clang_getCursorKind(c);
	std::vector<CXCursor> ks = kids(c);

	if (k == CXCursor_BinaryOperator)
	{
		CXBinaryOperatorKind op = clang_getCursorBinaryOperatorKind(c);

		if ((op == CXBinaryOperator_LAnd or op == CXBinaryOperator_LOr) and ks.size() == 2)
		{
			return clauses(ks[0]) + clauses(ks[1]);
		}
	}

	if ((k == CXCursor_ParenExpr or k == CXCursor_UnexposedExpr) and ks.size() == 1)
	{
		return clauses(ks[0]);
	}

	return 1;
}

static char const* keyword(CXCursorKind k)
{
	switch (k)
	{
		case CXCursor_IfStmt:
		{
			return "if";
		}

		case CXCursor_ForStmt:
		case CXCursor_CXXForRangeStmt:
		{
			return "for";
		}

		case CXCursor_WhileStmt:
		{
			return "while";
		}

		case CXCursor_DoStmt:
		{
			return "do";
		}

		case CXCursor_SwitchStmt:
		{
			return "switch";
		}

		case CXCursor_CXXTryStmt:
		{
			return "try";
		}

		default:
		{
			return nullptr;
		}
	}
}

static bool statement(CXCursorKind k)
{
	if (keyword(k) or k == CXCursor_CompoundStmt or k == CXCursor_ReturnStmt)
	{
		return false;
	}

	return k != CXCursor_CaseStmt and k != CXCursor_DefaultStmt;
}

static bool special(CXCursorKind k)
{
	return k == CXCursor_Constructor or k == CXCursor_Destructor or k == CXCursor_ConversionFunction;
}

static bool callable(CXCursorKind k)
{
	return special(k) or k == CXCursor_FunctionDecl or k == CXCursor_CXXMethod or k == CXCursor_FunctionTemplate;
}

static bool classLike(CXCursorKind k)
{
	switch (k)
	{
		case CXCursor_ClassDecl:
		case CXCursor_StructDecl:
		case CXCursor_UnionDecl:
		case CXCursor_ClassTemplate:
		case CXCursor_ClassTemplatePartialSpecialization:
		{
			return true;
		}

		default:
		{
			return false;
		}
	}
}

static bool member(CXCursor c)
{
	return special(clang_getCursorKind(c)) or classLike(clang_getCursorKind(clang_getCursorSemanticParent(c)));
}

static std::string usr(CXCursor c)
{
	CXCursor t = clang_getSpecializedCursorTemplate(c);

	if (not clang_Cursor_isNull(t))
	{
		c = t;
	}

	return takeString(clang_getCursorUSR(clang_getCanonicalCursor(c)));
}

static bool projectType(CXType t, int depth)
{
	if (depth > 8)
	{
		return false;
	}

	CXType c = clang_getCanonicalType(t);

	switch (c.kind)
	{
		case CXType_Pointer:
		case CXType_LValueReference:
		case CXType_RValueReference:
		{
			return projectType(clang_getPointeeType(c), depth + 1);
		}

		case CXType_ConstantArray:
		case CXType_IncompleteArray:
		case CXType_VariableArray:
		case CXType_DependentSizedArray:
		{
			return projectType(clang_getArrayElementType(c), depth + 1);
		}

		case CXType_Record:
		case CXType_Enum:
		{
			break;
		}

		default:
		{
			return false;
		}
	}

	if (not clang_Location_isInSystemHeader(clang_getCursorLocation(clang_getTypeDeclaration(c))))
	{
		return true;
	}

	int n = clang_Type_getNumTemplateArguments(c);

	for (int i = 0; i < n; i++)
	{
		if (projectType(clang_Type_getTemplateArgumentAsType(c, (unsigned)i), depth + 1))
		{
			return true;
		}
	}

	return false;
}

static char const* types(CXCursor c)
{
	if (projectType(clang_getCursorResultType(c), 0))
	{
		return "project";
	}

	for (CXCursor k : kids(c))
	{
		if (clang_getCursorKind(k) == CXCursor_ParmDecl and projectType(clang_getCursorType(k), 0))
		{
			return "project";
		}
	}

	return "plain";
}

static char const* linkage(CXCursor c)
{
	CXLinkageKind l = clang_getCursorLinkage(c);

	return l == CXLinkage_Internal or l == CXLinkage_UniqueExternal ? "internal" : "external";
}

static bool logical(CXCursor c)
{
	CXCursorKind k = clang_getCursorKind(c);

	if (k == CXCursor_BinaryOperator)
	{
		CXBinaryOperatorKind op = clang_getCursorBinaryOperatorKind(c);

		return op == CXBinaryOperator_LAnd or op == CXBinaryOperator_LOr;
	}

	return k == CXCursor_UnaryOperator and clang_getCursorUnaryOperatorKind(c) == CXUnaryOperator_LNot;
}

static unsigned offset(CXSourceLocation loc)
{
	unsigned o = 0;
	clang_getExpansionLocation(loc, nullptr, nullptr, nullptr, &o);

	return o;
}

static bool header(std::filesystem::path const& file)
{
	std::wstring ext = file.extension().wstring();

	for (wchar_t const* h : {L".h", L".hh", L".hpp", L".hxx", L".inl", L".ipp"})
	{
		if (_wcsicmp(ext.c_str(), h) == 0)
		{
			return true;
		}
	}

	return false;
}

static unsigned spelling(CXSourceLocation loc)
{
	unsigned o = 0;
	clang_getSpellingLocation(loc, nullptr, nullptr, nullptr, &o);

	return o;
}

struct Token
{
	std::string spelling;
	std::size_t offset;
};

class TreeBuilder
{
public:
	TreeBuilder(CXTranslationUnit unit, CXFile primary, SyntaxTree* tree)
		: unit(unit)
		, primary(primary)
		, tree(tree)
	{
	}

	void children(CXCursor c, SyntaxNode* owner)
	{
		std::vector<CXCursor> ks = kids(c);
		int condition = conditionIndex(c, ks);

		for (std::size_t i = 0; i < ks.size(); i++)
		{
			visit(ks[i], c, owner, (int)i == condition);
		}
	}

	void finish()
	{
		std::string fileClass;
		int most = 0;

		for (std::string const& cls : classes)
		{
			if (definitions[cls] > most)
			{
				most = definitions[cls];
				fileClass = cls;
			}
		}

		for (auto const& [n, id] : frees)
		{
			std::set<std::string> const& callers = uses[id];
			bool one = callers.size() == 1 and callers.begin()->starts_with("class:");

			n->node.attributes["file"] = fileClass.empty() ? "other" : "class";
			n->node.attributes["callers"] = callers.empty() ? "none" : (one ? "one-class" : "several");
			n->owner = one ? callers.begin()->substr(6) : fileClass;
		}
	}

private:
	void visit(CXCursor c, CXCursor parent, SyntaxNode* owner, bool condition)
	{
		CXFile where = nullptr;
		clang_getExpansionLocation(clang_getCursorLocation(c), &where, nullptr, nullptr, nullptr);

		if (not where or not clang_File_isEqual(where, primary))
		{
			return;
		}

		CXCursorKind k = clang_getCursorKind(c);

		if (macro(c))
		{
			SyntaxNode* n = owner;

			if (clang_getCursorKind(parent) == CXCursor_CompoundStmt and k != CXCursor_Namespace)
			{
				n = add(n, "statement", c);
			}

			if (logical(c) and inPrimary(c))
			{
				n = add(n, "expression", c);
				operatorToken(c, n);
			}

			children(c, n);
			return;
		}

		if (k == CXCursor_MacroDefinition and clang_Cursor_isMacroBuiltin(c))
		{
			return;
		}

		SyntaxNode* n = owner;

		if (condition)
		{
			n = add(n, "condition", c);
			n->clauses = clauses(c);
		}
		else if (clang_getCursorKind(parent) == CXCursor_CompoundStmt and statement(k))
		{
			n = add(n, "statement", c);
		}

		char const* type = special(k) ? "function" : nodeType(k);
		char const* kw = keyword(k);

		if (type)
		{
			n = declaration(n, c, type);
		}
		else if (kw)
		{
			n = add(n, "control", c);
			n->node.attributes["keyword"] = kw;
			n->node.attributes["braced"] = braced(c) ? "yes" : "no";
		}
		else if (k == CXCursor_CompoundStmt)
		{
			n = add(n, "block", c);
		}
		else if (k == CXCursor_ReturnStmt)
		{
			n = add(n, "return", c);
		}
		else if (k == CXCursor_CaseStmt or k == CXCursor_DefaultStmt)
		{
			n = add(n, "case", c);
			caseBody(c, n);
		}
		else if (logical(c))
		{
			n = add(n, "expression", c);
			operatorToken(c, n);
		}

		if (k == CXCursor_DeclRefExpr)
		{
			reference(c);
		}

		if (callable(k))
		{
			enclosing.push_back(c);
			children(c, n);
			enclosing.pop_back();
			return;
		}

		children(c, n);
	}

	void reference(CXCursor c)
	{
		CXCursor target = clang_getCursorReferenced(c);
		CXCursorKind k = clang_getCursorKind(target);

		if (k != CXCursor_FunctionDecl and k != CXCursor_FunctionTemplate)
		{
			return;
		}

		std::string callee = usr(target);
		std::string caller = "free";

		if (not enclosing.empty())
		{
			CXCursor e = enclosing.back();

			if (usr(e) == callee)
			{
				return;
			}

			if (member(e))
			{
				caller = "class:" + takeString(clang_getCursorSpelling(clang_getCursorSemanticParent(e)));
			}
		}

		uses[callee].insert(caller);
	}

	void caseBody(CXCursor c, SyntaxNode* n)
	{
		CXCursor body = c;

		while (clang_getCursorKind(body) == CXCursor_CaseStmt or clang_getCursorKind(body) == CXCursor_DefaultStmt)
		{
			std::vector<CXCursor> ks = kids(body);

			if (ks.empty())
			{
				return;
			}

			body = ks.back();
		}

		CXSourceRange r = clang_getCursorExtent(body);
		n->bodyStart = offset(clang_getRangeStart(r));
		n->bodyEnd = offset(clang_getRangeEnd(r));
		n->bodyBraced = clang_getCursorKind(body) == CXCursor_CompoundStmt;
	}

	SyntaxNode* declaration(SyntaxNode* owner, CXCursor c, char const* type)
	{
		SyntaxNode* n = add(owner, type, c);
		CXCursorKind k = clang_getCursorKind(c);
		unsigned line = 0;
		unsigned column = 0;
		clang_getExpansionLocation(clang_getCursorLocation(c), nullptr, &line, &column, nullptr);

		n->name = takeString(clang_getCursorSpelling(c));
		n->node.name = n->name;
		n->line = (int)line;
		n->column = (int)column;
		n->checkName = not special(k) and not exempt(c, n->name);
		n->nameOffset = offset(clang_getCursorLocation(c));

		if (k == CXCursor_TemplateTypeParameter or k == CXCursor_TemplateTemplateParameter)
		{
			n->node.attributes["storage"] = "template";
		}

		if (k == CXCursor_EnumDecl)
		{
			n->node.attributes["enum"] = clang_EnumDecl_isScoped(c) ? "class" : "plain";
		}

		char const* a = access(c);

		if (a)
		{
			n->node.attributes["access"] = a;
		}

		if (n->node.type == "variable")
		{
			n->node.attributes["storage"] = storage(c);
			n->node.attributes["const"] = constness(unit, c, n->name);
		}

		if (n->node.type == "function")
		{
			function(n, c);
		}

		return n;
	}

	void function(SyntaxNode* n, CXCursor c)
	{
		if (not member(c))
		{
			n->node.attributes["scope"] = "free";
			n->node.attributes["linkage"] = linkage(c);
			n->node.attributes["types"] = types(c);
			frees.push_back({n, usr(c)});
			return;
		}

		n->node.attributes["scope"] = "member";

		if (not clang_isCursorDefinition(c))
		{
			return;
		}

		std::string cls = takeString(clang_getCursorSpelling(clang_getCursorSemanticParent(c)));

		if (definitions[cls]++ == 0)
		{
			classes.push_back(cls);
		}
	}

	SyntaxNode* add(SyntaxNode* owner, char const* type, CXCursor c)
	{
		SyntaxNode* n = tree->add(owner, type);
		CXSourceRange r = clang_getCursorExtent(c);
		unsigned firstLine = 0;
		unsigned firstColumn = 0;
		unsigned lastLine = 0;
		clang_getExpansionLocation(clang_getRangeStart(r), nullptr, &firstLine, &firstColumn, nullptr);
		clang_getExpansionLocation(clang_getRangeEnd(r), nullptr, &lastLine, nullptr, nullptr);

		n->firstLine = (int)firstLine;
		n->lastLine = (int)lastLine;
		n->line = (int)firstLine;
		n->column = (int)firstColumn;
		n->start = offset(clang_getRangeStart(r));
		n->end = offset(clang_getRangeEnd(r));

		return n;
	}

	bool macro(CXCursor c) const
	{
		if (clang_getCursorKind(c) == CXCursor_MacroDefinition)
		{
			return false;
		}

		CXSourceLocation loc = clang_getRangeStart(clang_getCursorExtent(c));

		return offset(loc) != spelling(loc);
	}

	std::vector<Token> tokens(CXCursor c) const
	{
		CXToken* ts = nullptr;
		unsigned n = 0;
		std::vector<Token> result;
		clang_tokenize(unit, clang_getCursorExtent(c), &ts, &n);

		for (unsigned i = 0; i < n; i++)
		{
			result.push_back({takeString(clang_getTokenSpelling(unit, ts[i])), offset(clang_getTokenLocation(unit, ts[i]))});
		}

		clang_disposeTokens(unit, ts, n);

		return result;
	}

	bool braced(CXCursor c) const
	{
		if (clang_getCursorKind(c) == CXCursor_DoStmt)
		{
			std::vector<CXCursor> ks = kids(c);

			return not ks.empty() and clang_getCursorKind(ks[0]) == CXCursor_CompoundStmt;
		}

		std::vector<Token> ts = tokens(c);

		return not ts.empty() and ts.back().spelling == "}";
	}

	bool inPrimary(CXCursor c) const
	{
		CXFile where = nullptr;
		clang_getSpellingLocation(clang_getRangeStart(clang_getCursorExtent(c)), &where, nullptr, nullptr, nullptr);

		return where and clang_File_isEqual(where, primary);
	}

	void operatorToken(CXCursor c, SyntaxNode* n) const
	{
		bool inMacro = macro(c);
		std::size_t low = inMacro ? spelling(clang_getRangeStart(clang_getCursorExtent(c))) : 0;

		if (clang_getCursorKind(c) == CXCursor_BinaryOperator)
		{
			std::vector<CXCursor> ks = kids(c);

			if (not ks.empty())
			{
				CXSourceLocation end = clang_getRangeEnd(clang_getCursorExtent(ks[0]));
				low = inMacro ? spelling(end) : offset(end);
			}
		}

		for (Token const& t : tokens(c))
		{
			bool symbol = t.spelling == "&&" or t.spelling == "||" or t.spelling == "!";
			bool word = t.spelling == "and" or t.spelling == "or" or t.spelling == "not";

			if (t.offset >= low and (symbol or word))
			{
				n->operatorOffset = t.offset;
				n->operatorSpelling = t.spelling;
				return;
			}
		}
	}

	int conditionIndex(CXCursor c, std::vector<CXCursor> const& ks) const
	{
		CXCursorKind k = clang_getCursorKind(c);

		if (k == CXCursor_DoStmt)
		{
			return ks.size() == 2 ? 1 : -1;
		}

		if (k != CXCursor_IfStmt and k != CXCursor_WhileStmt and k != CXCursor_SwitchStmt and k != CXCursor_ForStmt)
		{
			return -1;
		}

		std::size_t open = 0;
		std::size_t close = 0;
		std::vector<std::size_t> semicolons;
		int depth = 0;

		for (Token const& t : tokens(c))
		{
			if (t.spelling == "(")
			{
				if (depth == 0)
				{
					open = t.offset;
				}

				depth++;
				continue;
			}

			if (t.spelling == ")")
			{
				depth--;

				if (depth == 0)
				{
					close = t.offset;
					break;
				}

				continue;
			}

			if (t.spelling == ";" and depth == 1)
			{
				semicolons.push_back(t.offset);
			}
		}

		std::size_t low = open;
		std::size_t high = close;

		if (k == CXCursor_ForStmt)
		{
			if (semicolons.size() != 2)
			{
				return -1;
			}

			low = semicolons[0];
			high = semicolons[1];
		}

		int result = -1;

		for (std::size_t i = 0; i < ks.size(); i++)
		{
			CXSourceRange r = clang_getCursorExtent(ks[i]);

			if (offset(clang_getRangeStart(r)) > low and offset(clang_getRangeEnd(r)) <= high)
			{
				result = (int)i;
			}
		}

		return result;
	}

	CXTranslationUnit unit;
	CXFile primary;
	SyntaxTree* tree;
	std::vector<CXCursor> enclosing;
	std::map<std::string, std::set<std::string>> uses;
	std::map<std::string, int> definitions;
	std::vector<std::string> classes;
	std::vector<std::pair<SyntaxNode*, std::string>> frees;
};

TranslationUnit::TranslationUnit(std::filesystem::path const& file,
                                 std::string const& text,
                                 CompileCommand const& cmd)
{
	source = file;

	std::string name = utf8(file);
	std::string mainName = name;
	std::string mainText;
	std::vector<char const*> argv;
	std::vector<CXUnsavedFile> unsaved = {{name.c_str(), text.data(), (unsigned long)text.size()}};

	if (header(file))
	{
		mainName = utf8(file.parent_path() / (file.filename().wstring() + L".strictor.cpp"));
		mainText = "#include \"" + utf8(file.filename()) + "\"\n";
		unsaved.push_back({mainName.c_str(), mainText.data(), (unsigned long)mainText.size()});
	}

	for (std::string const& arg : cmd.args)
	{
		argv.push_back(arg.c_str());
	}

	std::filesystem::path cwd = std::filesystem::current_path();
	std::filesystem::current_path(cmd.dir);

	index = clang_createIndex(0, 0);
	CXErrorCode code = clang_parseTranslationUnit2(index,
	                                               mainName.c_str(),
	                                               argv.data(),
	                                               (int)argv.size(),
	                                               unsaved.data(),
	                                               (unsigned)unsaved.size(),
	                                               CXTranslationUnit_DetailedPreprocessingRecord,
	                                               &unit);

	std::filesystem::current_path(cwd);

	if (code != CXError_Success)
	{
		clang_disposeIndex(index);
		throw std::runtime_error("libclang could not parse the file");
	}

	unsigned n = clang_getNumDiagnostics(unit);
	CXFile primary = clang_getFile(unit, name.c_str());
	CXFile stand = clang_getFile(unit, mainName.c_str());

	for (unsigned i = 0; i < n; i++)
	{
		CXDiagnostic d = clang_getDiagnostic(unit, i);
		CXDiagnosticSeverity severity = clang_getDiagnosticSeverity(d);

		if (severity < CXDiagnostic_Error)
		{
			clang_disposeDiagnostic(d);
			continue;
		}

		CXFile origin = nullptr;
		clang_getExpansionLocation(clang_getDiagnosticLocation(d), &origin, nullptr, nullptr, nullptr);

		bool elsewhere = origin and not clang_File_isEqual(origin, primary) and not clang_File_isEqual(origin, stand);

		if (severity == CXDiagnostic_Error and elsewhere)
		{
			clang_disposeDiagnostic(d);
			continue;
		}

		CXFile where = nullptr;
		unsigned line = 0;
		unsigned column = 0;
		clang_getSpellingLocation(clang_getDiagnosticLocation(d), &where, &line, &column, nullptr);

		std::string place = std::format("{}:{}", line, column);

		if (where and not clang_File_isEqual(where, primary))
		{
			place = takeString(clang_getFileName(where)) + ":" + place;
		}

		std::string msg = "parse failed at " + place + ": " + takeString(clang_getDiagnosticSpelling(d));
		clang_disposeDiagnostic(d);
		clang_disposeTranslationUnit(unit);
		clang_disposeIndex(index);
		throw std::runtime_error(msg);
	}
}

TranslationUnit::~TranslationUnit()
{
	clang_disposeTranslationUnit(unit);
	clang_disposeIndex(index);
}

SyntaxTree TranslationUnit::tree() const
{
	SyntaxTree t;
	TreeBuilder builder(unit, clang_getFile(unit, utf8(source).c_str()), &t);
	builder.children(clang_getTranslationUnitCursor(unit), nullptr);
	builder.finish();

	return t;
}
