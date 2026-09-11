"""ASD syntax and inheritance. No executable expressions or third-party parser."""
from dataclasses import dataclass, field
from pathlib import Path
import re


@dataclass
class Token:
    type: str
    value: object = ""
    unit: str = ""
    name: str = ""
    arguments: list = field(default_factory=list)


def values(text):
    """Parse ASD values into typed scalar, tuple, reference and expression tokens."""
    result = []
    i = 0
    while i < len(text):
        c = text[i]
        if c.isspace():
            while i < len(text) and text[i].isspace(): i += 1
            result.append(Token("whitespace", " "))
            continue
        if c in "@(":
            external = c == "@"
            if external: i += 1
            match = re.match(r"[A-Za-z_][\w-]*", text[i:]) if external else None
            name = match[0] if match else ""
            if match: i += len(name)
            if i < len(text) and text[i] == "(":
                start = i + 1
                depth = 1
                i += 1
                while i < len(text) and depth:
                    if text[i] == "(": depth += 1
                    elif text[i] == ")": depth -= 1
                    i += 1
                if depth: raise ValueError("Unclosed value parentheses")
                args = values(text[start:i-1])
                if not external:
                    result.append(Token("tuple", arguments=args))
                else:
                    if name not in ("", "font"): raise ValueError(f"Unknown function @{name}")
                    result.append(Token("function", name=name or "calc", arguments=args))
            elif name:
                result.append(Token("function", name="var", arguments=[Token("ident", "--" + name)]))
            else: raise ValueError("Expected an entity after @")
            continue
        match = re.match(r"(?:\d+(?:\.\d*)?|\.\d+)(?:[eE][+-]?\d+)?(px|pt)?", text[i:])
        if match:
            unit = match[1] or ""
            result.append(Token("dimension" if unit else "number", float(match[0][:-len(unit)] if unit else match[0]), unit))
            i += len(match[0]); continue
        match = re.match(r"[A-Za-z_][\w-]*", text[i:])
        if match:
            result.append(Token("ident", match[0])); i += len(match[0]); continue
        if c in ",+-*/": result.append(Token("literal", c)); i += 1; continue
        raise ValueError(f"Unexpected value character {c!r}")
    return result


def serialize(tokens):
    raise ValueError("Expected a scalar value or an explicit @(expression)")


def split_tuple(text):
    text = text.strip()
    if not text.startswith("("): return [text]
    if not text.endswith(")"): raise ValueError("Unclosed tuple")
    out, start, depth = [], 1, 0
    for i in range(1, len(text)-1):
        if text[i] == "(": depth += 1
        elif text[i] == ")": depth -= 1
        elif text[i] == "," and depth == 0:
            out.append(text[start:i].strip()); start = i + 1
    out.append(text[start:-1].strip())
    if any(not item for item in out): raise ValueError("Empty tuple component")
    return out


@dataclass
class Style:
    name: str
    template: bool = False
    bases: list = field(default_factory=list)
    props: dict = field(default_factory=dict)
    states: dict = field(default_factory=dict)
    fixed_id: int | None = None
    location: str = ""


def merge(left, right):
    out = dict(left)
    for key, value in right.items():
        if isinstance(value, dict):
            out[key] = merge(out.get(key, {}) if isinstance(out.get(key), dict) else {}, value)
        else:
            out[key] = value
    return out


def normalize_property(name, value):
    if name in ("size", "min-size", "max-size"):
        if not isinstance(value, str): raise ValueError(f'{name} expects a value, not a block')
        pair = split_tuple(value)
        if len(pair) not in (1, 2): raise ValueError(f"{name} expects one or two components")
        prefix = name.removesuffix("size")
        return {prefix + "width": pair[0], prefix + "height": pair[-1]}
    if name == 'overflow' and isinstance(value, str) and value != 'disabled':
        pair = split_tuple(value)
        if len(pair) not in (1, 2): raise ValueError('overflow expects one or two components')
        return {name: {'horizontal': pair[0], 'vertical': pair[-1]}}
    if name == 'aspect-ratio' and isinstance(value, str) and value != 'disabled':
        return {name: {'mode': value}}
    return {name: value}


