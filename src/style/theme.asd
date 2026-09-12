@color-surface: (42, 42, 43)
@color-surface-light: (65, 65, 65)
@color-surface-dark: (31, 31, 31)
@color-hover: (255, 255, 255, 0.15)
@color-border: (51, 51, 51)
@color-accent: (72, 114, 255)
@color-accent-hover: (91, 130, 255)
@color-text: (230, 230, 230)
@color-picker-size: 185px

global at #0:
    color: @color-text
    font-family: @font(0)
    font-size: 9pt
    margin: (5px, 0px)
    inline-spacing: 4px
    size: (fill, fit)

caret:
    padding: (0px, 1px)
    bg-color: @color-text

multiline-caret extends @caret

no-pad:
    margin: 0px
    padding: 0px

placeholder:
    margin: 0px
    padding: 0px
    color: (230, 230, 230, 0.5)

selection:
    bg-color: (72, 114, 255, 0.5)

rubber-band:
    bg-color: (72, 114, 255, 0.5)
    border: 1px (72, 114, 255, 0.8)

text-drag-icon:
    bg-color: (204, 204, 204)

column:
    inline-spacing: 8px

table:
    margin: (5px, 0px)
    padding: 0px
    bg-color: @color-surface-dark
    border: 1px @color-border
    radius: 4px

table-header-cell:
    margin: 0px
    padding: (0px, 8px)
    bg-color: @color-surface-light
    @hover:
        bg-color: @color-hover
    @active:
        bg-color: @color-hover

table-cell:
    margin: 0px
    padding: (0px, 8px)
    border-bottom: 1px @color-border
    align:
        vertical: center

shortcut:
    margin: 0px
    inline-spacing: 5px

shortcut-key:
    margin: 0px
    padding: (2px, 6px)
    bg-color: @color-surface-light
    radius: 3px

shortcut-key-text:
    margin: 0px
    padding: 0px
    font-size: 8pt

shortcut-input:
    margin: (8px, 0px)
    padding: (8px, 10px)
    min-height: 36px
    bg-color: @color-surface-dark
    border: 1px @color-border
    radius: 4px

shortcut-input-caret:
    margin: 0px
    padding: 0px
    color: @color-text

table-row-alt:
    margin: 0px
    bg-color: (255, 255, 255, 0.035)

table-resize-border:
    margin: (2px, 2px)
    padding: 6px
    @hover:
        bg-color: @color-accent
    @active:
        bg-color: @color-accent

table-tree-resize-border:
    margin: (2px, 2px)
    padding: 0.5px
    bg-color: @color-surface-light
    @hover:
        bg-color: @color-accent
        border: 1px @color-accent
    @active:
        bg-color: @color-accent
        border: 1px @color-accent

dockspace-resize-helper:
    margin: 8px
    padding: 2px
    bg-color: @color-surface-light
    @hover:
        bg-color: @color-surface-light
    @focus:
        bg-color: @color-accent
    @active:
        bg-color: @color-accent

dockspace-resize-helper-drag:
    margin: 8px
    padding: 2px
    bg-color: @color-surface-light
    @hover:
        bg-color: @color-accent
    @focus:
        bg-color: @color-accent
    @active:
        bg-color: @color-accent

dockspace-drop-zone:
    bg-color: (72, 114, 255, 0.5)

dockspace-node:
    width: fill
    height: fill
    min-width: 80px
    min-height: 80px

dockspace-node-fit-width:
    width: fit
    height: fill
    min-width: 80px
    min-height: 80px

dockspace-node-fit:
    width: fit
    height: fit
    min-width: 80px
    min-height: 80px

dock-node-tab-panel:
    margin: 0px
    bg-color: @color-surface-dark

dock-tabbar:
    width: fill
    height: fit
    margin: (4px, 0px)
    inline-spacing: 0px

dock-tabbar-menu:
    margin: (-2px, 4px, 0px, 4px)
    padding: 6px
    radius: 3px
    @hover:
        bg-color: @color-hover
    @active:
        bg-color: disabled

