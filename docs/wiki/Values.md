# Values and expressions

## Scalars and tuples

Values include numbers, dimensions, keywords and tuples. Tuple components are separated by commas and enclosed in parentheses.

```asd
panel:
    width: fill
    height: fit
    min-size: (160px, 120px)
    padding: (10px, 8px)
    bg-color: (42, 42, 43)
```

| Property | Component order |
| --- | --- |
| `size`, `min-size`, `max-size` | width, height |
| `margin`, `padding`, two components | vertical, horizontal |
| `margin`, `padding`, four components | top, right, bottom, left |
| Color | red, green, blue, optional alpha |

One size or spacing value applies to both axes or all sides. `width` and `height` may be declared separately, including their `min-` and `max-` forms.

Dimensions support `px` and `pt`. RGB channels use the 0–255 range. Alpha values from 0 through 1 are normalized; values greater than 1 use the 0–255 range. Omitted alpha is fully opaque.

## Variables

```asd
@color-text: (230, 230, 230)
@panel-width: 220px

panel:
    color: @color-text
    width: @panel-width
```

`@name: value` declares a variable. `@name` references its value. Forward references are allowed. Unknown references and dependency cycles are invalid. Variables and styles must have distinct names.

## Expressions

`@(expression)` evaluates arithmetic. Operators are `+`, `-`, `*` and `/`, including unary signs and nested parentheses. Multiplication and division precede addition and subtraction. Binary operators of equal precedence associate left to right.

```asd
panel:
    min-width: @(220px * @dpi)
    height: @((20px + 4px) * @dpi)
```

`@dpi` is externally supplied and cannot be redeclared. Pixel values are not implicitly multiplied by it.

## Function references

```asd
label:
    font-family: @font(0)
```

`@font(index)` references a font by a non-negative integer index. Unknown function names are invalid.

The `@` prefix identifies an external entity or operation: a variable, an expression, a function reference, a template declaration or a base-style reference.

## Disabled

Every property accepts `disabled`. It is an explicit override, not an omitted declaration: inheritance and state fallback cannot restore the disabled property.

```asd
transparent-button:
    @hover:
        bg-color: (255, 255, 255, 0.15)
    @active:
        bg-color: disabled
```

Here the active background is transparent even when hover is also applicable.

For scalar properties, `disabled` resets the property to its intrinsic style default: colors become transparent, spacing and size limits become zero, width becomes `fill`, height becomes `fit`, font becomes unset, and font size becomes the intrinsic default (12.5). It does not copy the value from `global`.

For `border` and `radius`, the corresponding visual feature is disabled. A disabled border side clears that side's mask; a disabled padding or margin side becomes zero. Whole extras are removed; see [Extras](Extras.md#disabled).
