/*
 * REQUESTRESPONSE.CPP                      Copyright (c) 2008-2010, Ilmatieteen
 * laitos
 *
 * References to JSONP:
 *
 *   http://bob.pythonmac.org/archives/2005/12/05/remote-json-jsonp/
 *   http://www.drdobbs.com/windows/226500168
 */
#include "RequestResponse.hpp"

#include <cctype>
#include <string>

using namespace std;

/*---=== Lua reader ===---
 *
 * A Lua chunk reader that converts URL encodings on the fly.
 */
struct my_reader_st {
  const char *p;
  char buf; // used for storage of decoded characters

  my_reader_st(const char *code) : p(code) {}
};

static int HEX_TO_INT(char c) {
  // Trusting ASCII order (digits before letters)
  //
  return (c <= '9') ? (c - '0') : (toupper(c) - 'A' + 10);
}

static const char *my_reader(lua_State *L, void *st_v, size_t *size) {
  struct my_reader_st &st = *(struct my_reader_st *)st_v;

  const char *start = st.p;
  if ((!start) || (!*start)) {
    *size = 0;
    return nullptr; // done
  }

  // '%NN' is "any non alphanumeric character"
  // '+' is space (same as '%20')
  //
  // Note: We expect encoding to be valid. If '%' is not followed by two
  //      numbers, the text goes through unchanged. This will probably
  //      cause an error in the script; anyways it should not happen in
  //      practise.
  //
  if (*start == '+') { // so often, it's best to check it first
    st.p++;
    *size = 1;
    return " ";
  }

  if (*start == '%' && isxdigit(start[1]) && isxdigit(start[2])) {
    // '%nn'
    //
    st.buf = (HEX_TO_INT(start[1]) << 4) | HEX_TO_INT(start[2]);
    st.p += 3;
    *size = 1;
    return &st.buf;
  }

  do {
    st.p++;
  } while ((*st.p) && (*st.p != ' ') && (*st.p != '%'));
  *size = st.p - start;
  return start;
}

/*
 * A JSONP callback must be a JavaScript identifier path such as
 * "cb" or "jQuery123.handler".
 */
static bool is_valid_jsonp_callback(const string &name) {
  if (name.empty() || name.size() > 64)
    return false;
  bool start = true;
  for (char c : name) {
    const bool ident_start = isalpha((unsigned char)c) || c == '_' || c == '$';
    if (start) {
      if (!ident_start)
        return false;
      start = false;
    } else if (c == '.') {
      start = true;
    } else if (!ident_start && !isdigit((unsigned char)c)) {
      return false;
    }
  }
  return !start;
}

/*---=== RequestResponse ===---*/

/*
 * Get 'code' (Lua query) and 'callback' (JSONP prefix) parameters and place
 * others in the 'kk' mapping.
 */
void RequestResponse::set_key_val(const string &key, const string &val) {
  if (key == "callback") {
    // SECURITY: the callback name is echoed verbatim in front of the response.
    // Accept only a plain JavaScript identifier path; anything else disables
    // the JSONP wrapping instead of reflecting attacker-chosen text.
    if (is_valid_jsonp_callback(val))
      jsonp_callback = val;
  } else if (key == "code") {
    code = val;
  } else {
    kk[key] = val;
  }
}

/*
 * Setting the code explicitly. Used for WMS wrapper only.
 */
void RequestResponse::set_script(const char *val) { code = val; }

/*
 * Write output, adding JSONP padding if requested by the client.
 */
void RequestResponse::set_output(unsigned resp_code, const char *mime,
                                 const char *s, size_t bytes) {
  bool jsonp_wrap = (jsonp_callback != nullptr) && (resp_code == 200);

  set_code(resp_code);
  set_mime(jsonp_wrap ? "script/java-script" : mime);

  if (bytes == ((size_t)(-1))) {
    bytes = strlen(s);
  }

  if (!jsonp_wrap) {
    append(s, bytes);
  } else {
// Note: Upper levels should have made sure JSONP does not get newlines.
//
#if 1
    for (unsigned i = 0; i < bytes; i++) {
      if (s[i] == '\n') {
        throw E_LOG_BUG0("Multiple lines in JSONP output");
      }
    }
#endif
    string tmp = string_fmt("%s(", jsonp_callback.c_str());
    append(tmp.c_str(), tmp.size());
    append(s, bytes);
    append(")", 1);
  }
}

/*
 * Compile the Lua source code
 */
int RequestResponse::compile_code(lua_State *L, const char *block_name) {
  assert(code != nullptr);

  // SECURITY: never accept precompiled bytecode from the client. LuaJIT does
  // not verify bytecode, so a crafted chunk is a memory corruption primitive
  // that bypasses the whole sandbox. 'lua_load()' accepts both text and binary
  // chunks; 'lua_loadx()' with mode "t" accepts text only. The URL decoding
  // done by 'my_reader' can produce any byte (including ESC), so the mode
  // restriction must be applied here and not by inspecting 'code'.
  //
  struct my_reader_st my_st(code.c_str());

  return lua_loadx(L, my_reader, (void *)&my_st, "=code", "t");
}
