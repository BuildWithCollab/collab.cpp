#!/usr/bin/env python3
# Codegen for the dual-mode architecture (see MODULE_DUAL_MODE_CODEGEN.md).
#
# Input:  include/collab/<area>.hpp                (canonical inline header)
# Output: include/collab/detail/<area>.decls.hpp   (declarations only)
#         src/<area>.cppm                          (module partition)
#         src/<area>_impl.cpp                      (force-emission impl unit)
#
# Convention assumed of canonical headers:
#   1. One declaration per line at namespace scope.
#   2. `inline` is the first keyword on lines that declare inline functions
#      or inline variables (modulo `inline constexpr` / `inline consteval`
#      which keep their bodies in the decls header).
#   3. Namespaces open with explicit braces, one open/close per line.
#   4. Function bodies brace-balanced and parseable by a depth counter.
#   5. No `using namespace` at file scope.
#
# Usage:
#   python scripts/generate.py             # generate all areas
#   python scripts/generate.py log term    # generate only specified areas

from __future__ import annotations

import re
import sys
from dataclasses import dataclass, field
from pathlib import Path
from typing import Iterable

ROOT = Path(__file__).resolve().parent.parent
INCLUDE_DIR = ROOT / "include" / "collab"
DETAIL_DIR = INCLUDE_DIR / "detail"
SRC_DIR = ROOT / "src"

MODULE_NAME = "collab"
ROOT_NS = "collab"


# ── Entity records ──────────────────────────────────────────────────────────

@dataclass
class InlineFunction:
    namespace: str           # full namespace path, e.g. "collab::log"
    name: str                # bare function name, e.g. "set_level"
    return_type: str         # e.g. "void"
    params_typed: str        # parameter types only, e.g. "level"
    is_template: bool = False

@dataclass
class InlineVariable:
    namespace: str
    name: str
    type: str

@dataclass
class TypeEntity:
    namespace: str
    name: str                # struct/class/enum name

@dataclass
class TemplateEntity:
    namespace: str
    name: str

@dataclass
class FreeDeclaration:
    namespace: str
    name: str                # function name (for using-decl)


@dataclass
class AreaState:
    inline_functions: list[InlineFunction] = field(default_factory=list)
    inline_variables: list[InlineVariable] = field(default_factory=list)
    types: list[TypeEntity] = field(default_factory=list)
    templates: list[TemplateEntity] = field(default_factory=list)
    free_decls: list[FreeDeclaration] = field(default_factory=list)
    decls_lines: list[str] = field(default_factory=list)

    def using_names(self) -> list[tuple[str, str]]:
        """Returns [(namespace, name)] pairs for unique top-level entities,
        skipping detail:: namespaces (private by convention)."""
        seen: set[tuple[str, str]] = set()
        out: list[tuple[str, str]] = []
        sources = (
            [(t.namespace, t.name) for t in self.types] +
            [(t.namespace, t.name) for t in self.templates] +
            [(v.namespace, v.name) for v in self.inline_variables] +
            [(f.namespace, f.name) for f in self.inline_functions] +
            [(f.namespace, f.name) for f in self.free_decls]
        )
        for ns, name in sources:
            if "::detail" in ns or ns.endswith("detail"):
                continue
            # Specializations of std:: templates live in the decls header for
            # visibility (ADL / explicit lookup) but are not re-exported.
            if ns == "std" or ns.startswith("std::"):
                continue
            key = (ns, name)
            if key in seen:
                continue
            seen.add(key)
            out.append(key)
        return out


# ── Parser ──────────────────────────────────────────────────────────────────

class ParseError(Exception):
    def __init__(self, line_no: int, msg: str):
        super().__init__(f"line {line_no}: {msg}")
        self.line_no = line_no


