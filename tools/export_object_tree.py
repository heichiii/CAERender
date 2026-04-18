#!/usr/bin/env python3
import argparse
import json
import re
from dataclasses import dataclass, field, asdict
from pathlib import Path
from typing import Dict, List, Set, Tuple


CLASS_DECL_RE = re.compile(
    r"\b(class|struct)\s+([A-Za-z_]\w*)\s*(?:\:\s*([^\{]+))?\s*\{",
    re.MULTILINE,
)

ACCESS_RE = re.compile(r"^\s*(public|protected|private)(?:\s+slots)?\s*:\s*$")

CPP_IMPL_RE = re.compile(
    r"(?:^|\n)\s*(?:template\s*<[^>]*>\s*)?"
    r"(?:[A-Za-z_][\w:<>,\s\*&~]*?\s+)?"
    r"([A-Za-z_]\w*)::(~?[A-Za-z_]\w*)\s*\(([^\)]*)\)"
    r"\s*(?:const\s*)?(?:noexcept\s*)?(?:->\s*[^\{\n]+\s*)?\{",
    re.MULTILINE,
)


@dataclass
class MethodInfo:
    signature: str
    name: str
    access: str
    declared_in: str
    line: int
    implemented: bool = False
    implemented_in: str = ""
    impl_line: int = -1


@dataclass
class MemberInfo:
    declaration: str
    access: str
    declared_in: str
    line: int


@dataclass
class ClassInfo:
    kind: str
    name: str
    bases: List[str] = field(default_factory=list)
    declared_in: str = ""
    line: int = -1
    members: List[MemberInfo] = field(default_factory=list)
    methods: List[MethodInfo] = field(default_factory=list)


def strip_comments(text: str) -> str:
    text = re.sub(r"//.*", "", text)
    text = re.sub(r"/\*.*?\*/", "", text, flags=re.DOTALL)
    return text


def find_matching_brace(text: str, open_idx: int) -> int:
    depth = 0
    in_str = False
    in_char = False
    escaped = False

    for idx in range(open_idx, len(text)):
        ch = text[idx]

        if in_str:
            if not escaped and ch == '"':
                in_str = False
            escaped = (ch == "\\" and not escaped)
            continue

        if in_char:
            if not escaped and ch == "'":
                in_char = False
            escaped = (ch == "\\" and not escaped)
            continue

        if ch == '"':
            in_str = True
            escaped = False
            continue

        if ch == "'":
            in_char = True
            escaped = False
            continue

        if ch == "{":
            depth += 1
        elif ch == "}":
            depth -= 1
            if depth == 0:
                return idx

    return -1


def line_number_from_offset(text: str, offset: int) -> int:
    return text.count("\n", 0, offset) + 1


def split_top_level_statements(body: str) -> List[Tuple[str, int]]:
    statements: List[Tuple[str, int]] = []
    start = 0
    depth_round = 0
    depth_angle = 0
    depth_square = 0
    depth_brace = 0
    in_str = False
    in_char = False
    escaped = False

    for i, ch in enumerate(body):
        if in_str:
            if not escaped and ch == '"':
                in_str = False
            escaped = (ch == "\\" and not escaped)
            continue

        if in_char:
            if not escaped and ch == "'":
                in_char = False
            escaped = (ch == "\\" and not escaped)
            continue

        if ch == '"':
            in_str = True
            escaped = False
            continue
        if ch == "'":
            in_char = True
            escaped = False
            continue

        if ch == "(":
            depth_round += 1
        elif ch == ")":
            depth_round = max(0, depth_round - 1)
        elif ch == "<":
            depth_angle += 1
        elif ch == ">":
            depth_angle = max(0, depth_angle - 1)
        elif ch == "[":
            depth_square += 1
        elif ch == "]":
            depth_square = max(0, depth_square - 1)
        elif ch == "{":
            depth_brace += 1
        elif ch == "}":
            depth_brace = max(0, depth_brace - 1)

        if (
            ch == ";"
            and depth_round == 0
            and depth_angle == 0
            and depth_square == 0
            and depth_brace == 0
        ):
            stmt = body[start : i + 1]
            line = body.count("\n", 0, start) + 1
            statements.append((stmt.strip(), line))
            start = i + 1

    return [s for s in statements if s[0]]


