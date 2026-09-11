import sys
import tempfile
import unittest
from unittest.mock import patch
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
import asd
import theme_compile as compiler


class AsdTests(unittest.TestCase):
    def model(self, source):
        with tempfile.TemporaryDirectory() as folder:
            path = Path(folder) / 'theme.asd'
            path.write_text(source, encoding='utf-8')
            tree = compiler.collect_tree([path])
            model = compiler.build_generated_model(tree, {}, {}, [])
            return tree, model

    def test_draft(self):
        tree, model = self.model('''@color: (42, 42, 43)
global at #0:
    color: @color
    font-family: @font(0)
    size: (fill, fit)
window:
    min-size: (160px, 120px)
    align:
        horizontal: center
    @hover:
        bg-color: @color
@common:
    min-size: @(220px * @dpi)
    radius: disabled
    align: disabled
window-docked extends @window, @common
empty
''')
        self.assertEqual(tree.fixed_ids, {'global': '0x00000000'})
        self.assertNotIn('common', tree.tags)
        rule = next(r for r in model['rules'] if r['tag_macro'] == 'AUIK_STYLE_TAG_WINDOW_DOCKED' and not r['state'])
        self.assertIn('disable(detail::StylePropertiesBits::extra)', rule['style'])
        self.assertEqual(rule['extra_storage_size'], '')
        self.assertTrue(any(r['tag_macro'] == 'AUIK_STYLE_TAG_EMPTY' for r in model['rules']))
        self.assertTrue(any(r['tag_macro'] == 'AUIK_STYLE_TAG_WINDOW_DOCKED' and r['state'] for r in model['rules']))

    def test_axis_order_and_base_order(self):
        _, model = self.model('''@a:
    size: (10px, 20px)
@b:
    width: 30px
x extends @a, @b:
    height: 40px
''')
        self.assertIn('width(30.0f)', model['rules'][0]['style'])
        self.assertIn('height(40.0f)', model['rules'][0]['style'])

    def test_variable_order(self):
        _, model = self.model('@a: @b\n@b: 12px\nx:\n    width: @a\n')
        self.assertEqual([v['name'] for v in model['variables']], ['style_var_b', 'style_var_a'])

    def test_expression(self):
        _, model = self.model('x:\n    width: @(2px * (@dpi + 1))\n')
        self.assertIn('dpi + 1.0f', model['rules'][0]['style'])

    def test_extra_field_override(self):
        _, model = self.model('a:\n    overflow: auto\nb extends @a:\n    overflow:\n        vertical: hidden\n')
        rule = next(r for r in model['rules'] if r['tag_macro'] == 'AUIK_STYLE_TAG_B')
        self.assertIn('OverflowMode::auto_, OverflowMode::hidden', rule['style'])

    def test_disabled_properties(self):
        _, model = self.model("""button:
    @hover:
        bg-color: (255, 255, 255, 0.15)
    @active:
        bg-color: disabled
        padding: disabled
        min-size: disabled
        radius: disabled
        border: 1px (255, 255, 255)
        border-left: disabled
        align:
            horizontal: disabled
""")
        active = next(r for r in model['rules'] if r['state'] == 'StyleState::active')
        for call in ('disable(detail::StylePropertiesBits::background_color)',
                     'disable(detail::StylePropertiesBits::padding)',
                     'disable(detail::StylePropertiesBits::min_width)',
                     'disable(detail::StylePropertiesBits::min_height)',
                     'disable(detail::StylePropertiesBits::border_radius | detail::StylePropertiesBits::corner_mask)',
                     'border_mask(0xEu)', 'border_thickness(1.0f)'):
            self.assertIn(call, active['style'])
        properties = ('color', 'font-size', 'font-family', 'inline-spacing', 'width', 'height',
                      'max-size', 'border-color', 'border-thickness', 'margin', 'padding-left')
        for prop in properties:
            with self.subTest(property=prop): self.model('x:\n    ' + prop + ': disabled\n')

    def test_persistent_ids_with_multiple_folders(self):
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder)
            base, app, extension = (root / n for n in ('base', 'app', 'extension'))
            for directory in (base, app, extension): directory.mkdir()
            (base / 'theme.asd').write_text('global at #0\n')
            compiler.write_ids(base / compiler.DEFAULT_IDS_CSV_NAME, {'global': '0x00000000'})
            (app / 'theme.asd').write_text('attributes-axis-group\nnew-style\n')
            registry = app / 'style_tags_id.csv'
            compiler.write_ids(registry, {'attributes-axis-group': '0x7DECA31F'})
            outputs = []
            for n in range(2):
                output = root / ('build' + str(n))
                argv = ['theme_compile', '--input-base', str(base / 'theme.asd'),
                        '--input-folder', str(app), '--input-folder', str(extension),
                        '--app-ids-csv', str(registry), '--output-folder', str(output)]
                with patch.object(sys, 'argv', argv), patch.object(compiler, 'generate_unique_id', return_value='0x12345678') as generate:
                    compiler.main()
                    self.assertEqual(generate.call_count, 1 if n == 0 else 0)
                outputs.append((output / 'theme_style_sheet.hpp').read_text())
                self.assertFalse((output / 'style_tags_id.csv').exists())
            self.assertEqual(outputs[0], outputs[1])
            self.assertIn('0x7DECA31F', outputs[0])
            self.assertEqual(compiler.read_ids(registry)['new-style'], '0x12345678')

    def test_errors(self):
        for text in ('a extends @missing', '@a:\n    width: 3px\nx extends @a:\n  bogus: 1',
                     'a extends @b\nb extends @a', '@a: @b\n@b: @a\nx',
                     'x:\n    width: @unknown', 'x:\n    align:\n        diagonal: center',
                     'x:\n\twidth: 2px', 'x\n    width: 2px', 'x at #0',
                     'x:\n    width: @(1 2)', 'x:\n    width: @(1 +)'):
            with self.subTest(text=text), self.assertRaises(ValueError): self.model(text)

    def test_aspect_ratio_disabled(self):
        for value in ('disabled', '\n        mode: disabled'):
            _, model = self.model('base:\n    aspect-ratio: preserve\nchild extends @base:\n    aspect-ratio: ' + value + '\n')
            rule = next(r for r in model['rules'] if r['tag_macro'] == 'AUIK_STYLE_TAG_CHILD')
            self.assertIn('disable(detail::StylePropertiesBits::extra)', rule['style'])
            self.assertEqual(rule['extra_storage_size'], '')

            state_value = value.replace('\n        ', '\n            ')
            _, model = self.model('base:\n    @active:\n        aspect-ratio: preserve\nchild extends @base:\n    @active:\n        aspect-ratio: ' + state_value + '\n')
            rule = next(r for r in model['rules'] if r['tag_macro'] == 'AUIK_STYLE_TAG_CHILD' and
                        r['state'] == 'StyleState::active')
            self.assertIn('disable(detail::StylePropertiesBits::extra)', rule['style'])
            self.assertEqual(rule['extra_storage_size'], '')
        for value in ('initial', '\n        mode: initial'):
            with self.assertRaises(ValueError): self.model('x:\n    aspect-ratio: ' + value + '\n')

    def test_state_extra_field_keeps_normal_fields(self):
        _, model = self.model('''x:
    align:
        horizontal: center
        vertical: bottom
    overflow: auto
    @hover:
        align:
            vertical: disabled
''')
        rule = next(r for r in model['rules'] if r['tag_macro'] == 'AUIK_STYLE_TAG_X' and
                    r['state'] == 'StyleState::hover')
        self.assertIn('ChildLayoutFlagBits::hcenter', rule['style'])
        self.assertNotIn('ChildLayoutFlagBits::bottom', rule['style'])
        self.assertIn('StyleExtraAlign', rule['extra_storage_size'])
        self.assertIn('StyleExtraOverflow', rule['extra_storage_size'])
        self.assertIn('OverflowMode::auto_', rule['style'])


if __name__ == '__main__': unittest.main()
