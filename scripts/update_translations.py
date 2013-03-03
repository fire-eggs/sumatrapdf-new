import os, re, util, langs_def

# number of missing translations for a language to be considered
# incomplete (will be excluded from Translations_txt.cpp) as a
# percentage of total string count of that specific file
INCOMPLETE_MISSING_THRESHOLD = 0.2

SRC_DIR = os.path.join(os.path.dirname(__file__), "..", "src")
C_TRANS_FILENAME = "Translations_txt.cpp"

C_DIRS_TO_PROCESS = [".", "installer", "browserplugin"]
# produce a simpler format for these dirs
C_SIMPLE_FORMAT_DIRS = ["installer", "browserplugin"]
# whitelist some files as an optimization
C_FILES_TO_EXCLUDE = [C_TRANS_FILENAME.lower()]

def should_translate(file_name):
    file_name = file_name.lower()
    if not file_name.endswith(".cpp"):
        return False
    return file_name not in C_FILES_TO_EXCLUDE

C_FILES_TO_PROCESS = []
for dir in C_DIRS_TO_PROCESS:
    d = os.path.join(SRC_DIR, dir)
    C_FILES_TO_PROCESS += [os.path.join(d, f) for f in os.listdir(d) if should_translate(f)]

TRANSLATION_PATTERN = r'\b_TRN?\("(.*?)"\)'

def extract_strings_from_c_files(with_paths=False):
    strings = []
    for f in C_FILES_TO_PROCESS:
        file_content = open(f, "r").read()
        file_strings = re.findall(TRANSLATION_PATTERN, file_content)
        if with_paths:
            strings += [(s, os.path.basename(os.path.dirname(f))) for s in file_strings]
        else:
            strings += file_strings
    return util.uniquify(strings)

TRANSLATIONS_TXT_C = """\
/* Generated by scripts\\update_translations.py - DO NOT EDIT MANUALLY */

#ifndef MAKELANGID
#include <windows.h>
#endif

// from http://msdn.microsoft.com/en-us/library/windows/desktop/dd318693(v=vs.85).aspx
// those definition are not present in 7.0A SDK my VS 2010 uses
#ifndef LANG_CENTRAL_KURDISH
#define LANG_CENTRAL_KURDISH 0x92
#endif

#ifndef SUBLANG_CENTRAL_KURDISH_CENTRAL_KURDISH_IRAQ
#define SUBLANG_CENTRAL_KURDISH_CENTRAL_KURDISH_IRAQ 0x01
#endif

#define LANGS_COUNT   %(langs_count)d
#define STRINGS_COUNT %(translations_count)d

typedef struct {
    const char *code;
    const char *fullName;
    LANGID id;
    bool isRTL;
} LangDef;

#define _LANGID(lang) MAKELANGID(lang, SUBLANG_NEUTRAL)

LangDef gLangData[LANGS_COUNT] = {
    %(lang_data)s
};

#undef _LANGID

const char *gTranslations[LANGS_COUNT * STRINGS_COUNT] = {
%(translations)s
};
"""

TRANSLATIONS_TXT_SIMPLE = """\
/* Generated by scripts\\update_translations.py - DO NOT EDIT MANUALLY */

#ifndef MAKELANGID
#include <windows.h>
#endif

int gTranslationsCount = %(translations_count)d;

const WCHAR *gTranslations[] = {
%(translations)s
};

const char *gLanguages[] = { %(langs_list)s };

// from http://msdn.microsoft.com/en-us/library/windows/desktop/dd318693(v=vs.85).aspx
// those definition are not present in 7.0A SDK my VS 2010 uses
#ifndef LANG_CENTRAL_KURDISH
#define LANG_CENTRAL_KURDISH 0x92
#endif

#ifndef SUBLANG_CENTRAL_KURDISH_CENTRAL_KURDISH_IRAQ
#define SUBLANG_CENTRAL_KURDISH_CENTRAL_KURDISH_IRAQ 0x01
#endif

// note: this index isn't guaranteed to remain stable over restarts, so
// persist gLanguages[index/gTranslationsCount] instead
int GetLanguageIndex(LANGID id)
{
    switch (id) {
#define _LANGID(lang) MAKELANGID(lang, SUBLANG_NEUTRAL)
    %(lang_id_to_index)s
#undef _LANGID
    }
}

bool IsLanguageRtL(int index)
{
    return %(rtl_lang_cmp)s;
}
"""

# use octal escapes because hexadecimal ones can consist of
# up to four characters, e.g. \xABc isn't the same as \253c
def c_oct(c):
    o = "00" + oct(ord(c))
    return "\\" + o[-3:]

def c_escape(txt):
    if txt is None:
        return "NULL"
    # escape all quotes
    txt = txt.replace('"', r'\"')
    # and all non-7-bit characters of the UTF-8 encoded string
    txt = re.sub(r"[\x80-\xFF]", lambda m: c_oct(m.group(0)[0]), txt)
    return '"%s"' % txt