def normalize_space(s: str) -> str:
    return re.sub(r"\s+", " ", s).strip()


def parse_bases(raw: str) -> List[str]:
    if not raw:
        return []

    parts = [p.strip() for p in raw.split(",")]
    bases: List[str] = []
    for p in parts:
        p = re.sub(r"\b(public|protected|private|virtual)\b", "", p)
        p = normalize_space(p)
        if p:
            bases.append(p)
    return bases


def parse_class_body(
    cls: ClassInfo,
    body: str,
    body_line_start: int,
    file_rel: str,
) -> None:
    clean = strip_comments(body)

    default_access = "private" if cls.kind == "class" else "public"
    access_marks: List[Tuple[int, str]] = [(1, default_access)]
    lines = clean.splitlines()
    for i, line in enumerate(lines, start=1):
        access_match = ACCESS_RE.match(line.strip())
        if access_match:
            # Access label affects following declarations.
            access_marks.append((i + 1, access_match.group(1)))

    def access_for_stmt(stmt_rel_line: int) -> str:
        current = default_access
        for start_line, acc in access_marks:
            if stmt_rel_line >= start_line:
                current = acc
            else:
                break
        return current

    for stmt, stmt_rel_line in split_top_level_statements(clean):
        if ACCESS_RE.match(stmt):
            continue

        stmt_single = normalize_space(stmt)
        if not stmt_single:
            continue

        # Some compact declarations may include an access label prefix in the same statement.
        stmt_single = re.sub(r"^(public|protected|private)(?:\s+slots)?\s*:\s*", "", stmt_single)
        if not stmt_single:
            continue

        if stmt_single in {"Q_OBJECT;", "Q_GADGET;"}:
            continue

        if re.match(r"^(using|typedef|enum|friend)\b", stmt_single):
            continue

        line_num = body_line_start + stmt_rel_line - 1
        access = access_for_stmt(stmt_rel_line)

        if "(" in stmt_single and ")" in stmt_single:
            method_name_match = re.search(r"(~?[A-Za-z_]\w*)\s*\(", stmt_single)
            name = method_name_match.group(1) if method_name_match else "<unknown>"
            cls.methods.append(
                MethodInfo(
                    signature=stmt_single,
                    name=name,
                    access=access,
                    declared_in=file_rel,
                    line=line_num,
                )
            )
        else:
            cls.members.append(
                MemberInfo(
                    declaration=stmt_single,
                    access=access,
                    declared_in=file_rel,
                    line=line_num,
                )
            )


def parse_headers(src_root: Path) -> Dict[str, ClassInfo]:
    classes: Dict[str, ClassInfo] = {}
    headers = sorted(src_root.rglob("*.h")) + sorted(src_root.rglob("*.hpp"))

    for hfile in headers:
        text = hfile.read_text(encoding="utf-8", errors="ignore")
        rel = hfile.relative_to(src_root.parent).as_posix()

        for m in CLASS_DECL_RE.finditer(text):
            kind, name, bases_raw = m.groups()

            # Skip "enum class X" patterns that are not class/struct definitions.
            prefix = text[max(0, m.start() - 10) : m.start()]
            if re.search(r"\benum\s*$", prefix):
                continue

            open_brace = m.end() - 1
            close_brace = find_matching_brace(text, open_brace)
            if close_brace < 0:
                continue

            line = line_number_from_offset(text, m.start())
            body_line_start = line_number_from_offset(text, open_brace) + 1
            body = text[open_brace + 1 : close_brace]

            cls = classes.get(name)
            if cls is None:
                cls = ClassInfo(
                    kind=kind,
                    name=name,
                    bases=parse_bases(bases_raw),
                    declared_in=rel,
                    line=line,
                )
                classes[name] = cls
            else:
                if not cls.bases:
                    cls.bases = parse_bases(bases_raw)

            parse_class_body(cls, body, body_line_start, rel)

    return classes