window-header-menu:
    margin: 0px
    padding: 6px
    radius: 4px
    @hover:
        bg-color: @color-hover
    @active:
        bg-color: disabled

dock-tab-item:
    margin: (0px, 2px)
    padding: (4px, 12px)
    @hover:
        bg-color: @color-hover
        radius: 3px

dock-tab-item-selected:
    margin: (0px, 2px)
    padding: (4px, 12px)
    bg-color: @color-surface-light
    radius: 3px
    @hover:
        bg-color: (255, 255, 255, 0.22)

tree:
    width: fit
    height: fit
    margin: 0px

tree-cell:
    margin: (0px, 5px)
    padding: (4px, 4px)

tree-row-alt:
    margin: 0px
    bg-color: (34, 34, 35)

tree-line:
    margin: 0px
    bg-color: (70, 70, 70)
    border: 2px solid

tree-collapse-icon:
    margin: (4px, 0px)
    padding: (0px, 3px, 0px, 7px)
    color: @color-text

collapse-header:
    padding: (4px, 0px)
    bg-color: @color-surface-light
    radius: 3px
    @hover:
        bg-color: @color-hover
    @active:
        bg-color: @color-surface-light

collapse-header-closed:
    padding: (4px, 0px)
    bg-color: @color-surface-light
    radius: 3px
    @hover:
        bg-color: @color-hover
    @active:
        bg-color: @color-surface-light

collapse-header-content:
    margin: (0px, 0px)
    padding: (4px, 0px, 4px, 8px)

collapse-header-trigger:
    margin: (0px, 8px)

window:
    min-width: 160px
    min-height: 120px
    padding: (10px, 8px)
    bg-color: @color-surface
    border: 1px @color-border
    radius: 6px
    overflow: auto

@dock-window-default:
    min-width: @(220px * @dpi)
    border: disabled
    radius: disabled

docked-window extends @window, @dock-window-default

modal-backdrop:
    bg-color: (0, 0, 0, 0.5)

modal-window:
    padding: (8px, 10px)
    bg-color: @color-surface
    border: 1px @color-border
    radius: 6px

window-header:
    padding: (8px, 8px)
    bg-color: @color-surface-dark
    radius: (6px, 6px, 0px, 0px)

titlebar:
    bg-color: @color-surface-dark

titlebar-leading-region:
    margin: 0px
    bg-color: @color-surface-light

titlebar-icon:
    margin: (0px, 8px)

text-button:
    width: fit
    height: fit
    padding: (4px, 14px)
    align:
        horizontal: center
    bg-color: @color-surface-light
    radius: 3px
    @hover:
        bg-color: @color-hover
    @active:
        bg-color: @color-surface-light

accent-text-button:
    min-width: 60px
    width: fit
    height: fit
    padding: (4px, 10px)
    align:
        horizontal: center
    bg-color: @color-accent
    radius: 3px
    @hover:
        bg-color: @color-accent-hover
    @active:
        bg-color: @color-accent

transparent-button:
    min-width: 60px
    width: fit
    height: fit
    align:
        horizontal: center
    padding: (4px, 10px)
    radius: 3px
    @hover:
        bg-color: @color-hover
    @active:
        bg-color: disabled

modal-title:
    padding: 0px
    font-family: @font(1)

modal-header:
    margin-bottom: 8px

modal-message-area:
    margin: (8px, 8px, 4px, 8px)

modal-icon:
    width: fit
    height: fit
    margin-right: 22px

modal-batch-label:
    font-size: @(11px * @dpi)

modal-controls-area:
    margin: (15px, 0px, 6px, 0px)

image-button:
    padding: 6px
    bg-color: @color-surface-light
    radius: 4px
    @hover:
        bg-color: @color-hover
    @active:
        bg-color: @color-surface-light

textbox:
    min-width: 160px
    padding: (4px, 8px)
    bg-color: @color-surface-light
    radius: 4px

multiline-textbox:
    min-width: 160px
    padding: (4px, 8px)
    align:
        vertical: top
    text:
        wrap: normal
        overflow: clip
    bg-color: @color-surface-light
    radius: 4px

