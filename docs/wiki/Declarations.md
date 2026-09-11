# Declarations

## Style declaration

```text
name [at #id] [extends @base, ...][:]
```

Square brackets denote optional syntax. If both clauses are present, `at` precedes `extends`.

```asd
base-style

panel at #42:
    min-size: (160px, 120px)

small-panel extends @panel

fixed-panel at #0x12345678 extends @panel:
    width: 200px
```

The colon is required when the declaration has a property block. It may be omitted when there are no additional properties. A bare declaration uses the global defaults.

## Explicit identifiers

`at #id` assigns a fixed unsigned 32-bit identifier. Decimal and hexadecimal notation are supported. Different entities must not share an identifier.

Identifier zero is reserved:

```asd
global at #0:
    color: (230, 230, 230)
    font-family: @font(0)
    font-size: 9pt
```

Other styles may omit `at`; identifier assignment is then external to the declaration.

## Templates

A leading `@` declares a reusable style template. A template has no style identifier and cannot contain an `at` clause.

```asd
@docked-common:
    border: disabled
    radius: disabled
```

Templates and ordinary styles can be referenced by `extends`.