def parse(path):
    lines = []
    for number, raw in enumerate(Path(path).read_text(encoding="utf-8").splitlines(), 1):
        if "\t" in raw: raise ValueError(f"{path}:{number}: use spaces for indentation")
        text = raw.strip()
        if not text or text.startswith("//"): continue
        lines.append((len(raw)-len(raw.lstrip()), text, number))
    styles, variables = [], {}
    index = 0

    def block(indent, style_fields=True):
        nonlocal index
        props = {}
        while index < len(lines) and lines[index][0] >= indent:
            current, text, number = lines[index]
            if current != indent: raise ValueError(f"{path}:{number}: unexpected indentation")
            if ":" not in text: raise ValueError(f"{path}:{number}: expected property: value")
            name, value = (s.strip() for s in text.split(":", 1))
            index += 1
            if not value:
                if index >= len(lines) or lines[index][0] <= indent:
                    raise ValueError(f"{path}:{number}: empty block")
                value = block(lines[index][0], name in ('@hover', '@active', '@focus'))
            props = merge(props, normalize_property(name, value) if style_fields else {name: value})
        return props

    while index < len(lines):
        indent, text, number = lines[index]
        if indent: raise ValueError(f"{path}:{number}: top-level entity must not be indented")
        index += 1
        variable = re.fullmatch(r"@([\w-]+):\s*(.+)", text)
        if variable:
            variables[variable[1]] = variable[2]; continue
        header = re.fullmatch(r"(@?)([A-Za-z_][\w-]*)(?:\s+at\s+#(0x[0-9a-fA-F]+|\d+))?(?:\s+extends\s+(.+?))?(:)?", text)
        if not header: raise ValueError(f"{path}:{number}: invalid style header {text!r}")
        bases = [s.strip().removeprefix("@") for s in (header[4] or "").split(",") if s.strip()]
        if header[4] and any(not s.strip().startswith("@") for s in header[4].split(",")):
            raise ValueError(f"{path}:{number}: extends references require @")
        item = Style(header[2], bool(header[1]), bases, fixed_id=int(header[3], 16 if header[3].startswith('0x') else 10) if header[3] else None,
                     location=f"{path}:{number}")
        if item.fixed_id is not None and not 0 <= item.fixed_id <= 0xffffffff:
            raise ValueError(f'{item.location}: ID must fit in u32')
        if (item.name == 'global' and item.fixed_id != 0) or (item.name != 'global' and item.fixed_id == 0):
            raise ValueError(f'{item.location}: ID #0 is reserved for global at #0')
        if item.template and item.fixed_id is not None: raise ValueError(f"{item.location}: template cannot have an ID")
        if index < len(lines) and lines[index][0] > 0:
            if not header[5]: raise ValueError(f"{item.location}: properties require ':'")
            props = block(lines[index][0])
            for key in list(props):
                if key.startswith("@"):
                    if key not in ("@hover", "@active", "@focus") or not isinstance(props[key], dict):
                        raise ValueError(f"{item.location}: invalid state {key}")
                    item.states[key[1:]] = props.pop(key)
            item.props = props
        styles.append(item)
    return styles, variables


def resolve(styles):
    entities = {}
    for item in styles:
        if item.name in entities:
            old = entities[item.name]
            if item.template != old.template: raise ValueError(f"{item.location}: entity kind changed")
            if item.fixed_id is not None and old.fixed_id is not None and item.fixed_id != old.fixed_id:
                raise ValueError(f"{item.location}: conflicting fixed ID")
            old.props = merge(old.props, item.props)
            old.states = merge(old.states, item.states)
            if item.bases: old.bases = item.bases
            if item.fixed_id is not None: old.fixed_id = item.fixed_id
        else: entities[item.name] = item
    done, visiting = {}, []
    def visit(name):
        if name in done: return done[name]
        if name in visiting: raise ValueError("Inheritance cycle: " + " -> ".join(visiting + [name]))
        if name not in entities: raise ValueError(f"Unknown base @{name}")
        visiting.append(name)
        item = entities[name]
        props, states = {}, {}
        for base in item.bases:
            bp, bs = visit(base)
            props, states = merge(props, bp), merge(states, bs)
        props, states = merge(props, item.props), merge(states, item.states)
        visiting.pop()
        done[name] = props, states
        return props, states
    for name in entities: visit(name)
    return entities, done


def declarations(props):
    """Lower named ASD fields to the style generator's semantic property operations."""
    aliases = {"bg-color": "background-color", "radius": "border-radius"}
    extras = {
        "align": {"horizontal": "text-align", "vertical": "vertical-align", "display": "display"},
        "text": {"wrap": "white-space", "overflow": "text-overflow"},
        "overflow": {"horizontal": "overflow-x", "vertical": "overflow-y"},
        "aspect-ratio": {"mode": "aspect-ratio"},
    }
    out = []
    for name, value in props.items():
        if name in extras:
            if value == "disabled":
                out.append(("disable-extra", name)); continue
            if isinstance(value, dict):
                for key, val in value.items():
                    if key not in extras[name] or isinstance(val, dict): raise ValueError(f"Unknown {name} field {key}")
                    if name == "align" and key == "vertical" and val == "center": val = "middle"
                    out.append((extras[name][key], val))
                continue
            if name in ("align", "text"): raise ValueError(f"{name} expects a block or disabled")
        elif isinstance(value, dict): raise ValueError(f"Unknown property block {name}")
        elif name not in {'bg-color', 'color', 'font-family', 'font-size', 'inline-spacing', 'radius', 'border',
                          'border-color', 'border-thickness', 'width', 'height', 'min-width', 'min-height',
                          'max-width', 'max-height', 'padding', 'margin'} and not any(
                              name == prefix + '-' + side for prefix in ('padding', 'margin', 'border')
                              for side in ('left', 'top', 'right', 'bottom')):
            raise ValueError(f'Unknown ASD property {name}')
        target = aliases.get(name, name)
        if target in ("padding", "margin", "border-radius", "overflow"):
            value = " ".join(split_tuple(value))
        if target.endswith("width") or target.endswith("height"):
            value = {"fill": "stretch", "fit": "fit-content", "min-fit": "min-content"}.get(value, value)
        out.append((target, value))
    return out
