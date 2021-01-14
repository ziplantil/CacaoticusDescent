import re
import argparse
import glob
import collections
import traceback


DEFAULT_ENCODING = 'latin-1'
C_STRING_REGEX = r"(\")(?:(?=(\\?))\2.)*?\1"
IGNORE_FUNCTIONS = ["mprintf", "Error", "cfopen", "Warning",
                    "IFTOK", "transl_get_string", "transl_fmt_string"]
MIN_COLUMNS = 33


class VarRef():
    def __init__(self, value):
        self.value = value


def flatten(arr):
    return [x for sarr in arr for x in sarr]


def safefind(haystack, needle, index=0):
    pos = haystack.find(needle, index)
    return len(haystack) if pos < 0 else pos


def minfind(haystack, needles, index=0):
    for i in range(index, len(haystack) - min(len(x) for x in needles) + 1):
        for n in needles:
            if haystack[i:i+len(n)] == n:
                return (i, n)
    return None


def confirm(text, default=None):
    if default == True:
        options = "Yn"
    elif default == False:
        options = "yN"
    else:
        options = "yn"
    while True:
        print("{} [{}]?".format(text, options), end=" ")
        sys.stdout.flush()
        v = input()[:1].lower()
        if not v and default != None:
            return default
        elif v == "y":
            return True
        elif v == "n":
            return False


class CodeSection():
    def __init__(self, allow, text, ln):
        self.allow, self.text, self.linenum = allow, text, ln

    def __repr__(self):
        return "<CodeSection allow={} L={} len={}>".format(self.allow, self.linenum, len(self.text))

    def splitlines(self):
        if '\n' not in self.text:
            return [self]
        code = self.text.replace('\r', '')
        ln = self.linenum
        allow = self.allow
        sections = []

        index = 0
        while (newindex := safefind(code, '\n', index)) < len(code):
            sections.append((code[index:newindex + 1], ln))
            index = newindex + 1
            ln += 1
        sections.append((code[index:newindex], ln))

        csections, lastsec = [], None
        for text, ln in sections:
            # don't allow preprocess lines to be processed
            preproc = (not lastsec or lastsec.linenum ==
                       ln or not text.startswith('#'))
            nignfnc = not any(
                (w + "(") in text or (w + " (") in text for w in IGNORE_FUNCTIONS) and "transl_fmt_string_" not in text
            csec = CodeSection(allow and preproc and nignfnc, text, ln)
            csections.append(csec)
            lastsec = csec

        return csections

    def split(self):
        index = 0
        sections = []
        code = self.text
        ln = self.linenum
        comsyms = {'/*': '*/', '//': '\n'}

        while (result := minfind(code, comsyms.keys(), index)):
            newindex, token = result
            secend = comsyms[token]
            scode = code[index:newindex + len(token)]
            sections.append((True, scode, ln))
            ln += scode.count('\n')
            index = newindex + len(token)

            newindex = safefind(code, secend, index)
            skip = 0 if secend == '\n' else len(secend)
            scode = code[index:newindex + skip]
            sections.append((False, scode, ln))
            ln += scode.count('\n')
            index = newindex + skip

        if index < len(code):
            sections.append((True, code[index:], ln))

        res = [CodeSection(allow, text, ln).splitlines()
               for allow, text, ln in sections]
        return flatten(res)


def strip_c_string(string):
    assert string.startswith('"') and string.endswith('"')
    string = string[1:-1]
    string = string.replace("\\\"", '"')
    string = string.replace("\\\\", "\\")
    return string


def input_key_factory(nrref, keys, filename, linenum):
    def input_key(m):
        string = m.group(0)
        while True:
            print("In file {}, line {}, string {}".format(
                filename, linenum, string))
            print("Enter key or empty to not convert")
            key = input()
            if key:
                value = strip_c_string(string)
                if value.count("%") > value.count("%%"):
                    # contains variables
                    counter = 0

                    def make_variable(m):
                        nonlocal counter
                        text = m.group(0)
                        ctr, fmt = str(counter), ""
                        counter += 1

                        if len(text) > 2:
                            fmt = "|" + text

                        return "{" + ctr + fmt + "}"

                    value = re.sub(r"%-?\.?[0-9]*(\w)", make_variable, value)
                    func = "transl_fmt_string"
                else:
                    func = "transl_get_string"
                if key in keys and keys[key] != value:
                    print("Error: Duplicate key!")
                    continue
                elif key not in keys:
                    keys[key] = value
                nrref.value += 1
                return "{}(\"{}\")".format(func, key)
            else:
                return string
    return input_key


def process_file(keys, filename, encoding, starting_line):
    with open(filename, 'r', encoding=encoding) as f:
        src = f.read()
    text = src
    keyb_int = False
    num_replacements_ref = VarRef(0)

    # split text into sections that we *are* allowed to search
    # and those we are *not* (such as comments, preprocessor
    # statements, etc.)

    sections = CodeSection(True, text, 1).split()

    for section in sections:
        if section.allow and section.linenum >= starting_line:
            try:
                section.text = re.sub(C_STRING_REGEX, input_key_factory(
                    num_replacements_ref, keys, filename, section.linenum), section.text)
            except KeyboardInterrupt:
                keyb_int = True
                break

    num_replacements = num_replacements_ref.value

    if keyb_int:
        if num_replacements == 0 or confirm("Commit {}".format(num_replacements), default=False) is False:
            raise KeyboardInterrupt()

    text = "".join(section.text for section in sections)

    if src != text:
        with open(filename + '.bak', 'w', encoding=encoding) as f:
            f.write(src)
        with open(filename, 'w', encoding=encoding) as f:
            f.write(text)

    if keyb_int:
        raise KeyboardInterrupt()


def main(me, *argv):
    parser = argparse.ArgumentParser()
    parser.add_argument('--encoding', nargs='?', default=DEFAULT_ENCODING)
    parser.add_argument('files', type=str, nargs='+')
    args = parser.parse_args(argv)
    keys = collections.OrderedDict()

    for pattern in args.files:
        starting_line = 0
        if '+' in pattern:
            pattern, starting_line = pattern.rsplit('+', 1)
            starting_line = int(starting_line)
        try:
            for filename in glob.iglob(pattern):
                process_file(keys, filename, args.encoding, starting_line)
        except KeyboardInterrupt:
            break
        except:
            traceback.print_exc()
            break

    if len(keys):
        print("")
        maxkeylen = max(max(len(key) for key in keys.keys()) + 4, MIN_COLUMNS)
        for key, value in keys.items():
            print("{}={}".format(key.ljust(maxkeylen), value))

    return 0


if __name__ == '__main__':
    import sys
    sys.exit(main(*sys.argv))
