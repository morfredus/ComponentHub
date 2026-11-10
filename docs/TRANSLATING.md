# Translating ComponentHub

ComponentHub is translated by adding a catalog file: no code to write. French and
Spanish are already there; English is the language of the source strings, so it needs no
catalog.

## Add a language

1. Create the catalog from the current strings (use the language and country code):

   ```bash
   python scripts/i18n/update-catalogs.py --new de_DE
   ```

   This creates `translations/componenthub_de.ts` with every string waiting for a
   translation. You can also copy `translations/componenthub_template.ts` and set
   `language="de_DE"` on the `<TS>` line.

2. Open the file in **Qt Linguist** (free, ships with Qt; on Windows with MSYS2:
   `pacman -S mingw-w64-x86_64-qt6-tools`) and translate. Each line shows the English text,
   and sometimes a short comment telling two meanings of the same word apart. Any text
   editor works too: fill the `<translation>` element and remove `type="unfinished"`.

3. Translate the string whose English text is just `English`: it is the **name of your
   language written in itself** (for example `Deutsch`), shown in Settings → Language.

4. Rebuild. The language shows up by itself in the selector; nothing else to register.

5. Check that nothing is missing:

   ```bash
   python scripts/i18n/update-catalogs.py --check
   ```

## Rules that keep a translation correct

- `%1`, `%2`... are placeholders replaced by the application (a name, a number). Keep
  them, in any order your language needs.
- `\n` is a line break: keep it where the English text has it.
- `&` before a letter marks the keyboard accelerator of a menu entry (`&File`). Pick a
  letter that is unique in that menu.
- Keep the punctuation style of your language (spaces before `:` in French, for example).
- The long texts (help and about window) are not in the catalogs: translate
  `resources/doc/help.en.html` and `resources/doc/about.en.html` into
  `help.<code>.html` and `about.<code>.html`, then add the two files to
  `resources/app.qrc` next to the others. Without them, the English text is shown.

## Update after the code changed

When a contributor adds or edits strings, run `python scripts/i18n/update-catalogs.py`.
Existing translations are kept, new strings arrive empty, removed strings disappear.
Review the diff of the `.ts` files like any other file: they contain no line numbers, so
only real changes show up.

## Why a script instead of `lupdate`

All strings go through `chui::tr("...")` and `CHUI_NOOP("...")` under one context, which
`lupdate` does not follow without extra setup. The script only reads those literals and
needs nothing but Python 3.
