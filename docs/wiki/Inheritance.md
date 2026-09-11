# Inheritance

`extends` specifies base styles or templates. Each reference begins with `@`.

```asd
window:
    min-size: (160px, 120px)
    padding: (10px, 8px)
    bg-color: (42, 42, 43)
    border: 1px (51, 51, 51)
    radius: 6px

@docked-common:
    border: disabled
    radius: disabled

window-docked extends @window, @docked-common
```

## Precedence

Bases are applied from left to right. A later base overrides an earlier base. Properties declared in the derived style override all bases.

```asd
@first:
    size: (100px, 50px)

@second:
    width: 200px

panel extends @first, @second:
    height: 80px
```

`panel` has width `200px` and height `80px`.

Nested extra fields and state declarations are merged by name. Unknown bases and inheritance cycles are invalid.

## Global defaults

`global at #0` supplies defaults to ordinary styles. These defaults are implicit: they are not copied into each base when resolving `extends`. Explicit properties take precedence over global defaults.

## Repeated declarations

Declarations of the same style are merged in declaration order. A later explicit `extends` list replaces the previous base list. Repeated declarations must retain the entity kind and must not specify conflicting fixed identifiers.
