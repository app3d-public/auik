#!/usr/bin/env python3
import argparse
import csv
import re
import subprocess
import sys
from collections import OrderedDict
from dataclasses import dataclass, field
from pathlib import Path

try:
    import tinycss2
except ImportError as exc:
    print("theme_compile.py requires tinycss2. Install it for the Python used to run this script.", file=sys.stderr)
    raise SystemExit(1) from exc

try:
    from jinja2 import Template
except ImportError as exc:
    print("theme_compile.py requires jinja2. Install it for the Python used to run this script.", file=sys.stderr)
    raise SystemExit(1) from exc


CLASS_RE = re.compile(r"\.([A-Za-z_][A-Za-z0-9_-]*)")
STATE_RE = re.compile(r":(hover|active|focus)\b")
SIGN_RE = re.compile(r"0x[0-9A-Fa-f]+")
VIRTUAL_VARIABLES = {"--system-font", "--system-font-bold", "--dpi"}
VIRTUAL_VALUES = {
    "--system-font": "fonts[AUIK_THEME_FONT_REGULAR]",
    "--system-font-bold": "fonts[AUIK_THEME_FONT_BOLD]",
    "--dpi": "dpi",
}
STYLE_STATES = {
    "hover": "StyleState::hover",
    "active": "StyleState::active",
    "focus": "StyleState::focus",
}
DEFAULT_IDS_CSV_NAME = "default_style_tags_id.csv"
APP_IDS_CSV_NAME = "style_tags_id.csv"
THEME_STYLE_SHEET_HEADER_NAME = "theme_style_sheet.hpp"
THEME_STYLE_SHEET_SOURCE_NAME = "theme_style_sheet.cpp"


@dataclass
class CssDeclaration:
    name: str
    value: str
    tokens: list = field(default_factory=list)


@dataclass
class CssRule:
    selectors: list[str]
    declarations: list[CssDeclaration] = field(default_factory=list)
    source: Path | None = None


@dataclass
class ThemeTree:
    rules: list[CssRule] = field(default_factory=list)
    tags: set[str] = field(default_factory=set)
    variables: set[str] = field(default_factory=set)


def parse_css(path: Path) -> list[CssRule]:
    stylesheet = tinycss2.parse_stylesheet(path.read_text(encoding="utf-8"), skip_comments=True, skip_whitespace=True)
    rules: list[CssRule] = []

    for item in stylesheet:
        if item.type != "qualified-rule":
            continue

        selector_text = tinycss2.serialize(item.prelude).strip()
        selectors = [selector.strip() for selector in selector_text.split(",") if selector.strip()]
        declarations: list[CssDeclaration] = []

        for declaration in tinycss2.parse_declaration_list(item.content, skip_comments=True, skip_whitespace=True):
            if declaration.type != "declaration":
                continue
            declarations.append(
                CssDeclaration(
                    name=declaration.name,
                    value=tinycss2.serialize(declaration.value).strip(),
                    tokens=list(declaration.value),
                )
            )

        rules.append(CssRule(selectors=selectors, declarations=declarations, source=path))

    return rules


def selector_tag(selector: str) -> str | None:
    match = CLASS_RE.search(selector)
    return match.group(1) if match else None


def selector_state(selector: str) -> str | None:
    match = STATE_RE.search(selector)
    return match.group(1) if match else None


def collect_tree(css_files: list[Path]) -> ThemeTree:
    tree = ThemeTree()
    for path in css_files:
        for rule in parse_css(path):
            tree.rules.append(rule)
            for selector in rule.selectors:
                tag = selector_tag(selector)
                if tag:
                    tree.tags.add(tag)
            for declaration in rule.declarations:
                if declaration.name.startswith("--") and declaration.name not in VIRTUAL_VARIABLES:
                    tree.variables.add(declaration.name[2:])
                for variable in collect_var_refs(declaration.tokens):
                    if variable not in VIRTUAL_VARIABLES:
                        tree.variables.add(variable[2:])
    return tree