def parse_cpp_impls(src_root: Path) -> Dict[str, List[Tuple[str, str, int]]]:
    impls: Dict[str, List[Tuple[str, str, int]]] = {}
    cpp_files = sorted(src_root.rglob("*.cpp"))

    for cpp in cpp_files:
        text = strip_comments(cpp.read_text(encoding="utf-8", errors="ignore"))
        rel = cpp.relative_to(src_root.parent).as_posix()

        for m in CPP_IMPL_RE.finditer(text):
            cls_name, method_name, args = m.groups()
            start = m.start(1)
            line = line_number_from_offset(text, start)
            signature = normalize_space(f"{method_name}({args})")
            impls.setdefault(cls_name, []).append((signature, rel, line))

    return impls


def merge_impls(classes: Dict[str, ClassInfo], impls: Dict[str, List[Tuple[str, str, int]]]) -> None:
    for cls_name, methods in impls.items():
        if cls_name not in classes:
            classes[cls_name] = ClassInfo(
                kind="class",
                name=cls_name,
                declared_in="<cpp-only>",
                line=-1,
            )

        cls = classes[cls_name]
        by_name: Dict[str, List[MethodInfo]] = {}
        for declared in cls.methods:
            by_name.setdefault(declared.name, []).append(declared)

        for impl_sig, impl_file, impl_line in methods:
            impl_name_match = re.match(r"(~?[A-Za-z_]\w*)\s*\(", impl_sig)
            impl_name = impl_name_match.group(1) if impl_name_match else ""
            candidates = by_name.get(impl_name, [])

            linked = False
            for c in candidates:
                if c.implemented:
                    continue
                c.implemented = True
                c.implemented_in = impl_file
                c.impl_line = impl_line
                linked = True
                break

            if not linked:
                cls.methods.append(
                    MethodInfo(
                        signature=impl_sig,
                        name=impl_name or "<unknown>",
                        access="unknown",
                        declared_in="<implementation>",
                        line=-1,
                        implemented=True,
                        implemented_in=impl_file,
                        impl_line=impl_line,
                    )
                )


def build_tree(classes: Dict[str, ClassInfo]) -> Dict[str, List[str]]:
    parent_to_children: Dict[str, List[str]] = {}
    for cls in classes.values():
        if not cls.bases:
            parent_to_children.setdefault("<root>", []).append(cls.name)
            continue

        for b in cls.bases:
            parent_to_children.setdefault(b, []).append(cls.name)

    for key in list(parent_to_children.keys()):
        parent_to_children[key] = sorted(set(parent_to_children[key]))
    return parent_to_children


def build_object_tree(classes: Dict[str, ClassInfo], include_qt_one_level: bool = True) -> Dict[str, List[str]]:
    class_names = set(classes.keys())
    tree: Dict[str, List[str]] = {}

    for owner_name, cls in classes.items():
        refs: Set[str] = set()
        qt_refs: Set[str] = set()
        for member in cls.members:
            for candidate in class_names:
                if candidate == owner_name:
                    continue
                if re.search(rf"\b{re.escape(candidate)}\b", member.declaration):
                    refs.add(candidate)

            if include_qt_one_level:
                for qt_type in re.findall(r"\b(Q[A-Za-z_]\w*)\b", member.declaration):
                    if qt_type not in class_names:
                        qt_refs.add(qt_type)
        merged = refs | qt_refs
        if merged:
            tree[owner_name] = sorted(merged)

    # Build synthetic root for object relation graph roots.
    referenced: Set[str] = set()
    for children in tree.values():
        referenced.update(children)

    roots = sorted([name for name in classes.keys() if name not in referenced])
    tree["<root>"] = roots
    return tree