def strip_param_names(params: str) -> str:
    """Convert a parenthesized parameter list into the bare-type form usable
    in a function pointer cast. Strips parameter names and default values.
    Naïve: assumes no function-pointer parameters."""
    inner = params.strip()
    if inner in ("", "void"):
        return ""
    # Split top-level commas (respecting angle brackets, parens).
    parts: list[str] = []
    depth = 0
    start = 0
    for i, c in enumerate(inner):
        if c in "<([":
            depth += 1
        elif c in ">)]":
            depth -= 1
        elif c == "," and depth == 0:
            parts.append(inner[start:i])
            start = i + 1
    parts.append(inner[start:])
    out_parts: list[str] = []
    for p in parts:
        p = p.strip()
        if "=" in p:
            p = p[: p.index("=")].rstrip()
        # Strip trailing identifier (the parameter name).
        m = re.search(r"[A-Za-z_][A-Za-z0-9_]*\s*$", p)
        if m and not p.rstrip().endswith(("&", "*", ">", "]", ")")):
            # If the type ends with a reference/pointer/template etc., there
            # was no name after it (anonymous parameter).
            stripped = p[: m.start()].rstrip()
            if stripped:
                p = stripped
        out_parts.append(p)
    return ", ".join(out_parts)


def parse(text: str) -> AreaState:
    state = AreaState()
    # Strip any user-written `#pragma once` — the generator inserts its own.
    text = re.sub(r"^\s*#pragma\s+once\s*\n", "", text, count=1, flags=re.MULTILINE)
    lines = text.splitlines(keepends=False)

    # Namespace stack: each entry is the dotted name (joined with ::).
    ns_stack: list[str] = []
    # Brace depth from `{`/`}` we've seen.
    brace_depth = 0
    # Brace depth corresponding to the bottom of the namespace stack.
    ns_brace_depths: list[int] = []

    i = 0
    while i < len(lines):
        raw_line = lines[i]
        stripped = raw_line.strip()
        line_no = i + 1

        # Namespace open: `namespace foo {` or `namespace foo::bar {`
        m_ns = re.match(r"namespace\s+([A-Za-z_][A-Za-z0-9_:]*)\s*\{\s*$", stripped)
        if m_ns and brace_depth == (ns_brace_depths[-1] if ns_brace_depths else 0):
            ns_name = m_ns.group(1)
            if ns_stack:
                full = ns_stack[-1] + "::" + ns_name
            else:
                full = ns_name
            ns_stack.append(full)
            brace_depth += 1
            ns_brace_depths.append(brace_depth)
            state.decls_lines.append(raw_line)
            i += 1
            continue

        # Namespace close: `}` or `}  // namespace foo`
        if (
            ns_stack
            and brace_depth == ns_brace_depths[-1]
            and re.match(r"\}\s*(//.*)?$", stripped)
        ):
            ns_stack.pop()
            ns_brace_depths.pop()
            brace_depth -= 1
            state.decls_lines.append(raw_line)
            i += 1
            continue

        at_ns_scope = brace_depth == (ns_brace_depths[-1] if ns_brace_depths else 0)
        current_ns = ns_stack[-1] if ns_stack else ""

        # At namespace scope, classify the line.
        if at_ns_scope:
            # Template (function or class) — keep verbatim through matching
            # `};` (class) or `}` (function) or trailing `;`.
            if re.match(r"template\s*<", stripped):
                start = i
                # Capture until the template head closes (matching `<...>`).
                # Then continue until the entity body or declaration ends.
                # We rely on brace tracking + a trailing `;` for the
                # declaration-only case.
                buf: list[str] = []
                local_brace = 0
                started_body = False
                # Walk forward until we've consumed the full template entity.
                while i < len(lines):
                    cur = lines[i]
                    buf.append(cur)
                    for c in _strip_strings_and_comments(cur):
                        if c == "{":
                            local_brace += 1
                            started_body = True
                        elif c == "}":
                            local_brace -= 1
                    if started_body and local_brace == 0:
                        i += 1
                        break
                    # Declaration-only template (no body) ends with `;`.
                    if not started_body and cur.rstrip().endswith(";"):
                        i += 1
                        break
                    i += 1
                # Record. Try to extract the name from the first non-template
                # line that contains either `struct`, `class`, or a function
                # signature.
                template_name = _extract_template_name(buf)
                if template_name:
                    state.templates.append(TemplateEntity(current_ns, template_name))
                for ln in buf:
                    state.decls_lines.append(ln)
                continue

            # struct/class/enum at namespace scope.
            m_type = re.match(
                r"(struct|class|enum(?:\s+class)?)\s+([A-Za-z_][A-Za-z0-9_]*)\b",
                stripped,
            )
            if m_type:
                kind = m_type.group(1)
                name = m_type.group(2)
                # Forward declaration ending with `;` on this line.
                if stripped.endswith(";") and "{" not in stripped:
                    state.decls_lines.append(raw_line)
                    state.types.append(TypeEntity(current_ns, name))
                    i += 1
                    continue
                # Block definition: keep verbatim until matching `};`.
                buf = []
                local_brace = 0
                started = False
                while i < len(lines):
                    cur = lines[i]
                    buf.append(cur)
                    for c in _strip_strings_and_comments(cur):
                        if c == "{":
                            local_brace += 1
                            started = True
                        elif c == "}":
                            local_brace -= 1
                    if started and local_brace == 0:
                        i += 1
                        break
                    i += 1
                state.types.append(TypeEntity(current_ns, name))
                for ln in buf:
                    state.decls_lines.append(ln)
                continue

            # inline constexpr / inline consteval — keep body verbatim.
            if re.match(r"inline\s+(constexpr|consteval)\b", stripped):
                # Capture through matching `}` or trailing `;`.
                buf, i = _consume_inline_definition(lines, i)
                # Record as an entity name for the cppm using-decl.
                name = _extract_inline_name(buf[0])
                if name:
                    # Treat as a variable if no `(` before `=` / `{`.
                    head = buf[0]
                    paren = head.find("(")
                    assign_or_brace = min(
                        (head.find(x) for x in ("=", "{") if x in head),
                        default=-1,
                    )
                    if paren != -1 and (assign_or_brace == -1 or paren < assign_or_brace):
                        # function — treat like a template (body stays).
                        state.templates.append(TemplateEntity(current_ns, name))
                    else:
                        # variable
                        t = _extract_variable_type(buf[0])
                        state.inline_variables.append(InlineVariable(current_ns, name, t))
                for ln in buf:
                    state.decls_lines.append(ln)
                continue

            # inline (plain) — function or variable.
            if re.match(r"inline\b", stripped):
                # Function vs variable: look for `(` vs `=`/`{` after the
                # `inline ` prefix.
                head_for_classify = stripped[len("inline"):].lstrip()
                # Walk the head looking for the first of: `(`, `=`, `{` at
                # depth 0.
                depth = 0
                kind = None
                pos = 0
                for j, c in enumerate(head_for_classify):
                    if c in "<(":
                        if c == "(" and depth == 0:
                            kind = "fn"
                            pos = j
                            break
                        depth += 1
                    elif c in ">)":
                        depth -= 1
                    elif c == "=" and depth == 0:
                        kind = "var"
                        pos = j
                        break
                    elif c == "{" and depth == 0:
                        kind = "var"  # brace-init variable
                        pos = j
                        break
                if kind == "fn":
                    buf, i = _consume_inline_definition(lines, i)
                    fn = _parse_inline_function(buf, current_ns, line_no)
                    if fn is not None:
                        state.inline_functions.append(fn)
                        decl_line = f"{fn.return_type} {fn.name}({fn.params_typed});"
                        state.decls_lines.append(decl_line)
                    continue
                elif kind == "var":
                    buf, i = _consume_inline_definition(lines, i)
                    name = _extract_inline_name(buf[0])
                    t = _extract_variable_type(buf[0])
                    if name and t:
                        state.inline_variables.append(InlineVariable(current_ns, name, t))
                        decl_line = f"extern {t} {name};"
                        state.decls_lines.append(decl_line)
                    continue
                else:
                    raise ParseError(line_no, f"could not classify inline line: {stripped!r}")

            # Namespace-scope type alias: `using NAME = ...;` — record as a
            # type entity so the module exports it.
            m_using = re.match(r"using\s+([A-Za-z_][A-Za-z0-9_]*)\s*=", stripped)
            if m_using and stripped.endswith(";"):
                state.types.append(TypeEntity(current_ns, m_using.group(1)))
                state.decls_lines.append(raw_line)
                i += 1
                continue

            # Non-inline function declaration (e.g. `std::unique_ptr<sink> foo();`).
            # Heuristic: ends with `);` and looks like a free function decl.
            if (
                stripped.endswith(");")
                and not stripped.startswith("//")
                and "(" in stripped
            ):
                paren = stripped.index("(")
                head = stripped[:paren].rstrip()
                name = _extract_function_name(head)
                if name:
                    state.free_decls.append(FreeDeclaration(current_ns, name))
                state.decls_lines.append(raw_line)
                i += 1
                continue

            # Everything else (comments, blank lines, #include lines, using
            # declarations, etc.) passes through.
            state.decls_lines.append(raw_line)
            i += 1
            continue

        # Below namespace scope (inside a type or function body) — pass through.
        # Update brace_depth based on the raw line.
        for c in _strip_strings_and_comments(raw_line):
            if c == "{":
                brace_depth += 1
            elif c == "}":
                brace_depth -= 1
        state.decls_lines.append(raw_line)
        i += 1

    return state


