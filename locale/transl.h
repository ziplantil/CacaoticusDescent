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
#include <unordered_map>

struct transl_compiled_string_token;

struct transl_compiled_string
{
    std::vector<transl_compiled_string_token> tokens;
};

struct transl_compiled_string_token_variable
{
    std::string form;
    transl_compiled_string fallback;
};

struct transl_compiled_string_token
{
    int number;
    std::string text;
    transl_compiled_string_token_variable var;
};

struct transl_entry
{
    transl_compiled_string base;
    std::string pass;
    std::unordered_map<std::string, transl_compiled_string> forms;
};

enum class transl_param_type
{
    TKEY,
    STRING,
    INT
};

struct transl_param
{
    transl_param_type type;
    int intval;
    std::string text;
    transl_entry* entry;
};

typedef std::unordered_map<std::string, transl_entry> transl_stringmap;

int transl_load_language(const char* hog);
const char* transl_get_plural_key(const char* key, int n);
const char* transl_get_atom(const char* key, const char* form);
const char* transl_get_string(const char* key);

const char* transl_fmt_string_t(const char* key, const char* t1);
const char* transl_fmt_string_tt(const char* key, const char* t1, const char* t2);
const char* transl_fmt_string_s(const char* key, const char* s1);
const char* transl_fmt_string_i(const char* key, int i);
const char* transl_fmt_string_is(const char* key, int i, const char* s1);
const char* transl_fmt_string_si(const char* key, const char* s1, int i);
const char* transl_fmt_string_ti(const char* key, const char* t1, int i);

extern char LangHogfile_initialized;
