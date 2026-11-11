# SPDX-FileCopyrightText: 2026 Abdolmanaf Mehrabi Sisakht
# SPDX-License-Identifier: MIT
"""Tests of Tools/UpdateCatalog.py:  python Tests/UpdateCatalogTests.py"""
import json
import os
import sys
import tempfile
import unittest

sys.path.insert(0, os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), 'Tools'))
import UpdateCatalog  # noqa: E402

SOURCE = r'''
// tr("in a comment") is no text
/* nor tr("in this one") */
inline LocalizedText tr(std::string source, std::string key = {});   // the declaration: no text, no warning
void f() {
    label.setText(glw::tr("Open"));
    menu.add(tr("Save {file}", "menu.save").arg("file", name));
    auto a = tr("two " "parts");
    auto b = tr(u8"سلام \"quoted\"\n");
    auto c = tr(R"(raw "text")");
    auto d = trPlural("{n} file", "{n} files", count);
    auto e = trPlural("{n} node", "{n} nodes", n, 0, "nodes.count");
    auto g = tr(name);                       // not a literal: warned
    auto h = translator.tr("member");        // another's
    char q = '"';                            // a quote in a character
    auto i = tr("after the quote");
    auto j = tr("Open");                     // again: one text
}
'''


class UpdateCatalogTests(unittest.TestCase):
    def setUp(self):
        self.dir = tempfile.TemporaryDirectory()
        self.source = os.path.join(self.dir.name, 'Window.cpp')
        self.catalog = os.path.join(self.dir.name, 'Translations', 'fa.json')
        with open(self.source, 'w', encoding='utf-8') as f:
            f.write(SOURCE)

    def tearDown(self):
        self.dir.cleanup()

    def run_tool(self, *extra):
        return UpdateCatalog.main(['--language', 'fa', '--catalog', self.catalog, self.dir.name, *extra])

    def read(self):
        with open(self.catalog, encoding='utf-8') as f:
            return json.load(f)

    def test_finds_the_texts_of_the_calls(self):
        warnings = []
        found = UpdateCatalog.find_texts(self.source, {'tr', 'trPlural'}, {'trPlural'}, warnings)
        keys = [f.key for f in found]
        self.assertEqual(keys, ['Open', 'menu.save', 'two parts', 'سلام "quoted"\n', 'raw "text"', '{n} file',
                                'nodes.count', 'after the quote', 'Open'])
        self.assertEqual(found[1].source, 'Save {file}')
        self.assertEqual(found[5].plural, '{n} files')
        self.assertEqual(len(warnings), 1)
        self.assertIn('Window.cpp:13', warnings[0])

    def test_makes_a_catalog_and_keeps_its_translations(self):
        self.assertEqual(self.run_tool(), 0)
        catalog = self.read()
        self.assertEqual(catalog['language'], 'fa')
        self.assertEqual(catalog['texts']['Open'], ['Open', ''])
        self.assertEqual(catalog['texts']['menu.save'], ['Save {file}', ''])
        self.assertEqual(catalog['texts']['{n} file'], ['{n} file', {'one': '', 'other': ''}])
        self.assertEqual(len(catalog['texts']), 8)
        # translated, then the sources change: one English changes, one text goes, one comes
        catalog['texts']['Open'] = ['Open', 'باز کردن']
        catalog['texts']['two parts'] = ['two parts', 'دو بخش']
        catalog['texts']['{n} file'] = ['{n} file', {'one': '{n} فایل', 'other': '{n} فایل'}]
        with open(self.catalog, 'w', encoding='utf-8') as f:
            json.dump(catalog, f, ensure_ascii=False)
        with open(self.source, 'w', encoding='utf-8') as f:
            f.write(SOURCE.replace('tr("Open")', 'tr("Open…")', 1).replace('tr("Open")', 'tr("Open…", "Open")')
                    .replace('"two " "parts"', '"new"'))
        self.assertEqual(self.run_tool(), 0)
        catalog = self.read()
        # the old English kept with its translation: outdated, the application shows the English
        self.assertEqual(catalog['texts']['Open'], ['Open', 'باز کردن'])
        self.assertEqual(catalog['texts']['new'], ['new', ''])
        self.assertEqual(catalog['obsolete']['two parts'], ['two parts', 'دو بخش'])
        self.assertEqual(catalog['texts']['{n} file'][1]['one'], '{n} فایل')
        # back in the sources: back from the obsolete ones, translated
        with open(self.source, 'w', encoding='utf-8') as f:
            f.write(SOURCE)
        self.run_tool()
        catalog = self.read()
        self.assertEqual(catalog['texts']['two parts'], ['two parts', 'دو بخش'])
        self.assertNotIn('obsolete', catalog)
        # up to date: --check passes; changed, it fails, and writes nothing
        self.assertEqual(self.run_tool('--check'), 0)
        with open(self.source, 'a', encoding='utf-8') as f:
            f.write('auto z = tr("more");\n')
        self.assertEqual(self.run_tool('--check'), 1)
        self.assertNotIn('more', self.read()['texts'])

    def test_a_catalog_of_another_language_is_refused(self):
        os.makedirs(os.path.dirname(self.catalog))
        with open(self.catalog, 'w', encoding='utf-8') as f:
            json.dump({'language': 'ar', 'texts': {}}, f)
        with self.assertRaises(SystemExit):
            self.run_tool()

    def test_forms_by_language(self):
        self.assertEqual(UpdateCatalog.forms_of('ar-EG'), ['zero', 'one', 'two', 'few', 'many', 'other'])
        self.assertEqual(UpdateCatalog.forms_of('xx'), ['one', 'other'])


if __name__ == '__main__':
    unittest.main()