def read_ids(path: Path) -> dict[str, str]:
    if not path.exists():
        return {}
    with path.open("r", encoding="utf-8", newline="") as file:
        reader = csv.DictReader(file)
        if reader.fieldnames != ["Tag", "ID"]:
            raise ValueError(f"{path}: expected CSV header 'Tag,ID'")
        return {row["Tag"].strip(): row["ID"].strip() for row in reader if row.get("Tag") and row.get("ID")}


def write_ids(path: Path, ids: dict[str, str]) -> None:
    lines = ["Tag,ID\n"]
    lines.extend(f"{key},{ids[key]}\n" for key in sorted(ids))
    write_text_if_changed(path, "".join(lines))


def write_text_if_changed(path: Path, text: str) -> bool:
    if path.exists() and path.read_text(encoding="utf-8") == text:
        return False
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(text, encoding="utf-8")
    return True


def touch(path: Path) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.touch()


def local_umbf_sign_request_path() -> str:
    candidate = Path(__file__).resolve().parents[2] / "umbf" / "scripts" / "sign_request.py"
    return str(candidate)


def sign_request_command(script_path: str) -> tuple[list[str], str]:
    return [sys.executable, script_path, "u32"], f"{sys.executable} {script_path} u32"


def generate_id_with_sign_request(preferred_path: str = "") -> str:
    script_path = preferred_path or local_umbf_sign_request_path()
    command, command_text = sign_request_command(script_path)
    try:
        result = subprocess.run(command, check=False, capture_output=True, text=True)
    except OSError as exc:
        raise RuntimeError(f"Failed to generate id via {command_text}: {exc}") from exc

    if result.returncode != 0:
        output = (result.stderr or result.stdout).strip()
        raise RuntimeError(f"Failed to generate id via {command_text}: exited with code {result.returncode}: {output}")

    match = SIGN_RE.search(result.stdout)
    if not match:
        raise RuntimeError(f"Failed to generate id via {command_text}: did not print a u32 signature: {result.stdout.strip()}")

    return match.group(0).upper().replace("X", "x")


def generate_unique_id(used_ids: set[str], sign_request_path: str = "") -> str:
    for _ in range(128):
        value = generate_id_with_sign_request(sign_request_path)
        if value != "0x00000000" and value not in used_ids:
            return value
    raise RuntimeError("Failed to generate a unique id via sign_request.py after 128 attempts")


def update_ids(csv_path: Path, names: set[str], used_ids: set[str] | None = None, sign_request_path: str = "") -> dict[str, str]:
    old_ids = read_ids(csv_path)
    ids = {name: old_ids[name] for name in sorted(names) if name in old_ids}
    used = set(used_ids or set()) | set(ids.values())
    for name in sorted(names):
        if name in ids:
            continue
        ids[name] = generate_unique_id(used, sign_request_path)
        used.add(ids[name])
    write_ids(csv_path, ids)
    return ids


def merge_ids(base_ids: dict[str, str], app_ids: dict[str, str]) -> dict[str, str]:
    merged = dict(base_ids)
    merged.update(app_ids)
    return merged


def macro_suffix(name: str) -> str:
    return re.sub(r"[^A-Za-z0-9]+", "_", name).strip("_").upper()


def tag_macro(name: str) -> str:
    return f"AUIK_STYLE_TAG_{macro_suffix(name)}"


def var_macro(name: str) -> str:
    return f"AUIK_STYLE_VAR_{macro_suffix(name)}"


def local_var_name(name: str) -> str:
    return "style_var_" + re.sub(r"[^A-Za-z0-9]+", "_", name).strip("_").lower()


def fmt_float(value: float) -> str:
    text = f"{value:.8g}"
    if "e" not in text and "." not in text:
        text += ".0"
    return f"{text}f"


def clean_tokens(tokens: list) -> list:
    return [token for token in tokens if token.type not in ("whitespace", "comment")]


def split_words(tokens: list) -> list[list]:
    words: list[list] = []
    current: list = []
    for token in tokens:
        if token.type == "whitespace":
            if current:
                words.append(current)
                current = []
            continue
        current.append(token)
    if current:
        words.append(current)
    return words