def extract_subtree(tree: Dict[str, List[str]], start: str) -> Dict[str, List[str]]:
    if start not in tree and start not in {c for children in tree.values() for c in children}:
        raise ValueError(f"Start node not found in tree: {start}")

    out: Dict[str, List[str]] = {}
    visited: Set[str] = set()

    def dfs(node: str) -> None:
        if node in visited:
            return
        visited.add(node)
        children = tree.get(node, [])
        out[node] = list(children)
        for child in children:
            dfs(child)

    dfs(start)
    return out


def collect_leaf_nodes(tree: Dict[str, List[str]], start: str) -> List[str]:
    leaves: Set[str] = set()
    visited: Set[str] = set()

    def dfs(node: str) -> None:
        if node in visited:
            return
        visited.add(node)
        children = tree.get(node, [])
        if not children:
            leaves.add(node)
            return
        for child in children:
            dfs(child)

    dfs(start)
    return sorted(leaves)


def tree_lines(tree: Dict[str, List[str]]) -> List[str]:
    lines: List[str] = []

    def dfs(node: str, prefix: str, visited: set) -> None:
        if node in visited:
            lines.append(f"{prefix}- {node} (cycle)")
            return

        visited.add(node)
        children = tree.get(node, [])
        for idx, child in enumerate(children):
            last = idx == len(children) - 1
            branch = "└─" if last else "├─"
            lines.append(f"{prefix}{branch} {child}")
            dfs(child, prefix + ("   " if last else "│  "), visited.copy())

    lines.append("<root>")
    dfs("<root>", "", set())
    return lines


def tree_lines_from(tree: Dict[str, List[str]], start: str) -> List[str]:
    lines: List[str] = [start]

    def dfs(node: str, prefix: str, visited: Set[str]) -> None:
        if node in visited:
            lines.append(f"{prefix}- {node} (cycle)")
            return

        visited.add(node)
        children = tree.get(node, [])
        for idx, child in enumerate(children):
            last = idx == len(children) - 1
            branch = "└─" if last else "├─"
            lines.append(f"{prefix}{branch} {child}")
            dfs(child, prefix + ("   " if last else "│  "), visited.copy())

    dfs(start, "", set())
    return lines


def class_to_dict(cls: ClassInfo) -> dict:
    return {
        "kind": cls.kind,
        "name": cls.name,
        "bases": cls.bases,
        "declared_in": cls.declared_in,
        "line": cls.line,
        "members": [asdict(m) for m in cls.members],
        "methods": [asdict(m) for m in cls.methods],
    }


def write_markdown(
    output: Path,
    classes: Dict[str, ClassInfo],
    tree: Dict[str, List[str]],
    start_node: str,
    leaf_nodes: List[str],
    tree_mode: str,
    qt_one_level_nodes: List[str],
) -> None:
    lines: List[str] = []
    lines.append("# Object Tree and Custom Class Report")
    lines.append("")
    lines.append(f"- Tree Mode: {tree_mode}")
    lines.append(f"- Start Node: {start_node}")
    lines.append("")
    lines.append("## Object Tree")
    lines.append("")
    lines.append("```text")
    lines.extend(tree_lines_from(tree, start_node))
    lines.append("```")
    lines.append("")
    lines.append("## Leaf Classes")
    lines.append("")
    if leaf_nodes:
        for leaf in leaf_nodes:
            lines.append(f"- {leaf}")
    else:
        lines.append("- (none)")
    lines.append("")

    lines.append("## Qt One-Level Objects")
    lines.append("")
    if qt_one_level_nodes:
        for node in qt_one_level_nodes:
            lines.append(f"- {node}")
    else:
        lines.append("- (none)")
    lines.append("")

    reachable = set([start_node])
    for line in tree_lines_from(tree, start_node)[1:]:
        node = line.split(" ")[-1]
        if node and node != "(cycle)":
            reachable.add(node)

    for cls_name in sorted([n for n in classes if n in reachable]):
        cls = classes[cls_name]
        base_text = ", ".join(cls.bases) if cls.bases else "(none)"
        lines.append(f"## {cls.kind} {cls.name}")
        lines.append("")
        lines.append(f"- Declared: {cls.declared_in}:{cls.line if cls.line > 0 else '?'}")
        lines.append(f"- Bases: {base_text}")
        lines.append("")

        lines.append("### Members")
        if not cls.members:
            lines.append("- (none)")
        else:
            for m in cls.members:
                lines.append(
                    f"- [{m.access}] {m.declaration}  ({m.declared_in}:{m.line})"
                )
        lines.append("")

        lines.append("### Methods")
        if not cls.methods:
            lines.append("- (none)")
        else:
            for method in cls.methods:
                impl = (
                    f" -> impl {method.implemented_in}:{method.impl_line}"
                    if method.implemented
                    else " -> impl (not found)"
                )
                decl = (
                    f"{method.declared_in}:{method.line}"
                    if method.line > 0
                    else method.declared_in
                )
                lines.append(f"- [{method.access}] {method.signature}  ({decl}){impl}")
        lines.append("")

    output.write_text("\n".join(lines), encoding="utf-8")