def _strip_strings_and_comments(line: str) -> str:
    """Return the line with string/char literals and // and /* */ comments
    blanked out, so brace counting only sees real braces."""
    out: list[str] = []
    i = 0
    n = len(line)
    while i < n:
        c = line[i]
        if c == "/" and i + 1 < n and line[i + 1] == "/":
            break
        if c == "/" and i + 1 < n and line[i + 1] == "*":
            j = line.find("*/", i + 2)
            if j == -1:
                break
            i = j + 2
            continue
        if c == '"' or c == "'":
            quote = c
            i += 1
            while i < n:
                if line[i] == "\\" and i + 1 < n:
                    i += 2
                    continue
                if line[i] == quote:
                    i += 1
                    break
                i += 1
            continue
        out.append(c)
        i += 1
    return "".join(out)


def _consume_inline_definition(lines: list[str], start: int) -> tuple[list[str], int]:
    """Consume lines from `start` until the inline definition's braces close
    (or, for a variable with `=` and no braces, until the trailing `;`).
    Returns (collected_lines, next_index)."""
    buf: list[str] = []
    local_brace = 0
    started = False
    i = start
    while i < len(lines):
        cur = lines[i]
        buf.append(cur)
        for c in _strip_strings_and_comments(cur):
            if c == "{":
                local_brace += 1
                started = True
            elif c == "}":
                local_brace -= 1
        if started and local_brace == 0:
            return buf, i + 1
        if not started and cur.rstrip().endswith(";"):
            return buf, i + 1
        i += 1
    raise ParseError(start + 1, "unterminated inline definition")


