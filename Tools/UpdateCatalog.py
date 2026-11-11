# SPDX-FileCopyrightText: 2026 Abdolmanaf Mehrabi Sisakht
# SPDX-License-Identifier: MIT
"""Finds the texts to translate in C++ sources and brings a language's catalog (Localization.hpp) up to date with them,
keeping what is translated: what lupdate is to Qt's .ts files.

  python Tools/UpdateCatalog.py --language fa --catalog Translations/fa.json Source/ Include/
  python Tools/UpdateCatalog.py --language fa --catalog Translations/fa.json --check Source/     (CI: is it up to date?)

The texts are those of tr("...") and trPlural("...", "...", count) with literal strings (adjacent ones joined, escapes
and raw strings read), and their keys when given as literals: tr("Open", "menu.open"), trPlural(one, other, n, 0,
"files.count"). Calls of other names that make texts the same way are found too with --function NAME. A call whose text
is not a literal cannot be found: --warn lists them.

In the catalog every text is written as [its English, its translation]: when the English changes in the sources, the
translation is kept as it was, of the old English, which the application does not use (it shows the English) until a
translator updates it; such texts are reported as outdated. A new text is added untranslated (its translation empty: the
English shows). A translated text no longer in the sources goes to the catalog's "obsolete" texts (the application reads only
"texts"), and comes back from there, translated, if it comes back in the sources; --drop-obsolete forgets them (an
untranslated one is forgotten).

A counted text (trPlural) is written with the forms its language has (CLDR's categories), empty until translated.
"""
import argparse
import json
import os
import re
import sys

EXTENSIONS = ('.cpp', '.cc', '.cxx', '.c++', '.hpp', '.hh', '.hxx', '.h', '.inl', '.ipp', '.mm')

# The plural forms (CLDR's categories) of some languages; the others: one and other.
FORMS = {
    'fa': ['one', 'other'], 'en': ['one', 'other'], 'de': ['one', 'other'], 'es': ['one', 'many', 'other'],
    'fr': ['one', 'many', 'other'], 'it': ['one', 'many', 'other'], 'pt': ['one', 'many', 'other'],
    'ru': ['one', 'few', 'many', 'other'], 'uk': ['one', 'few', 'many', 'other'], 'pl': ['one', 'few', 'many', 'other'],
    'cs': ['one', 'few', 'many', 'other'], 'ar': ['zero', 'one', 'two', 'few', 'many', 'other'],
    'he': ['one', 'two', 'other'], 'tr': ['one', 'other'], 'ja': ['other'], 'zh': ['other'], 'ko': ['other'],
    'id': ['other'], 'vi': ['other'], 'th': ['other'],
}


def forms_of(language):
    return FORMS.get(language, FORMS.get(re.split('[-_]', language)[0], ['one', 'other']))


# ---- C++ tokens: identifiers, string literals (their values), the rest one character each; comments left out ----

class Token:
    __slots__ = ('kind', 'value', 'line')

    def __init__(self, kind, value, line):
        self.kind, self.value, self.line = kind, value, line


ESCAPES = {'n': b'\n', 't': b'\t', 'r': b'\r', '0': b'\0', 'a': b'\a', 'b': b'\b', 'f': b'\f', 'v': b'\v',
           '\\': b'\\', '"': b'"', "'": b"'", '?': b'?'}


def unescape(body):
    """The bytes of a string literal's body, its escapes read, as UTF-8 text."""
    out = bytearray()
    i = 0
    while i < len(body):
        c = body[i]
        if c != '\\':
            out += c.encode('utf-8')
            i += 1
            continue
        e = body[i + 1] if i + 1 < len(body) else ''
        if e == 'x':
            m = re.match(r'[0-9a-fA-F]+', body[i + 2:])
            out.append(int(m.group(0), 16) & 0xFF if m else 0)
            i += 2 + (len(m.group(0)) if m else 0)
        elif e in 'uU':
            n = 4 if e == 'u' else 8
            out += chr(int(body[i + 2:i + 2 + n], 16)).encode('utf-8')
            i += 2 + n
        elif e in '01234567':
            m = re.match(r'[0-7]{1,3}', body[i + 1:])
            out.append(int(m.group(0), 8) & 0xFF)
            i += 1 + len(m.group(0))
        else:
            out += ESCAPES.get(e, e.encode('utf-8'))
            i += 2
    return out.decode('utf-8', errors='replace')


