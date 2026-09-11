# Extras

An extra is declared by name. Its fields appear in an indented block.

```asd
label:
    align:
        horizontal: center
        vertical: center
    text:
        wrap: nowrap
        overflow: ellipsis
```

## Fields

| Extra | Field | Accepted keywords |
| --- | --- | --- |
| `align` | `display` | block, inline |
| `align` | `horizontal` | left, center, right |
| `align` | `vertical` | top, center, bottom |
| `text` | `wrap` | normal, nowrap |
| `text` | `overflow` | clip, ellipsis |
| `overflow` | `horizontal`, `vertical` | visible, hidden, auto, scroll |
| `aspect-ratio` | `mode` | disabled, preserve |

`overflow: auto` sets both axes to `auto`. `aspect-ratio: preserve` is shorthand for `mode: preserve`.

`aspect-ratio: disabled` removes the extra. The following block is equivalent:

```asd
panel:
    aspect-ratio:
        mode: disabled
```

`initial` is not a valid value.

## Disabled

```asd
plain-label extends @label:
    align: disabled
```

`disabled` removes the entire extra, including an inherited value. It does not assign zero values to its fields. The keyword replaces the block. Individual fields also accept `disabled`: alignment flags are cleared, text wrapping and ellipsis are disabled, and overflow axes become visible. Other fields of the extra remain unchanged.

`border: disabled` and `radius: disabled` disable the corresponding visual property. See [Values](Values.md#disabled) for scalar properties.