def _parse_inline_function(buf: list[str], ns: str, line_no: int) -> InlineFunction | None:
    """Parse the head of an inline function and return its record."""
    # Join lines up to the first `{` to get the full signature.
    joined = " ".join(line.strip() for line in buf)
    # Strip the leading `inline`.
    body_at = joined.find("{")
    if body_at == -1:
        return None
    head = joined[:body_at].strip()
    if head.startswith("inline"):
        head = head[len("inline"):].lstrip()
    # head is now `R name(args...)` possibly with trailing const/noexcept etc.
    # Find the outermost `(` for the parameter list.
    depth = 0
    open_paren = -1
    for j, c in enumerate(head):
        if c == "<":
            depth += 1
        elif c == ">":
            depth -= 1
        elif c == "(" and depth == 0:
            open_paren = j
            break
    if open_paren == -1:
        return None
    close_paren = _find_matching_close(head, open_paren)
    if close_paren == -1:
        return None
    params = head[open_paren + 1 : close_paren]
    before_paren = head[:open_paren].rstrip()
    m_name = re.search(r"([A-Za-z_][A-Za-z0-9_]*)$", before_paren)
    if not m_name:
        return None
    name = m_name.group(1)
    return_type = before_paren[: m_name.start()].rstrip()
    params_typed = strip_param_names(params)
    return InlineFunction(ns, name, return_type, params_typed)


def _find_matching_close(s: str, open_idx: int) -> int:
    depth = 0
    for i in range(open_idx, len(s)):
        if s[i] == "(":
            depth += 1
        elif s[i] == ")":
            depth -= 1
            if depth == 0:
                return i
    return -1