def split_commas(tokens: list) -> list[list]:
    chunks: list[list] = []
    current: list = []
    for token in tokens:
        if token.type == "literal" and getattr(token, "value", "") == ",":
            chunks.append(clean_tokens(current))
            current = []
        else:
            current.append(token)
    chunks.append(clean_tokens(current))
    return [chunk for chunk in chunks if chunk]


def collect_var_refs(tokens: list) -> set[str]:
    refs: set[str] = set()
    for token in tokens:
        if token.type == "function" and token.name == "var":
            args = clean_tokens(token.arguments)
            if args and args[0].type == "ident":
                refs.add(args[0].value)
        elif token.type == "function":
            refs.update(collect_var_refs(token.arguments))
    return refs


def alpha_to_u8(expr: str) -> str:
    try:
        value = float(expr.rstrip("f"))
    except ValueError:
        return expr
    if value <= 1.0:
        value *= 255.0
    return str(max(0, min(255, int(round(value)))))


def color_channel_to_u8(tokens: list, variables: dict[str, str]) -> str:
    expr = resolve_value(tokens, variables)
    try:
        value = float(expr.rstrip("f"))
    except ValueError:
        return expr
    return str(max(0, min(255, int(round(value)))))


def resolve_function(token, variables: dict[str, str]) -> str:
    name = token.name.lower()
    args = clean_tokens(token.arguments)

    if name == "var":
        if not args or args[0].type != "ident":
            raise ValueError("var() expects a CSS variable name")
        var_name = args[0].value
        if var_name in VIRTUAL_VALUES:
            return VIRTUAL_VALUES[var_name]
        return variables[var_name[2:]]

    if name == "calc":
        return resolve_calc(args, variables)

    if name in ("rgb", "rgba"):
        channels = split_commas(args)
        if len(channels) < 3:
            raise ValueError(f"{name}() expects at least three channels")
        r = color_channel_to_u8(channels[0], variables)
        g = color_channel_to_u8(channels[1], variables)
        b = color_channel_to_u8(channels[2], variables)
        alpha = resolve_value(channels[3], variables) if len(channels) > 3 else "255"
        return f"amal::rgba8_to_vec4({r}, {g}, {b}, {alpha_to_u8(alpha)})"

    raise ValueError(f"Unsupported function {name}()")


def resolve_value(tokens: list, variables: dict[str, str]) -> str:
    tokens = clean_tokens(tokens)
    if len(tokens) == 1:
        token = tokens[0]
        if token.type == "number":
            return fmt_float(float(token.value))
        if token.type == "dimension":
            unit = token.unit.lower()
            if unit == "px":
                return fmt_float(float(token.value))
            if unit == "pt":
                return f"pt_to_px({fmt_float(float(token.value))}, dpi)"
            raise ValueError(f"Unsupported unit '{token.unit}'")
        if token.type == "ident":
            return token.value
        if token.type == "function":
            return resolve_function(token, variables)

    return tinycss2.serialize(tokens).strip()


def resolve_calc(tokens: list, variables: dict[str, str]) -> str:
    parts: list[str] = []
    for token in tokens:
        if token.type == "whitespace":
            parts.append(" ")
        elif token.type == "literal":
            parts.append(token.value)
        elif token.type in ("number", "dimension", "ident", "function"):
            parts.append(resolve_value([token], variables))
        else:
            parts.append(tinycss2.serialize([token]).strip())
    return "".join(parts).strip()


def resolve_box(tokens: list, variables: dict[str, str], ctor: str) -> str:
    words = split_words(tokens)
    values = [resolve_value(word, variables) for word in words]
    if len(values) == 1:
        return f"amal::vec2{{{values[0]}}}"
    if len(values) == 2:
        return f"amal::vec2{{{values[1]}, {values[0]}}}"
    if len(values) == 4:
        return f"amal::vec4{{{values[3]}, {values[0]}, {values[1]}, {values[2]}}}"
    raise ValueError(f"{ctor} expects 1, 2, or 4 values")


