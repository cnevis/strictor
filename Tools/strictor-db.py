import json
import subprocess
import sys
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path

MSBUILD = r"C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\amd64\MSBuild.exe"
SKIP = {"vendor", "Build", ".labs", ".claude", ".git", "node_modules", "vcpkg_installed"}
PROPERTIES = ["ExternalIncludePath", "VC_IncludePath", "WindowsSDK_IncludePath"]


def projects(repo):
    found = []

    for p in repo.rglob("*.vcxproj"):
        rel = p.relative_to(repo).parts

        if len(rel) <= 4 and not SKIP.intersection(rel):
            found.append(p)

    return sorted(found)


def split(value):
    return [v for v in value.split(";") if v and not v.startswith("%(")]


def externals(project, props):
    toolchain = set()

    for name in PROPERTIES[1:]:
        toolchain.update(Path(v).resolve() for v in split(props.get(name, "")))

    found = []

    for v in split(props.get("ExternalIncludePath", "")):
        p = (project.parent / v).resolve()

        if p not in toolchain and p not in found:
            found.append(p)

    return found


def entries(project):
    cmd = [MSBUILD, str(project), "-getItem:ClCompile", "-getProperty:" + ",".join(PROPERTIES),
           "-p:Configuration=Debug", "-p:Platform=x64", "-nologo"]
    out = subprocess.run(cmd, capture_output=True, text=True, encoding="utf-8", errors="replace")
    start = out.stdout.find("{")

    if start < 0:
        print(f"skipped {project}: no compile items", file=sys.stderr)
        return []

    data = json.loads(out.stdout[start:])
    external = externals(project, data.get("Properties", {}))
    result = []

    for item in data.get("Items", {}).get("ClCompile", []):
        file = (project.parent / item["Identity"]).resolve()
        args = ["cl.exe", "/nologo", "/std:c++latest", "/EHsc", "/permissive-", "/Zc:__cplusplus", "/Zc:preprocessor"]
        args += ["/I" + str((project.parent / inc).resolve()) for inc in split(item.get("AdditionalIncludeDirectories", ""))]
        args += ["/external:I" + str(inc) for inc in external]
        args += ["/D" + d for d in split(item.get("PreprocessorDefinitions", ""))]
        args += ["/FI" + f for f in split(item.get("ForcedIncludeFiles", ""))]
        args += ["/c", str(file)]
        result.append({"directory": str(project.parent), "file": str(file), "arguments": args})

    return result


def main():
    if len(sys.argv) != 2:
        print("usage: strictor-db <repo>", file=sys.stderr)
        return 2

    repo = Path(sys.argv[1]).resolve()
    ps = projects(repo)

    with ThreadPoolExecutor(max_workers=6) as pool:
        lists = list(pool.map(entries, ps))

    all_entries = [e for l in lists for e in l]
    out = repo / "Build" / "compile_commands.json"
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_text(json.dumps(all_entries, indent=1), encoding="utf-8")
    print(f"{out}: {len(all_entries)} entries from {len(ps)} projects")

    return 0


sys.exit(main())