def _extract_template_name(buf: list[str]) -> str | None:
    """Extract the entity name from a template definition (class, function, or
    alias). Returns None for explicit/partial specializations (`std::hash<X>`
    etc.) — they belong in the decls header for visibility but are not
    re-exportable via using-decl."""
    joined = " ".join(line.strip() for line in buf)
    # Strip the template<...> header.
    m = re.match(r"template\s*<", joined)
    if not m:
        return None
    head = joined[m.end() - 1 :]  # starts at `<`
    close = _find_matching_close_generic(head, 0, "<", ">")
    if close == -1:
        return None
    rest = head[close + 1 :].lstrip()
    # Alias template: `template<...> using NAME = ...;`
    m_alias = re.match(r"using\s+([A-Za-z_][A-Za-z0-9_]*)\s*=", rest)
    if m_alias:
        return m_alias.group(1)
    # struct/class/enum — but skip specializations (qualified name or
    # trailing template-arg list).
    m_type = re.match(r"(struct|class|enum(?:\s+class)?)\s+([A-Za-z_][A-Za-z0-9_:]*)", rest)
    if m_type:
        name = m_type.group(2)
        if "::" in name:
            return None
        after = rest[m_type.end():].lstrip()
        if after.startswith("<"):
            return None
        return name
    # function: R name(...). Find first `(`, then last identifier before it.
    paren = rest.find("(")
    if paren != -1:
        before = rest[:paren].rstrip()
        m_name = re.search(r"([A-Za-z_][A-Za-z0-9_]*)$", before)
        if m_name:
            return m_name.group(1)
    return None


_OPERATOR_RE = re.compile(
    r"operator\s*"
    r"(?:<<=|>>=|<=>|<<|>>|==|!=|<=|>=|\+\+|--|->\*|->|&&|\|\||"
    r"<|>|\+|-|\*|/|%|&|\||\^|=|\(\s*\)|\[\s*\]|,|~|!)"
)


def _extract_function_name(head: str) -> str | None:
    """Pull the function name out of a head like `R name` or `R operator<<`.
    `head` is the text before the opening `(`."""
    h = head.rstrip()
    m = _OPERATOR_RE.search(h)
    if m and m.end() == len(h):
        return h[m.start():].replace(" ", "")
    m = re.search(r"([A-Za-z_][A-Za-z0-9_]*)$", h)
    return m.group(1) if m else None


def _find_matching_close_generic(s: str, open_idx: int, openc: str, closec: str) -> int:
    depth = 0
    for i in range(open_idx, len(s)):
        if s[i] == openc:
            depth += 1
        elif s[i] == closec:
            depth -= 1
            if depth == 0:
                return i
    return -1


def _extract_inline_name(line: str) -> str | None:
    """Pull the entity name out of an `inline ...` line, function or variable."""
    s = line.strip()
    if s.startswith("inline"):
        s = s[len("inline"):].lstrip()
    if s.startswith("constexpr"):
        s = s[len("constexpr"):].lstrip()
    elif s.startswith("consteval"):
        s = s[len("consteval"):].lstrip()
    # Function: find first `(`, name is last identifier before it.
    paren = s.find("(")
    eq = s.find("=")
    brace = s.find("{")
    candidates = [p for p in (paren, eq, brace) if p != -1]
    if not candidates:
        return None
    pos = min(candidates)
    if pos == paren and (eq == -1 or paren < eq) and (brace == -1 or paren < brace):
        before = s[:paren].rstrip()
        m = re.search(r"([A-Za-z_][A-Za-z0-9_]*)$", before)
        return m.group(1) if m else None
    # Variable: name is last identifier before `=` or `{`.
    before = s[:pos].rstrip()
    m = re.search(r"([A-Za-z_][A-Za-z0-9_]*)$", before)
    return m.group(1) if m else None


def _extract_variable_type(line: str) -> str | None:
    """Pull the type out of an `inline T name = value;` or `inline T name{...};` line."""
    s = line.strip()
    if s.startswith("inline"):
        s = s[len("inline"):].lstrip()
    if s.startswith("constexpr"):
        s = s[len("constexpr"):].lstrip()
    elif s.startswith("consteval"):
        s = s[len("consteval"):].lstrip()
    eq = s.find("=")
    brace = s.find("{")
    candidates = [p for p in (eq, brace) if p != -1]
    if not candidates:
        return None
    pos = min(candidates)
    before = s[:pos].rstrip()
    m = re.search(r"([A-Za-z_][A-Za-z0-9_]*)$", before)
    if not m:
        return None
    return before[: m.start()].rstrip()


# ── Emit ────────────────────────────────────────────────────────────────────

GEN_HEADER = "// Generated by scripts/generate.py — do not edit by hand.\n"