def resolve_box_values(tokens: list, variables: dict[str, str], ctor: str) -> list[str]:
    words = split_words(tokens)
    values = [resolve_value(word, variables) for word in words]
    if len(values) == 1:
        return [values[0], values[0], values[0], values[0]]
    if len(values) == 2:
        return [values[1], values[0], values[1], values[0]]
    if len(values) == 4:
        return [values[3], values[0], values[1], values[2]]
    raise ValueError(f"{ctor} expects 1, 2, or 4 values")


def resolve_box_side(tokens: list, variables: dict[str, str], ctor: str, side: str) -> str:
    words = split_words(tokens)
    if len(words) != 1:
        raise ValueError(f"{ctor}-{side} expects 1 value")
    return resolve_value(words[0], variables)


def resolve_border_radius(tokens: list, variables: dict[str, str]) -> list[str]:
    words = split_words(tokens)
    values = [resolve_value(word, variables) for word in words]
    if len(values) == 1:
        return [f"border_radius({values[0]})"]
    if len(values) != 4:
        raise ValueError("border-radius expects 1 or 4 values")

    mask = 0
    radius = "0.0f"
    for index, value in enumerate(values):
        if value not in ("0", "0.0f"):
            mask |= 1 << index
            if radius == "0.0f":
                radius = value
    return [f"border_radius({radius})", f"corner_mask(0x{mask:X}u)"]


def resolve_border(tokens: list, variables: dict[str, str]) -> list[str]:
    words = split_words(tokens)
    thickness = None
    color = None
    for word in words:
        if any(token.type == "function" and token.name.lower() in ("rgb", "rgba", "var") for token in word):
            color = resolve_value(word, variables)
        elif any(token.type in ("dimension", "number") for token in word):
            thickness = resolve_value(word, variables)
    calls: list[str] = []
    if color:
        calls.append(f"border_color({color})")
    if thickness:
        calls.append(f"border_thickness({thickness})")
    return calls


def single_ident(tokens: list) -> str | None:
    words = split_words(tokens)
    if len(words) != 1 or len(words[0]) != 1:
        return None
    token = words[0][0]
    return token.value.lower() if token.type == "ident" else None


def resolve_axis_size(tokens: list, variables: dict[str, str], axis: str) -> str:
    ident = single_ident(tokens)
    if ident == "min-content":
        return "AUIK_SIZE_X_MIN_FIT" if axis == "x" else "AUIK_SIZE_Y_MIN_FIT"
    if ident == "fit-content":
        return "AUIK_SIZE_X_MIN_FIT_REQUIRE" if axis == "x" else "AUIK_SIZE_Y_MIN_FIT_REQUIRE"
    if ident == "stretch":
        return "AUIK_SIZE_X_FILL" if axis == "x" else "AUIK_SIZE_Y_FILL"
    return resolve_value(tokens, variables)


def align_flag_for_display(value: str) -> str:
    if value == "block":
        return "ChildLayoutFlagBits::block"
    if value == "inline":
        return "ChildLayoutFlagBits::linline"
    raise ValueError(f"Unsupported display value '{value}'")


def align_flag_for_text_align(value: str) -> str:
    if value == "left":
        return "ChildLayoutFlagBits::hleft"
    if value == "center":
        return "ChildLayoutFlagBits::hcenter"
    if value == "right":
        return "ChildLayoutFlagBits::aright"
    raise ValueError(f"Unsupported text-align value '{value}'")


def align_flag_for_vertical_align(value: str) -> str:
    if value == "top":
        return "ChildLayoutFlagBits::top"
    if value == "middle":
        return "ChildLayoutFlagBits::vcenter"
    if value == "bottom":
        return "ChildLayoutFlagBits::bottom"
    raise ValueError(f"Unsupported layout vertical-align value '{value}'")


def text_wrap_for_white_space(value: str) -> str:
    if value == "nowrap":
        return "static_cast<TextWrapMode>(0u)"
    if value == "normal":
        return "static_cast<TextWrapMode>(1u)"
    raise ValueError(f"Unsupported white-space value '{value}'")


def text_overflow_for_value(value: str) -> str:
    if value == "ellipsis":
        return "static_cast<TextOverflowMode>(1u)"
    if value == "clip":
        return "static_cast<TextOverflowMode>(0u)"
    raise ValueError(f"Unsupported text-overflow value '{value}'")