checkbox:
    padding: 3px
    bg-color: @color-surface-light
    radius: 3px
    @hover:
        bg-color: @color-hover
    @active:
        bg-color: @color-surface-light

radio-button:
    padding: 10px
    bg-color: @color-surface-light
    radius: 10px
    @hover:
        bg-color: @color-hover
    @active:
        bg-color: @color-surface-light

radio-button-indicator:
    margin: 0px
    padding: 5px
    bg-color: @color-text
    radius: 5px

switch-button:
    padding: 2px
    bg-color: @color-surface-light
    radius: 10px
    @hover:
        bg-color: @color-hover
    @active:
        bg-color: @color-surface-light

switch-button-on:
    padding: 2px
    bg-color: @color-accent
    radius: 10px
    @hover:
        bg-color: @color-accent-hover

switch-button-grab:
    padding: 8px
    bg-color: @color-text
    radius: 8px

combo-box:
    padding: (4px, 8px)
    bg-color: @color-surface-light
    radius: 3px
    @hover:
        bg-color: @color-hover
    @focus:
        radius: (4px, 4px, 0px, 0px)

combo-box-popup:
    margin: 0px
    padding: (2px, 0px)
    max-height: @(280px * @dpi)
    bg-color: @color-surface-dark
    radius: (0px, 0px, 4px, 4px)
    overflow: auto

combo-box-item:
    margin: (2px, 4px)
    padding: (3px, 8px)
    radius: 3px
    @hover:
        bg-color: @color-hover
    @active:
        bg-color: @color-surface-light
    @focus:
        bg-color: @color-surface-light

combo-box-item-multi:
    margin: (2px, 4px)
    padding: (3px, 26px, 3px, 6px)
    radius: 3px
    align:
        vertical: center
    @hover:
        bg-color: @color-hover
    @active:
        bg-color: @color-surface-light
    @focus:
        bg-color: @color-surface-light

tree-reorder-indicator:
    width: fill
    height: 2px
    margin: 0px
    bg-color: (90, 90, 90)

combo-box-item-selected:
    margin: (2px, 4px)
    padding: (4px, 8px)
    bg-color: @color-accent
    radius: 3px
    @hover:
        bg-color: @color-accent-hover
    @focus:
        bg-color: @color-accent-hover

tabbar:
    width: fit
    height: fit

tabbar-item:
    margin: 0px
    padding: (4px, 8px)
    align:
        vertical: center
    bg-color: @color-surface-light
    radius: 3px
    @hover:
        bg-color: @color-hover

tabbar-item-selected:
    margin: 0px
    padding: (4px, 8px)
    align:
        vertical: center
    bg-color: @color-accent
    radius: 3px
    @hover:
        bg-color: @color-accent-hover
    @focus:
        bg-color: @color-accent-hover

tabbar-change-icon:
    width: @(8px * @dpi)
    height: @(8px * @dpi)
    margin: (0px, @(8px * @dpi))
    bg-color: (190, 190, 190)
    radius: @(4px * @dpi)

tabbar-popup-button:
    margin: 0px
    padding: (5px, 8px)
    radius: 4px
    @hover:
        bg-color: @color-hover
    @focus:
        bg-color: @color-surface-light

tabbar-popup:
    margin: 0px
    padding: (2px, 0px)
    min-height: @(96px * @dpi)
    max-height: @(280px * @dpi)
    bg-color: @color-surface-dark
    radius: 4px
    overflow: auto

tabbar-popup-item:
    margin: (2px, 4px)
    padding: (4px, 8px)
    radius: 3px
    @hover:
        bg-color: @color-hover
    @active:
        bg-color: @color-surface-light
    @focus:
        bg-color: @color-surface-light

close-button:
    width: @(20px * @dpi)
    height: @(20px * @dpi)
    margin: (0px, 2px)
    padding: @(2px * @dpi)
    align:
        horizontal: center
        vertical: center
    radius: 12px
    @hover:
        bg-color: (255, 255, 255, 0.15)
    @active:
        bg-color: @color-surface-light