def emit_decls(area: str, state: AreaState) -> str:
    out = [GEN_HEADER, "#pragma once", ""]
    out.extend(state.decls_lines)
    if not out[-1].endswith("\n"):
        out.append("")
    return "\n".join(out) + "\n"


def emit_cppm(area: str, state: AreaState) -> str:
    pairs = state.using_names()
    # Group using-decls by namespace.
    by_ns: dict[str, list[str]] = {}
    for ns, name in pairs:
        by_ns.setdefault(ns, []).append(name)

    lines = [
        GEN_HEADER,
        "module;",
        "",
        f"#include <collab/detail/{area}.decls.hpp>",
        "",
        f"export module {MODULE_NAME}:{area};",
        "",
    ]
    for ns, names in by_ns.items():
        lines.append(f"export namespace {ns} {{")
        for name in names:
            lines.append(f"    using ::{ns}::{name};")
        lines.append("}")
        lines.append("")
    return "\n".join(lines).rstrip() + "\n"


def emit_impl(area: str, state: AreaState) -> str:
    lines = [
        GEN_HEADER,
        "module;",
        "",
        f"#include <collab/{area}.hpp>",
        "",
        f"module {MODULE_NAME};",
        "",
    ]
    if not state.inline_functions:
        # No inline functions to force-emit. Emit a placeholder TU body so the
        # impl unit is still a valid module interface attachment point.
        lines.append("// (no inline non-template functions to force-emit)")
        return "\n".join(lines) + "\n"

    # Emit using-directives for every namespace whose entities we cast. This
    # makes unqualified type names (e.g. `level`, `sink`) inside the casts
    # resolvable. The fully-qualified `&::ns::name` for the address itself is
    # always unambiguous regardless.
    used_namespaces: list[str] = []
    seen: set[str] = set()
    for fn in state.inline_functions:
        if fn.namespace and fn.namespace not in seen:
            seen.add(fn.namespace)
            used_namespaces.append(fn.namespace)

    lines.append("namespace {")
    for ns in used_namespaces:
        lines.append(f"using namespace ::{ns};")
    lines.append("#if defined(__GNUC__) || defined(__clang__)")
    lines.append("[[gnu::used]]")
    lines.append("#endif")
    lines.append(
        f"[[maybe_unused]] const void* const _emit_{area}_symbols[] = {{"
    )
    for fn in state.inline_functions:
        sig = f"{fn.return_type} (*)({fn.params_typed})"
        qualified = f"::{fn.namespace}::{fn.name}"
        lines.append(
            f"    reinterpret_cast<const void*>(static_cast<{sig}>(&{qualified})),"
        )
    lines.append("};")
    lines.append("}")
    return "\n".join(lines) + "\n"


# ── Driver ──────────────────────────────────────────────────────────────────

def discover_areas() -> list[str]:
    """All canonical inline headers in include/collab/*.hpp (excluding detail/)."""
    out: list[str] = []
    for p in sorted(INCLUDE_DIR.glob("*.hpp")):
        out.append(p.stem)
    return out


def generate_area(area: str) -> None:
    canonical = INCLUDE_DIR / f"{area}.hpp"
    if not canonical.is_file():
        raise SystemExit(f"no canonical header at {canonical}")
    text = canonical.read_text(encoding="utf-8")
    state = parse(text)

    DETAIL_DIR.mkdir(parents=True, exist_ok=True)
    SRC_DIR.mkdir(parents=True, exist_ok=True)

    (DETAIL_DIR / f"{area}.decls.hpp").write_text(emit_decls(area, state), encoding="utf-8")
    (SRC_DIR / f"{area}.cppm").write_text(emit_cppm(area, state), encoding="utf-8")
    (SRC_DIR / f"{area}_impl.cpp").write_text(emit_impl(area, state), encoding="utf-8")
    print(f"  generated {area}: "
          f"{len(state.inline_functions)} inline fns, "
          f"{len(state.inline_variables)} inline vars, "
          f"{len(state.types)} types, "
          f"{len(state.templates)} templates, "
          f"{len(state.free_decls)} free decls")


def main(argv: list[str]) -> int:
    areas: Iterable[str] = argv[1:] if len(argv) > 1 else discover_areas()
    for area in areas:
        generate_area(area)
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