def declaration_calls(declaration: CssDeclaration, variables: dict[str, str]) -> list[str]:
    name = declaration.name
    tokens = declaration.tokens
    if name == "padding":
        return [f"padding({resolve_box(tokens, variables, name)})"]
    if name == "margin":
        return [f"margin({resolve_box(tokens, variables, name)})"]
    if name in ("padding-left", "padding-top", "padding-right", "padding-bottom"):
        return []
    if name in ("margin-left", "margin-top", "margin-right", "margin-bottom"):
        return []
    if name == "background-color":
        return [f"background_color({resolve_value(tokens, variables)})"]
    if name == "color":
        return [f"text_color({resolve_value(tokens, variables)})"]
    if name == "font-size":
        return [f"text_size({resolve_value(tokens, variables)})"]
    if name == "font-family":
        return [f"font({resolve_value(tokens, variables)})"]
    if name == "inline-spacing":
        return [f"inline_spacing({resolve_value(tokens, variables)})"]
    if name == "width":
        return [f"width({resolve_axis_size(tokens, variables, 'x')})"]
    if name == "height":
        return [f"height({resolve_axis_size(tokens, variables, 'y')})"]
    if name == "min-width":
        return [f"min_width({resolve_value(tokens, variables)})"]
    if name == "min-height":
        return [f"min_height({resolve_value(tokens, variables)})"]
    if name == "border-radius":
        return resolve_border_radius(tokens, variables)
    if name == "border":
        return resolve_border(tokens, variables)
    if name == "border-color":
        return [f"border_color({resolve_value(tokens, variables)})"]
    if name == "border-thickness":
        return [f"border_thickness({resolve_value(tokens, variables)})"]
    if name.startswith("--"):
        return []
    raise ValueError(f"Unsupported CSS property '{name}'")


def style_expression(calls: list[str]) -> str:
    if not calls:
        return "make_style()"
    lines = ["make_style()"]
    lines.extend(f"            .{call}" for call in calls)
    return "\n".join(lines)


BOX_SIDE_INDEX = {
    "left": 0,
    "top": 1,
    "right": 2,
    "bottom": 3,
}


def update_box_property(style_rule: dict, name: str, tokens: list, variables: dict[str, str]) -> bool:
    ctor = ""
    if name == "padding" or name.startswith("padding-"):
        ctor = "padding"
    elif name == "margin" or name.startswith("margin-"):
        ctor = "margin"
    else:
        return False

    boxes = style_rule.setdefault("boxes", {})
    values = boxes.get(ctor, ["0.0f", "0.0f", "0.0f", "0.0f"])
    if name == ctor:
        values = resolve_box_values(tokens, variables, ctor)
    else:
        side = name.removeprefix(f"{ctor}-")
        if side not in BOX_SIDE_INDEX:
            return False
        values[BOX_SIDE_INDEX[side]] = resolve_box_side(tokens, variables, ctor, side)
    boxes[ctor] = values
    style_rule["properties"][ctor] = [f"{ctor}(amal::vec4{{{values[0]}, {values[1]}, {values[2]}, {values[3]}}})"]
    return True


def update_extra_property(style_rule: dict, declaration: CssDeclaration, variables: dict[str, str]) -> bool:
    name = declaration.name
    if name not in ("display", "position", "text-align", "vertical-align", "white-space", "text-overflow",
                    "left", "top", "right", "bottom"):
        return False

    extras = style_rule.setdefault("extras", {})
    if name == "position":
        value = single_ident(declaration.tokens)
        if value != "absolute":
            raise ValueError(f"{name} supports only 'absolute'")
        align = extras.setdefault("align", {})
        align["absolute"] = True
    elif name == "display":
        value = single_ident(declaration.tokens)
        if not value:
            raise ValueError(f"{name} expects a single keyword")
        align = extras.setdefault("align", {})
        align["display"] = align_flag_for_display(value)
    elif name == "text-align":
        value = single_ident(declaration.tokens)
        if not value:
            raise ValueError(f"{name} expects a single keyword")
        align = extras.setdefault("align", {})
        align["h"] = align_flag_for_text_align(value)
    elif name == "vertical-align":
        value = single_ident(declaration.tokens)
        if not value:
            raise ValueError(f"{name} expects a single keyword")
        align = extras.setdefault("align", {})
        align["v"] = align_flag_for_vertical_align(value)
    elif name == "white-space":
        value = single_ident(declaration.tokens)
        if not value:
            raise ValueError(f"{name} expects a single keyword")
        text = extras.setdefault("text", {})
        text["wrap"] = text_wrap_for_white_space(value)
    elif name == "text-overflow":
        value = single_ident(declaration.tokens)
        if not value:
            raise ValueError(f"{name} expects a single keyword")
        text = extras.setdefault("text", {})
        text["overflow"] = text_overflow_for_value(value)
    elif name in ("left", "top", "right", "bottom"):
        align = extras.setdefault("align", {})
        align["absolute"] = True
        position = align.setdefault("position", {})
        position[name] = resolve_value(declaration.tokens, variables)
    return True