def tokens(text):
    i, line, n = 0, 1, len(text)
    while i < n:
        c = text[i]
        if c == '\n':
            line += 1
            i += 1
        elif c.isspace():
            i += 1
        elif text.startswith('//', i):
            i = text.find('\n', i)
            i = n if i < 0 else i
        elif text.startswith('/*', i):
            end = text.find('*/', i + 2)
            end = n if end < 0 else end + 2
            line += text.count('\n', i, end)
            i = end
        elif c.isalpha() or c == '_':
            m = re.compile(r'[A-Za-z_0-9]+').match(text, i)
            word = m.group(0)
            # a string's prefix (u8"...", L"...", R"(...)"), or an identifier
            if i + len(word) < n and text[i + len(word)] == '"' and re.fullmatch(r'(u8|u|U|L)?R?', word):
                raw = word.endswith('R')
                start = i + len(word)
                if raw:
                    open_paren = text.find('(', start)
                    delimiter = text[start + 1:open_paren]
                    close = text.find(')' + delimiter + '"', open_paren)
                    value = text[open_paren + 1:close]
                    end = close + len(delimiter) + 2
                    yield Token('string', value, line)
                    line += text.count('\n', i, end)
                    i = end
                else:
                    value, end = string_at(text, start)
                    yield Token('string', value, line)
                    i = end
            else:
                yield Token('name', word, line)
                i += len(word)
        elif c == '"':
            value, end = string_at(text, i)
            yield Token('string', value, line)
            i = end
        elif c == "'":
            # a character: skipped (its quote must not start a string)
            j = i + 1
            while j < n and text[j] != "'":
                j += 2 if text[j] == '\\' else 1
            i = j + 1
        elif c.isdigit():
            m = re.compile(r"[0-9A-Za-z_.']+").match(text, i)
            yield Token('number', m.group(0), line)
            i += len(m.group(0))
        else:
            yield Token('punct', c, line)
            i += 1


def string_at(text, start):
    """The value of the ordinary string literal whose quote is at `start`, and where it ends."""
    j = start + 1
    while j < len(text) and text[j] != '"':
        j += 2 if text[j] == '\\' else 1
    return unescape(text[start + 1:j]), j + 1


# ---- the calls ----

class Found:
    def __init__(self, source, key, plural, where):
        self.source, self.key, self.plural, self.where = source, key, plural, [where]


def arguments(toks, at):
    """The arguments of the call whose '(' is toks[at]: each a list of tokens; and the index after its ')'."""
    args, current, depth = [], [], 0
    i = at + 1
    while i < len(toks):
        t = toks[i]
        if t.kind == 'punct' and t.value in '([{':
            depth += 1
        elif t.kind == 'punct' and t.value in ')]}':
            if depth == 0:
                if current or args:
                    args.append(current)
                return args, i + 1
            depth -= 1
        elif t.kind == 'punct' and t.value == ',' and depth == 0:
            args.append(current)
            current = []
            i += 1
            continue
        current.append(t)
        i += 1
    return args, i


def literal(arg):
    """The text of an argument made of string literals only (adjacent ones joined); None otherwise."""
    if not arg or any(t.kind != 'string' for t in arg):
        return None
    return ''.join(t.value for t in arg)


def find_texts(path, functions, plurals, warnings):
    with open(path, encoding='utf-8', errors='replace') as f:
        toks = list(tokens(f.read()))
    found = []
    for i, t in enumerate(toks):
        if t.kind != 'name' or t.value not in functions or i + 1 >= len(toks) or toks[i + 1].value != '(':
            continue
        # a member of something else (x.tr, p->tr) is not ours; a declaration (LocalizedText tr(std::string...)) has no
        # literal: passed over below
        if i > 0 and toks[i - 1].value == '.' or (i > 1 and toks[i - 1].value == '>' and toks[i - 2].value == '-'):
            continue
        args, _ = arguments(toks, i + 1)
        where = '%s:%d' % (path, t.line)
        plural = t.value in plurals
        first = literal(args[0]) if args else None
        if first is None:
            # a declaration or a definition (a type before the name: LocalizedText tr(std::string source)) is no call
            declared = i > 0 and toks[i - 1].kind == 'name' and toks[i - 1].value not in ('return', 'co_return', 'throw')
            if warnings is not None and args and not declared:
                warnings.append('%s: %s(...) of a text that is not a literal' % (where, t.value))
            continue
        if plural:
            other = literal(args[1]) if len(args) > 1 else None
            if other is None:
                if warnings is not None:
                    warnings.append('%s: %s(...) without the literal of its other form' % (where, t.value))
                continue
            key = literal(args[4]) if len(args) > 4 else None
            found.append(Found(first, key or first, other, where))
        else:
            key = literal(args[1]) if len(args) > 1 else None
            found.append(Found(first, key or first, None, where))
    return found


def sources(paths):
    for p in paths:
        if os.path.isdir(p):
            for root, dirs, files in os.walk(p):
                dirs[:] = sorted(d for d in dirs if not d.startswith('.') and d not in ('build', 'out', 'node_modules'))
                for name in sorted(files):
                    if name.lower().endswith(EXTENSIONS):
                        yield os.path.join(root, name)
        else:
            yield p


