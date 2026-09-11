# States

State-dependent properties are declared in nested `@hover`, `@active` and `@focus` blocks.

```asd
text-button:
    bg-color: (65, 65, 65)
    @hover:
        bg-color: (255, 255, 255, 0.15)
    @active:
        bg-color: (72, 114, 255)
    @focus:
        radius: 4px
```

Properties outside state blocks describe the normal state. A state uses normal values for properties it does not override.

State blocks are inherited through `extends`. Matching states from successive bases are merged; the derived style's matching state is applied last.

```asd
quiet-button extends @text-button:
    @hover:
        bg-color: (50, 50, 50)
```

`quiet-button` overrides the inherited hover background and retains the inherited active and focus declarations. States cannot be nested inside other states.