def extra_calls(style_rule: dict) -> list[str]:
    extras = style_rule.get("extras", {})
    calls: list[str] = []
    align = extras.get("align")
    if align:
        flags: list[str] = []
        for key in ("display", "h", "v"):
            value = align.get(key)
            if value and value != "0u":
                flags.append(value)
        if align.get("absolute"):
            flags.append("ChildLayoutFlagBits::absolute")
        flag_expr = " | ".join(flags) if flags else "0u"
        position = align.get("position", {})
        position_values = {
            "left": position.get("left", "0.0f"),
            "top": position.get("top", "0.0f"),
            "right": position.get("right", "0.0f"),
            "bottom": position.get("bottom", "0.0f"),
        }
        calls.append(
            "align_extra(StyleExtraAlign{"
            f"static_cast<u32>({flag_expr}), "
            f"amal::vec4{{{position_values['left']}, {position_values['top']}, "
            f"{position_values['right']}, {position_values['bottom']}" + "}})"
        )
    text = extras.get("text")
    if text:
        wrap = text.get("wrap", "static_cast<TextWrapMode>(0u)")
        overflow = text.get("overflow", "static_cast<TextOverflowMode>(1u)")
        calls.append(f"text_extra(StyleExtraText{{{wrap}, {overflow}}})")
    return calls


def build_defines(ids: dict[str, str], variable_names: set[str]) -> list[dict[str, str]]:
    defines = []
    for name, value in sorted(ids.items()):
        if name in variable_names:
            macro = var_macro(name)
        else:
            macro = tag_macro(name)
        defines.append({"name": macro, "value": value})
    return defines


def build_generated_model(tree: ThemeTree, ids: dict[str, str], header_ids: dict[str, str], include_headers: list[str]) -> dict:
    variables = {name: local_var_name(name) for name in tree.variables}
    var_def_map: OrderedDict[str, dict[str, str]] = OrderedDict()
    style_rule_map: OrderedDict[tuple[str, str], dict] = OrderedDict()

    for rule in tree.rules:
        is_root = any(selector == ":root" for selector in rule.selectors)
        if is_root:
            for declaration in rule.declarations:
                if not declaration.name.startswith("--") or declaration.name in VIRTUAL_VARIABLES:
                    continue
                name = declaration.name[2:]
                var_def_map[name] = {
                    "name": variables[name],
                    "macro": var_macro(name),
                    "value": resolve_value(declaration.tokens, variables),
                }
            continue

        for selector in rule.selectors:
            tag = selector_tag(selector)
            if not tag:
                continue
            state = selector_state(selector) or ""
            key = (tag, state)
            style_rule = style_rule_map.setdefault(
                key,
                {
                    "tag_macro": tag_macro(tag),
                    "properties": OrderedDict(),
                    "extras": {},
                    "source_declarations": rule.declarations,
                    "state": STYLE_STATES.get(state, ""),
                },
            )
            for declaration in rule.declarations:
                if update_box_property(style_rule, declaration.name, declaration.tokens, variables):
                    continue
                if update_extra_property(style_rule, declaration, variables):
                    continue
                calls = declaration_calls(declaration, variables)
                if calls:
                    style_rule["properties"][declaration.name] = calls

    style_rules: list[dict[str, str]] = []
    for style_rule in style_rule_map.values():
        calls: list[str] = []
        for property_calls in style_rule["properties"].values():
            calls.extend(property_calls)
        calls.extend(extra_calls(style_rule))
        if not calls:
            continue
        style_rules.append(
            {
                "tag_macro": style_rule["tag_macro"],
                "style": style_expression(calls),
                "state": style_rule["state"],
            }
        )

    header_variables = tree.variables & set(header_ids)

    return {
        "defines": build_defines(header_ids, header_variables),
        "include_headers": include_headers,
        "variables": list(var_def_map.values()),
        "rules": style_rules,
    }