# ---- the catalog ----

def english_of(entry):
    """The English an entry translates (none: written as its translation alone, of whatever the English is)."""
    return entry[0] if isinstance(entry, list) and len(entry) == 2 else None


def translation_of(entry):
    return entry[1] if isinstance(entry, list) and len(entry) == 2 else entry


def is_translated(translation):
    if isinstance(translation, dict):
        return any(v for v in translation.values())
    return bool(translation)


def update(catalog, found, language, drop_obsolete):
    """The catalog brought up to date with the texts found (by key, in the order found); what changed, counted."""
    texts = catalog.get('texts', {})
    obsolete = catalog.get('obsolete', {})
    by_key = {}
    for f in found:
        if f.key in by_key:
            other = by_key[f.key]
            other.where += f.where
            if other.source != f.source:
                print('warning: "%s" is two texts: %s and %s' % (f.key, other.where[0], f.where[0]), file=sys.stderr)
        else:
            by_key[f.key] = f
    report = {'new': [], 'outdated': [], 'obsolete': [], 'untranslated': 0}
    result = {}
    for key, f in by_key.items():
        old = texts.get(key, obsolete.get(key))
        if old is None:
            translation = {form: '' for form in forms_of(language)} if f.plural else ''
            result[key] = [f.source, translation]
            report['new'].append(key)
        else:
            english = english_of(old)
            translation = translation_of(old)
            if english is None:
                english = f.source      # a translation alone: of the English it was made for, taken to be this one
            if english != f.source and is_translated(translation):
                report['outdated'].append(key)
            elif english != f.source:
                english = f.source      # nothing translated: nothing to keep of the old English
            if f.plural and not isinstance(translation, dict):
                translation = {form: '' for form in forms_of(language)}
            result[key] = [english, translation]
        if not is_translated(result[key][1]):
            report['untranslated'] += 1
    # what is no longer in the sources: kept only when translated (nothing else of it is worth keeping)
    gone = {k: v for k, v in list(texts.items()) + list(obsolete.items()) if k not in by_key and is_translated(translation_of(v))}
    report['obsolete'] = [k for k in texts if k not in by_key]
    updated = {'language': language, 'texts': result}
    if gone and not drop_obsolete:
        updated['obsolete'] = gone
    return updated, report


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__.split('\n\n')[0])
    parser.add_argument('paths', nargs='+', help='source files, or folders to look in')
    parser.add_argument('--language', required=True, help='the catalog\'s language (BCP 47: fa, ar-EG)')
    parser.add_argument('--catalog', required=True, help='its JSON file (made when not there)')
    parser.add_argument('--function', action='append', default=[], help='another name that makes a text to translate')
    parser.add_argument('--plural-function', action='append', default=[], help='another name that makes a counted one')
    parser.add_argument('--drop-obsolete', action='store_true', help='forget the texts no longer in the sources')
    parser.add_argument('--check', action='store_true', help='write nothing; fail when the catalog is not up to date')
    parser.add_argument('--warn', action='store_true', help='list the calls whose text is not a literal')
    options = parser.parse_args(argv)

    plurals = {'trPlural'} | set(options.plural_function)
    functions = {'tr'} | set(options.function) | plurals
    warnings = [] if options.warn else None
    found = []
    for path in sources(options.paths):
        found += find_texts(path, functions, plurals, warnings)
    for w in warnings or []:
        print('warning: ' + w, file=sys.stderr)

    catalog = {'language': options.language, 'texts': {}}
    if os.path.exists(options.catalog):
        with open(options.catalog, encoding='utf-8') as f:
            catalog = json.load(f)
        if catalog.get('language') != options.language:
            sys.exit('%s is the catalog of "%s", not of "%s"' % (options.catalog, catalog.get('language'), options.language))
    updated, report = update(catalog, found, options.language, options.drop_obsolete)
    text = json.dumps(updated, ensure_ascii=False, indent=2) + '\n'

    current = None
    if os.path.exists(options.catalog):
        with open(options.catalog, encoding='utf-8') as f:
            current = f.read()
    print('%d texts: %d new, %d untranslated, %d outdated, %d no longer in the sources'
          % (len(updated['texts']), len(report['new']), report['untranslated'], len(report['outdated']), len(report['obsolete'])))
    for key in report['outdated']:
        print('  outdated: ' + key)
    if options.check:
        if text != current:
            print('%s is not up to date' % options.catalog, file=sys.stderr)
            return 1
        return 0
    if text != current:
        folder = os.path.dirname(os.path.abspath(options.catalog))
        os.makedirs(folder, exist_ok=True)
        with open(options.catalog, 'w', encoding='utf-8', newline='\n') as f:
            f.write(text)
    return 0


if __name__ == '__main__':
    sys.exit(main())
