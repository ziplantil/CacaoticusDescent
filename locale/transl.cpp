/* Cacaoticus Descent - Translation core library */
/*
The code contained in this file is not the property of Parallax Software,
and is not under the terms of the Parallax Software Source license.
Instead, it is released under the terms of the MIT License,
as described in copying.txt
*/
/* NO CONCURRENCY SUPPORT! */

#pragma once

#include <string>
#include <vector>
#include <utility>
#include "locale/transl.h"
#include "cfile/cfile.h"
#include "misc/error.h"

#define STRING_CACHE_SIZE 32
#define STRING_TMP_DEFAULT 128

transl_stringmap strings;
const char* language_code = "";
const std::string empty_string = "";
std::string tmp[STRING_CACHE_SIZE];
int tmpi = 0;

transl_compiled_string transl_parse_string(const char** bufp, bool brace);

transl_compiled_string_token transl_parse_brace(const char** bufp)
{
    const char* buf = *bufp;
    int number = 0;
    std::string form;
    transl_compiled_string fallback;
    while ('0' <= *buf && *buf <= '9')
        number = number * 10 + (*buf++ - '0');
    if (*buf == '<')
    {
        ++buf;
        const char* cstart = buf;
        while (*buf && *buf != '|' && *buf != '}')
            ++buf;
        form = std::string(cstart, buf - cstart);
    }
    if (*buf == '|')
    {
        ++buf;
        fallback = transl_parse_string(&buf, true);
    }
    if (*buf == '}')
        ++buf;
    else
        Assert(!*buf);
    *bufp = buf;
    return transl_compiled_string_token{ number, "", transl_compiled_string_token_variable{ form, fallback } };
}

int valid_hex_digit(char* c)
{
    if ('0' <= *c && *c <= '9')
    {
        *c -= '0';
        return 1;
    }
    if ('A' <= *c && *c <= 'F')
    {
        *c += 10 - 'A';
        return 1;
    }
    if ('a' <= *c && *c <= 'f')
    {
        *c += 10 - 'a';
        return 1;
    }
    return 0;
}

transl_compiled_string transl_parse_string(const char** bufp, bool brace)
{
    const char* buf = *bufp;
    transl_compiled_string res;
    char c;
    std::vector<transl_compiled_string_token> tokens;
    std::string value;
    value.reserve(1024);
    while ((c = *buf) && c != '\n')
    {
        if (c == '\\')
        {
            c = *++buf;
            if (c == 't')
                value += '\t';
            else if (c == 'n')
                value += '\n';
            else if (c == 'x')
            {
                char a = 0, b = 0;
                if (*buf) a = *++buf;
                if (*buf) b = *++buf;
                if (valid_hex_digit(&a) && valid_hex_digit(&b))
                    value += (char)((a << 4) | b);
            }
            else
                value += c;
        }
        else if (c == '{')
        {
            if (!value.empty())
                tokens.push_back({ -1, value });
            value = "";
            ++buf;
            tokens.push_back(transl_parse_brace(&buf));
            continue;
        }
        else if (brace && c == '}')
            break;
        else
            value += *buf;
        ++buf;
    }

    if (!value.empty())
        tokens.push_back({ -1, value });

    *bufp = buf;
    return { std::move(tokens) };
}

const char* rtrim(const char* end, const char* start)
{
    while (end > start && *(end - 1) == ' ')
        --end;
    return end;
}

void transl_parse_line(const char* buf)
{
    char c;
    while ((c = *buf) && c == ' ')
        ++buf;
    if (!c) return;
    // comments
    if (c == '#') return;
    
    const char* collectstart = buf;
    std::string key, form, pass, *collecting = &key;

    while ((c = *buf) && c != '\n')
    {
        if (c == '>' && collecting == &key)
        {
            const char* endbuf = rtrim(buf, collectstart);
            *collecting = std::string(collectstart, endbuf - collectstart);
            collecting = &pass, collectstart = buf + 1;
        }
        else if (c == '<' && collecting == &key)
        {
            const char* endbuf = rtrim(buf, collectstart);
            *collecting = std::string(collectstart, endbuf - collectstart);
            collecting = &form, collectstart = buf + 1;
        }
        else if (c == '=')
        {
            const char* endbuf = rtrim(buf, collectstart);
            *collecting = std::string(collectstart, endbuf - collectstart);
            collecting = NULL;
            break;
        }
        else
            ++buf;
    }

    if (collecting || *buf++ != '=')
        return;

    transl_compiled_string str = transl_parse_string(&buf, false);
    auto res = strings.emplace(key, transl_entry{ str, pass });
    if (res.second)
        res.first->second.base = str;
    if (!form.empty())
        res.first->second.forms.emplace(form, str);
}