def main() -> None:
    parser = argparse.ArgumentParser(
        description="Export object tree and class members/methods from C++ project sources."
    )
    parser.add_argument(
        "--src",
        default="src",
        help="Source directory to scan. Default: src",
    )
    parser.add_argument(
        "--tree-mode",
        choices=["inheritance", "object"],
        default="object",
        help="Tree mode: inheritance or object(member reference). Default: object",
    )
    parser.add_argument(
        "--start",
        default="MainWindow",
        help="Start class name for subtree export. Default: MainWindow",
    )
    parser.add_argument(
        "--include-qt-one-level",
        action="store_true",
        default=True,
        help="Include Qt member object types in tree as one-level nodes. Default: enabled",
    )
    parser.add_argument(
        "--json-out",
        default="build/mainwindow_object_tree.json",
        help="Output JSON path. Default: build/class_report.json",
    )
    parser.add_argument(
        "--md-out",
        default="build/mainwindow_object_tree.md",
        help="Output Markdown path. Default: build/class_report.md",
    )
    args = parser.parse_args()

    src_root = Path(args.src).resolve()
    if not src_root.exists():
        raise FileNotFoundError(f"Source folder does not exist: {src_root}")

    classes = parse_headers(src_root)
    impls = parse_cpp_impls(src_root)
    merge_impls(classes, impls)
    full_tree = (
        build_tree(classes)
        if args.tree_mode == "inheritance"
        else build_object_tree(classes, include_qt_one_level=args.include_qt_one_level)
    )

    start_node = args.start
    if start_node not in classes and start_node != "<root>":
        raise ValueError(f"Start class not found: {start_node}")

    if start_node not in full_tree and start_node != "<root>":
        # Keep empty branch if class exists but no outgoing edges.
        full_tree[start_node] = []

    tree = extract_subtree(full_tree, start_node)
    leaf_nodes = collect_leaf_nodes(tree, start_node)
    qt_one_level_nodes = sorted([n for n in tree.get(start_node, []) if re.match(r"^Q[A-Za-z_]\w*$", n)])

    json_out = Path(args.json_out)
    md_out = Path(args.md_out)
    json_out.parent.mkdir(parents=True, exist_ok=True)
    md_out.parent.mkdir(parents=True, exist_ok=True)

    report = {
        "source_root": str(src_root),
        "class_count": len(classes),
        "tree_mode": args.tree_mode,
        "start_node": start_node,
        "leaf_nodes": leaf_nodes,
        "qt_one_level_nodes": qt_one_level_nodes,
        "tree": tree,
        "classes": {k: class_to_dict(v) for k, v in sorted(classes.items())},
    }

    json_out.write_text(json.dumps(report, indent=2, ensure_ascii=False), encoding="utf-8")
    write_markdown(
        md_out,
        classes,
        tree,
        start_node,
        leaf_nodes,
        args.tree_mode,
        qt_one_level_nodes,
    )

    print(f"Exported {len(classes)} classes")
    print(f"JSON: {json_out}")
    print(f"Markdown: {md_out}")


if __name__ == "__main__":
    main()