def write_outputs(output_folder: Path, model: dict) -> None:
    output_folder.mkdir(parents=True, exist_ok=True)
    header = output_folder / THEME_STYLE_SHEET_HEADER_NAME
    source = output_folder / THEME_STYLE_SHEET_SOURCE_NAME

    header_template = Template(
        """#pragma once

#include <auik/theme.hpp>
{% if defines %}
{% for define in defines %}
#define {{ define.name }} {{ define.value }}u
{% endfor %}
{% endif %}

namespace auik
{
    Theme *create_style_sheet_theme(Font **fonts, f32 dpi);
} // namespace auik
"""
    )
    source_template = Template(
        """#include \"""" + THEME_STYLE_SHEET_HEADER_NAME + """\"

#include <acul/memory/alloc.hpp>
#include <amal/color.hpp>
#include <auik/auik.hpp>
#include <auik/theme.hpp>
{% for header in include_headers %}
#include "{{ header }}"
{% endfor %}

namespace auik
{
    Theme *create_style_sheet_theme(Font **fonts, f32 dpi)
    {
        auto *theme = acul::alloc<Theme>();
{% for variable in variables %}
        const auto {{ variable.name }} = {{ variable.value }};
        theme->set_var({{ variable.macro }}, {{ variable.name }});
{% endfor %}
{% for rule in rules %}
        theme->add_style({{ rule.tag_macro }}, {{ rule.style }}{% if rule.state %},
                         {{ rule.state }}{% endif %});
{% endfor %}
        return theme;
    }
} // namespace auik
"""
    )

    write_text_if_changed(header, header_template.render(defines=model["defines"]))
    write_text_if_changed(
        source,
        source_template.render(
            include_headers=model["include_headers"],
            variables=model["variables"],
            rules=model["rules"],
        ),
    )