int transl_load_language(const char* hog)
{
    int i;
    for (i = 0; i < STRING_CACHE_SIZE; ++i)
    {
        tmp[i].clear();
        tmp[i].reserve(STRING_TMP_DEFAULT);
    }
    
    char buf[1024];
    if (!cfile_use_langfile(hog))
    {
        Error("Cannot find specified language");
        return 0;
    }

    strings.clear();
    CFILE* strfile = cfopen("STRINGS.LNG", "rb");
    if (strfile)
    {
        while (cfgets(buf, sizeof(buf), strfile))
            transl_parse_line(buf);
        cfclose(strfile);
        if (language_code = transl_get_atom("LanguageCode", ""))
            return 1;
    }

    Error("Invalid or missing STRINGS.LNG");
    return 0;
}

const std::string& transl_raw(std::string& buf, transl_compiled_string& fmt, bool shortcut)
{
    buf.clear();
    if (fmt.tokens.size() == 0)
        return shortcut ? empty_string : (buf.clear(), buf);
    else if (fmt.tokens.size() == 1 && fmt.tokens[0].number < 0)
        return shortcut ? fmt.tokens[0].text : (buf = fmt.tokens[0].text, buf);
    for (transl_compiled_string_token& tok : fmt.tokens)
    {
        if (tok.number < 0) /* itself a variable */
            buf += "{" + std::to_string(tok.number) + "}";
        else
            buf += tok.text;
    }

    return buf;
}

const std::string& transl_format(std::string& buf, transl_compiled_string& fmt, const std::vector<transl_param>& args, bool shortcut)
{
    buf.clear();
    if (fmt.tokens.size() == 0)
        return shortcut ? empty_string : (buf.clear(), buf);
    else if (fmt.tokens.size() == 1 && fmt.tokens[0].number < 0)
        return shortcut ? fmt.tokens[0].text : (buf = fmt.tokens[0].text, buf);
    for (transl_compiled_string_token& tok : fmt.tokens)
    {
        if (tok.number < 0)
            buf += tok.text;
        else /* variable */
        {
            int n = tok.number;
            if (n >= args.size())
                buf += "{" + std::to_string(tok.number) + "}";
            else
            {
                transl_compiled_string_token_variable& var = tok.var;
                const transl_param& entry = args[n];
                switch (entry.type)
                {
                case transl_param_type::INT:
                    if (tok.var.form.empty())
                        buf += std::to_string(entry.intval);
                    else
                    {
                        bool fullwidth = false;
                        std::string tmps, form = tok.var.form;
                        if (form.rfind("fw", 0) == 0)
                        {
                            fullwidth = true;
                            form = form.substr(2);
                        }
                        if (!form.empty() && form[0] == '%')
                        {
                            char fbuf[16];
                            snprintf(fbuf, sizeof(fbuf), form.c_str(), entry.intval);
                            tmps = std::string(fbuf);
                        }
                        else
                            tmps = std::to_string(entry.intval);
                        if (fullwidth) /* convert to fullwidth numerals */
                        {
                            std::string nums = tmps;
                            tmps.clear();
                            tmps.reserve(nums.length());
                            for (const char& c : nums)
                            {
                                if (c == '-') tmps += "\xef\xbc\x8d";
                                else if ('0' <= c && c <= '9')
                                {
                                    tmps += "\xef\xbc";
                                    tmps += c + 0x60;
                                }
                                else tmps += c;
                            }
                        }
                        buf += tmps;
                    }
                    break;
                case transl_param_type::STRING:
                    buf += entry.text;
                    break;
                case transl_param_type::TKEY:
                    {
                        transl_entry* ptr = entry.entry;
                        std::string tmps;
                        if (tok.var.form.empty())
                            transl_raw(tmps, ptr->base, false);
                        else
                        {
                            auto it = ptr->forms.find(tok.var.form);
                            if (it != ptr->forms.end())
                                transl_raw(tmps, it->second, false);
                            else
                                transl_format(tmps, tok.var.fallback, { entry }, false);
                        }
                        buf += tmps;
                    }
                    break;
                }
            }
        }
    }
    return buf;
}