window-caption-button:
    margin: 0px
    align:
        horizontal: center
        vertical: center
    @hover:
        bg-color: (255, 255, 255, 0.15)
    @active:
        bg-color: @color-surface-light

window-caption-close-button:
    margin: 0px
    align:
        horizontal: center
        vertical: center
    radius: 0px
    @hover:
        bg-color: (193, 42, 28)
    @active:
        bg-color: (193, 42, 28)

window-menu-bar:
    width: fit
    height: fit
    margin: 0px
    bg-color: (0, 0, 0, 0.15)

main-menu-bar:
    margin: 0px
    bg-color: @color-surface-dark

titlebar-menu-bar:
    width: fit
    height: fit
    margin: (0px, 8px)
    inline-spacing: 0px

menu-bar-item:
    margin: (2px, 2px)
    padding: (2px, 8px)
    radius: 3px
    @hover:
        bg-color: @color-hover
    @active:
        bg-color: @color-surface-light
    @focus:
        bg-color: @color-hover

main-menu-item:
    margin: (2px, 2px)
    padding: (2px, 8px)
    radius: 3px
    @hover:
        bg-color: @color-hover
    @active:
        bg-color: @color-surface-light
    @focus:
        bg-color: @color-hover

titlebar-menu-item:
    margin: 0px
    padding: (4px, 8px)
    align:
        vertical: center
    radius: 3px
    @hover:
        bg-color: @color-hover
    @active:
        bg-color: @color-surface-light
    @focus:
        bg-color: (255, 255, 255, 0.08)

menu-shortcut:
    margin: (0px, 0px, 0px, 50px)
    color: (184, 184, 184)

menu-popup:
    padding: (2px, 0px)
    bg-color: (25, 25, 25)
    radius: (0px, 0px, 4px, 4px)
    overflow: auto

separator:
    margin: (2px, 4px)
    padding: 0.5px
    bg-color: (77, 77, 77)

slider:
    padding: (4px, 0px)
    bg-color: @color-surface-light
    radius: 2.5px
    @active:
        bg-color: @color-accent

progress-bar:
    padding: (4px, 0px)
    bg-color: @color-surface-light
    radius: 2.5px

progress-bar-active:
    padding: (4px, 0px)
    bg-color: @color-accent
    radius: 2.5px

slider-grab:
    margin: (4px, 0px)
    padding: 6px
    bg-color: @color-text
    radius: 6px
    @hover:
        margin: (0px, -4px)
        padding: 10px
        border: 4px (255, 255, 255, 0.25)
        radius: 10px
    @active:
        margin: (0px, -4px)
        padding: 10px
        border: 4px (255, 255, 255, 0.35)
        radius: 10px

gradient-slider-grab:
    padding: 5px
    bg-color: @color-text
    border: 1.5px (0, 0, 0, 0.5)
    radius: 5px

gradient-slider-grab-border:
    padding: 8px
    bg-color: @color-text
    border: 1.5px (0, 0, 0, 0.5)
    radius: 8px

gradient-slider:
    padding: (4px, 0px)
    bg-color: @color-surface-light
    radius: 2.5px

tooltip:
    margin: 0px
    padding: (6px, 10px)
    bg-color: @color-surface-dark
    border: 0px @color-border
    radius: 3px
    color: (235, 235, 235)

scrollbar-track:
    margin: 0px
    padding: 2px
    bg-color: @color-surface-dark

scrollbar-thumb:
    margin: 0px
    padding: (0px, 4px)
    bg-color: (89, 89, 89)
    radius: 2px
    @hover:
        bg-color: (128, 128, 128)
    @active:
        bg-color: (128, 128, 128)

scrollbar-track-internal:
    margin: 4px

scrollbar-thumb-internal:
    margin: 0px
    padding: (0px, 4px)
    bg-color: (89, 89, 89)
    radius: 2px
    @hover:
        bg-color: (128, 128, 128)
    @active:
        bg-color: (128, 128, 128)