def write_ids_header(path: Path, ids: dict[str, str], variable_names: set[str]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    defines = build_defines(ids, variable_names)

    template = Template(
        """#pragma once

{% for define in defines -%}
#define {{ define.name }} {{ define.value }}u
{% endfor %}
"""
    )
    write_text_if_changed(path, template.render(defines=defines))


def main() -> int:
    parser = argparse.ArgumentParser(description="Compile auik CSS theme sources.")
    parser.add_argument("--input-folder", action="append", type=Path, help="Folder with project CSS files")
    parser.add_argument("--input-base", required=True, type=Path, help="Base CSS file")
    parser.add_argument("--ids-csv", action="append", type=Path, default=[], help="Additional predeclared style ids CSV")
    parser.add_argument(
        "--processed-ids",
        action="append",
        nargs=2,
        metavar=("IDS_CSV", "HEADER"),
        default=[],
        help="Style ids CSV and generated header already owned by a module",
    )
    parser.add_argument("--ids-output-csv", type=Path, help="Path to the ids CSV updated from --input-base")
    parser.add_argument("--output-folder", type=Path, help="Folder for generated hpp/cpp")
    parser.add_argument("--ids-header", type=Path, help="Generate default widget/style tag defines from the base CSS id cache")
    parser.add_argument("--ids-only", action="store_true", help="Only update id caches and generate --ids-header")
    parser.add_argument("--sign-request", type=Path, help="Path to umbf scripts/sign_request.py")
    parser.add_argument("--stamp", type=Path, help="Touch a stamp file after successful generation")
    args = parser.parse_args()

    input_base = args.input_base.resolve()
    input_folders = [path.resolve() for path in args.input_folder] if args.input_folder else [input_base.parent]
    output_folder = args.output_folder.resolve() if args.output_folder else None
    sign_request_path = str(args.sign_request.resolve()) if args.sign_request else ""
    if not args.ids_only and output_folder is None:
        parser.error("--output-folder is required unless --ids-only is used")
    if args.ids_only and not args.ids_header:
        parser.error("--ids-only requires --ids-header")

    if not input_base.exists():
        raise FileNotFoundError(f"Base CSS not found: {input_base}")
    for input_folder in input_folders:
        if not input_folder.exists():
            raise FileNotFoundError(f"Input folder not found: {input_folder}")
    for ids_csv in args.ids_csv:
        if not ids_csv.exists():
            raise FileNotFoundError(f"Style ids CSV not found: {ids_csv}")
    for ids_csv, header in args.processed_ids:
        if not Path(ids_csv).exists():
            raise FileNotFoundError(f"Processed style ids CSV not found: {ids_csv}")
        if not Path(header).exists():
            raise FileNotFoundError(f"Processed style ids header not found: {header}")

    base_css_files = [input_base]
    app_css_files = []
    for input_folder in input_folders:
        folder_files = [path for path in sorted(input_folder.rglob("*.css")) if path.resolve() != input_base]
        if input_folder == input_base.parent:
            base_css_files.extend(folder_files)
        else:
            app_css_files.extend(folder_files)

    css_files = base_css_files + app_css_files
    base_tree = collect_tree(base_css_files)
    app_tree = collect_tree(app_css_files)
    tree = collect_tree(css_files)

    processed_ids = {}
    external_header_ids = {}
    include_headers = []
    for ids_csv, header in args.processed_ids:
        processed_ids.update(read_ids(Path(ids_csv).resolve()))
        include_headers.append(str(Path(header).resolve()).replace("\\", "/"))
    for ids_csv in args.ids_csv:
        external_header_ids.update(read_ids(ids_csv.resolve()))

    base_csv_path = args.ids_output_csv.resolve() if args.ids_output_csv else input_base.parent / DEFAULT_IDS_CSV_NAME
    base_names = base_tree.tags | base_tree.variables
    if processed_ids or external_header_ids:
        base_names = base_names - set(processed_ids) - set(external_header_ids)
    if args.ids_only:
        base_ids = update_ids(base_csv_path, base_names, sign_request_path=sign_request_path)
    else:
        base_ids = read_ids(base_csv_path)

    app_csv_path = None
    app_ids = {}
    app_names = (app_tree.tags | app_tree.variables) - set(base_ids) - set(processed_ids) - set(external_header_ids)
    if app_names:
        if len(input_folders) == 1:
            app_csv_path = input_folders[0] / APP_IDS_CSV_NAME
        else:
            app_csv_path = output_folder / APP_IDS_CSV_NAME
        app_ids = update_ids(
            app_csv_path,
            app_names,
            set(base_ids.values()) | set(processed_ids.values()) | set(external_header_ids.values()),
            sign_request_path,
        )

    app_header_ids = merge_ids(external_header_ids, app_ids)
    ids = merge_ids(merge_ids(base_ids, processed_ids), app_header_ids)

    if not args.ids_only:
        model = build_generated_model(tree, ids, app_header_ids, include_headers)
        write_outputs(output_folder, model)
    if args.ids_header:
        write_ids_header(args.ids_header.resolve(), base_ids, base_tree.variables)
    if args.stamp:
        touch(args.stamp.resolve())
    print(f"Parsed {len(css_files)} css file(s), {len(tree.rules)} rule(s)")
    print(f"Updated base ids: {base_csv_path}")
    if app_csv_path:
        print(f"Updated app ids: {app_csv_path}")
    if args.ids_header:
        print(f"Generated ids header: {args.ids_header.resolve()}")
    if not args.ids_only:
        print(f"Generated: {output_folder / THEME_STYLE_SHEET_HEADER_NAME}")
        print(f"Generated: {output_folder / THEME_STYLE_SHEET_SOURCE_NAME}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