def get_trans_for_lang(strings_dict, keys, lang_arg):
    if lang_arg == "en":
        return keys
    trans, untrans = [], []
    for k in keys:
        found = [tr for (lang, tr) in strings_dict[k] if lang == lang_arg]
        if found:
            assert len(found) == 1
            # don't include a translation, if it's the same as the default
            if found[0] == k:
                found[0] = None
            trans.append(found[0])
        else:
            trans.append(None)
            untrans.append(k)
    if len(untrans) > INCOMPLETE_MISSING_THRESHOLD * len(keys):
        return None
    return trans

def lang_sort_func(x,y):
    # special case: default language is first
    if x[0] == "en": return -1
    if y[0] == "en": return 1
    return cmp(x[1], y[1])

def make_lang_id(lang):
    ids = lang[2]
    if ids is None:
        return "-1"
    ids = [el.replace(" ", "_") for el in ids]
    if len(ids) == 2:
        id = "MAKELANGID(LANG_%s, SUBLANG_%s_%s)" % (ids[0], ids[0], ids[1])
    else:
        assert len(ids) == 1
        id = "_LANGID(LANG_%s)" % (ids[0])
    return id.upper()

def is_rtl_lang(lang):
    return "true" if len(lang) > 3 and lang[3] == "RTL" else "false"

# correctly sorts strings containing escaped tabulators
def key_sort_func(a, b):
    return cmp(a.replace(r"\t", "\t"), b.replace(r"\t", "\t"))

def gen_c_code_for_dir(strings_dict, keys, dir_name):
    langs_idx = sorted(langs_def.g_langs, cmp=lang_sort_func)
    assert "en" == langs_idx[0][0]
    translations_count = len(keys)

    lines = []
    incomplete_langs = []
    for lang in langs_idx:
        trans = get_trans_for_lang(strings_dict, keys, lang[0])
        if not trans:
            incomplete_langs.append(lang)
            continue
        lines.append("")
        lines.append("  /* Translations for language %s */" % lang[0])
        lines += ["  %s," % c_escape(t) for t in trans]
    for lang in incomplete_langs:
        langs_idx.remove(lang)
    translations = "\n".join(lines)

    #print [l[1] for l in langs_idx]
    lang_data = ['{ "%s", %s, %s, %s },' % (lang[0], c_escape(lang[1]), make_lang_id(lang), is_rtl_lang(lang)) for lang in langs_idx]
    lang_data = "\n    ".join(lang_data)
    langs_count = len(langs_idx)

    file_content = TRANSLATIONS_TXT_C % locals()
    file_name = os.path.join(SRC_DIR, dir_name, C_TRANS_FILENAME)
    file(file_name, "wb").write(file_content)

def gen_c_code_simple(strings_dict, keys, dir_name):
    langs_idx = sorted(langs_def.g_langs, cmp=lang_sort_func)
    assert "en" == langs_idx[0][0]
    translations_count = len(keys)

    lines = []
    incomplete_langs = []
    for lang in langs_idx:
        trans = get_trans_for_lang(strings_dict, keys, lang[0])
        if not trans:
            incomplete_langs.append(lang)
            continue
        lines.append('  /* Translations for language %s */' % lang[0])
        lines += ['  L"%s",' % t.replace('"', '\\"') if t else '  NULL,' for t in trans]
        lines.append("")
    lines.pop()
    for lang in incomplete_langs:
        langs_idx.remove(lang)
    translations = "\n".join(lines)

    langs_list = ", ".join(['"%s"' % lang[0] for lang in langs_idx] + ["NULL"])
    lang_id_to_index = "\n    ".join(["case %s: return %d;" % (make_lang_id(lang), langs_idx.index(lang) * len(keys)) for lang in langs_idx] + ["default: return -1;"])
    rtl_lang_cmp = " || ".join(["%d == index" % langs_idx.index(lang) * len(keys) for lang in langs_idx if is_rtl_lang(lang) == "true"]) or "false"

    import codecs
    file_content = codecs.BOM_UTF8 + TRANSLATIONS_TXT_SIMPLE % locals()
    file_name = os.path.join(SRC_DIR, dir_name, C_TRANS_FILENAME)
    file(file_name, "wb").write(file_content)

def gen_c_code(strings_dict, strings):
    for dir in C_DIRS_TO_PROCESS:
        keys = [s[0] for s in strings if s[1] == dir and s[0] in strings_dict]
        keys.sort(cmp=key_sort_func)
        if dir not in C_SIMPLE_FORMAT_DIRS:
            gen_c_code_for_dir(strings_dict, keys, dir)
        else:
            gen_c_code_simple(strings_dict, keys, dir)

def main():
    import apptransdl
    changed = apptransdl.downloadAndUpdateTranslationsIfChanged()
    if changed:
        print("\nNew translations received from the server, checkin %s and translations.txt" % C_TRANS_FILENAME)
    else:
        apptransdl.regenerateLangs()

if __name__ == "__main__":
    main()
