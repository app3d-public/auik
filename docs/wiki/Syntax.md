# Syntax

An ASD document consists of top-level declarations. A colon introduces an indented block. Each property occupies one line.

```asd
button:
    size: fit
    padding: (4px, 14px)
    align:
        horizontal: center
```

## Indentation

Indentation uses spaces. Entries in the same block must have equal indentation. Nested blocks must be indented further than their parent. Tabs are invalid. Four spaces per level are recommended.

Blank lines are ignored. A line beginning with `//`, optionally preceded by whitespace, is a comment. Inline comments are not supported.

```asd
// Shared colors
@surface: (42, 42, 43)
```

## Names

Names and keywords are case-sensitive. Style names begin with a letter or underscore and may contain letters, digits, underscores and hyphens.

## Properties

```text
property: value
property:
    field: value
```

Unknown properties and fields are invalid. Later declarations override earlier declarations of the same property.

`size` expands into independent axes at its declaration position. The same rule applies to `min-size` and `max-size`.

```asd
panel:
    size: (200px, 100px)
    width: 300px
```

The resulting width is `300px`; the height remains `100px`.
