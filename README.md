# strictor

strictor is a C++ style code checker I made to plug into my AI assisted workflow. It runs clang-format, then walks the libclang syntax tree and applies naming, layout and design rules from CSS-like strictor stylesheet. Rules it can fix are rewritten in place and the rest is reported as diagnostics for my AI agents.

## Usage

```
strictor file    [-p <build-dir>] [--whole] <path>...
strictor changed [-p <build-dir>] [--whole]
strictor stdin   [-p <build-dir>] [--whole] <path>
```

- `file` checks and fixes the given files.
- `changed` checks the files git reports as changed.
- `stdin` reads source from standard input, writes the fixed text to standard output and reports to standard error. `<path>` names the file the text belongs to.

By default only lines that differ from git `HEAD` are fixed and reported. `--whole` checks the entire file.

`-p` names the folder holding `compile_commands.json`. Without it, strictor searches upward from each file, and parses without compile flags when none is found. `Tools/strictor-db.py <repo>` writes `Build/compile_commands.json` for an MSBuild repository.

Diagnostics are printed as `file:line:column: rule: message`. The exit code is 0 when clean, 1 when there are diagnostics and 2 on errors.

## Stylesheets

strictor applies every `.strictor` file from the source file's folder up to the drive root, outer files first. Selectors match syntax nodes by type and attribute, combine with `>` and descendant spaces, and cascade by specificity like CSS.

```css
type
{
    name: PascalCase;
    plural: report;
}

function
{
    name: camelCase;
    banned-suffixes: Of For At;
}

variable[storage=local]
{
    name: camelCase;
    rename: index=idx;
}

variable[storage=member][access=private]
{
    name: m_PascalCase;
}

control
{
    blank-before: 1;
    blank-after: 1;
}

control > condition
{
    max-clauses: 4;
}

expression
{
    logical-operators: words;
}

case
{
    braces: insert;
}

type[enum=plain]
{
    report: use enum class;
}
```

Node types are `type`, `function`, `variable`, `enumerator`, `macro`, `namespace`, `block`, `statement`, `control`, `condition`, `expression`, `return`, `case` and `*`.

| Attribute | Values                                               |
| --------- | ---------------------------------------------------- |
| `storage` | `local`, `parameter`, `template`, `member`, `global` |
| `access`  | `public`, `protected`, `private`                     |
| `const`   | `constexpr`, `const`, `none`                         |
| `keyword` | `if`, `for`, `while`, `do`, `switch`, `try`          |
| `braced`  | `yes`, `no`                                          |
| `scope`   | `free`, `member`                                     |
| `linkage` | `internal`, `external`                               |
| `file`    | `class`, `other`                                     |
| `callers` | `none`, `one-class`, `several`                       |
| `types`   | `plain`, `project`                                   |
| `enum`    | `class`, `plain`                                     |
| `lines`   | `single`, `multi` (functions)                        |

| Property                                                             | Effect                                                                                                                       |
| -------------------------------------------------------------------- | ---------------------------------------------------------------------------------------------------------------------------- |
| `name`                                                               | Allowed name styles: `PascalCase`, `camelCase`, `UPPER_SNAKE`, `snake_case`, optionally with a prefix such as `m_PascalCase` |
| `rename`                                                             | `old=new` word replacements                                                                                                  |
| `banned-names`, `banned-parts`, `banned-prefixes`, `banned-suffixes` | Words a name must not be or contain                                                                                          |
| `blank-before`, `blank-after`                                        | Exact blank lines around a statement or declaration                                                                          |
| `max-clauses`                                                        | Most `and`/`or` clauses in a condition                                                                                       |
| `logical-operators`                                                  | `words` rewrites `&&`, `\|\|` and `!` to `and`, `or` and `not`                                                               |
| `declarators`                                                        | `split` puts each declarator in its own declaration                                                                          |
| `braces`                                                             | `insert` braces case bodies                                                                                                  |
| `helper`                                                             | `report` flags free functions that should be members; `helper-allow` and `helper-folders` exempt names and folders           |
| `plural`                                                             | `report` flags plural type names; `plural-allow` exempts words                                                               |
| `report`                                                             | Reports every matching node with the given message                                                                           |
| `script`                                                             | Lua rule files, relative to the stylesheet                                                                                   |

## Lua rules

A script registers handlers per node type and reports through the context it receives:

```lua
on('function', function(node, r)
    if node.name:find('_') then
        r:report('custom.underscore', node, 'no underscores in function names')
    end
end)
```

Nodes expose `type`, `name`, `line`, `column`, `parent`, `attributes`, `lines()` and `blankBefore()`. The context has `report(rule, node, message)` and `insertBefore(node, text)`.

## Building

Requires Visual Studio 2026, CMake 3.25, LLVM installed in `Program Files\LLVM` (strictor loads `libclang.dll` and runs `clang-format.exe` from there) and vcpkg with `VCPKG_ROOT` set.

```
cmake --preset x64
cmake --build --preset x64-release
```

## License

MIT. See [LICENSE](LICENSE).