void transl_collect_form(std::string& form, transl_compiled_string& fmt, const std::vector<transl_param>& args)
{
    bool first = true;
    bool allempty = true;
    form.clear();
    for (const transl_param& entry : args)
    {
        if (first)
            first = false;
        else
            form += ";";
        if (entry.type == transl_param_type::TKEY)
        {
            std::string& s = entry.entry->pass;
            form += s;
            allempty &= s.empty();
        }
    }
    if (allempty)
        form.clear();
}

const std::string& transl_format(std::string& buf, transl_entry& fmt, const std::vector<transl_param>& args, bool shortcut)
{
    std::string form;
    transl_collect_form(form, fmt.base, args);
    if (!form.empty())
    {
        auto it = fmt.forms.find(form);
        return transl_format(buf, it != fmt.forms.end() ? it->second : fmt.base, args, shortcut);
    }
    return transl_format(buf, fmt.base, args, shortcut);
}

std::string& transl_unknown(std::string& buf, const char* key)
{
    buf.clear();
    buf += "?{";
    buf.append(key);
    buf += "}";
    return buf;
}

const char* transl_get_atom(const char* key, const char* form)
{
    int j = tmpi; tmpi = (tmpi + 1) % STRING_CACHE_SIZE;
    auto it = strings.find(key);
    if (it == strings.end())
        return NULL;
    if (!form || !*form)
        return transl_raw(tmp[j], it->second.base, true).c_str();
    auto it2 = it->second.forms.find(form);
    if (it2 == it->second.forms.end())
        return transl_raw(tmp[j], it->second.base, true).c_str();
    return transl_raw(tmp[j], it2->second, true).c_str();
}

const char* transl_get_string_int(const char* key, const std::vector<transl_param>& args)
{
    int j = tmpi; tmpi = (tmpi + 1) % STRING_CACHE_SIZE;
    auto it = strings.find(key);
    if (it == strings.end())
        return transl_unknown(tmp[j], key).c_str();
    return transl_format(tmp[j], it->second, args, true).c_str();
}

static inline transl_param transl_param_tkey(const char* key)
{
    auto it = strings.find(key);
    transl_param res;
    bool found = (it != strings.end());
    if (found)
    {
        res.type = transl_param_type::TKEY;
        res.entry = &it->second;
    }
    else
    {
        res.type = transl_param_type::STRING;
        res.text = "?{"; res.text += key; res.text += "}";
    }
    return res;
}

static inline transl_param transl_param_text(const char* s)
{
    return transl_param{ transl_param_type::STRING, 0, s, NULL };
}

static inline transl_param transl_param_int(int i)
{
    return transl_param{ transl_param_type::INT, i, "", NULL };
}

const char* transl_get_string(const char* key)
{
    return transl_get_string_int(key, {});
}

const char* transl_fmt_string_t(const char* key, const char* t1)
{
    return transl_get_string_int(key, { transl_param_tkey(t1) });
}

const char* transl_fmt_string_tt(const char* key, const char* t1, const char* t2)
{
    return transl_get_string_int(key, { transl_param_tkey(t1), transl_param_tkey(t2) });
}

const char* transl_fmt_string_s(const char* key, const char* s1)
{
    return transl_get_string_int(key, { transl_param_text(s1) });
}

const char* transl_fmt_string_i(const char* key, int i)
{
    return transl_get_string_int(key, { transl_param_int(i) });
}

const char* transl_fmt_string_is(const char* key, int i, const char* s1)
{
    return transl_get_string_int(key, { transl_param_int(i), transl_param_text(s1) });
}

const char* transl_fmt_string_si(const char* key, const char* s1, int i)
{
    return transl_get_string_int(key, { transl_param_text(s1), transl_param_int(i) });
}

const char* transl_fmt_string_ti(const char* key, const char* t1, int i)
{
    return transl_get_string_int(key, { transl_param_tkey(t1), transl_param_int(i) });
}

const char* transl_pluralize(int n)
{
    // language code dependent code can go here
    if (n == 1)
        return "One";
    else
        return "Other";
}

const char* transl_get_plural_key(const char* key, int n)
{
    int j = tmpi; tmpi = (tmpi + 1) % STRING_CACHE_SIZE;
    tmp[j] = key;
    tmp[j] += transl_pluralize(n);
    return tmp[j].c_str();
}
