// Minimal JSON reader: enough for levels/*.json and tunables.json. Throws std::runtime_error on bad input.
#pragma once
#include <cstdlib>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

struct Json {
  enum Type { Null, Bool, Num, Str, Arr, Obj } type = Null;
  double num = 0;
  bool b = false;
  std::string str;
  std::vector<Json> arr;
  std::vector<std::pair<std::string, Json>> obj;

  const Json* get(const std::string& k) const {
    for (auto& [key, v] : obj) if (key == k) return &v;
    return nullptr;
  }
  float numOr(const std::string& k, float d) const { auto v = get(k); return v && v->type == Num ? float(v->num) : d; }
  std::string strOr(const std::string& k, const std::string& d) const { auto v = get(k); return v && v->type == Str ? v->str : d; }

  static Json parse(const std::string& s) {
    size_t i = 0;
    Json j = value(s, i);
    ws(s, i);
    if (i != s.size()) fail("trailing characters", i);
    return j;
  }

 private:
  [[noreturn]] static void fail(const char* what, size_t i) { throw std::runtime_error(std::string("JSON: ") + what + " at " + std::to_string(i)); }
  static void ws(const std::string& s, size_t& i) { while (i < s.size() && (s[i] == ' ' || s[i] == '\n' || s[i] == '\r' || s[i] == '\t')) i++; }
  static bool lit(const std::string& s, size_t& i, const char* w) {
    size_t n = std::char_traits<char>::length(w);
    if (s.compare(i, n, w) != 0) return false;
    i += n;
    return true;
  }
  static std::string string(const std::string& s, size_t& i) {
    std::string out;
    i++;  // opening quote
    while (i < s.size() && s[i] != '"') {
      char c = s[i++];
      if (c != '\\') { out += c; continue; }
      if (i >= s.size()) fail("bad escape", i);
      char e = s[i++];
      switch (e) {
        case 'n': out += '\n'; break;
        case 't': out += '\t'; break;
        case 'r': out += '\r'; break;
        case 'b': out += '\b'; break;
        case 'f': out += '\f'; break;
        case 'u': {  // BMP code point to UTF-8
          if (i + 4 > s.size()) fail("bad \\u escape", i);
          unsigned cp = std::stoul(s.substr(i, 4), nullptr, 16);
          i += 4;
          if (cp < 0x80) out += char(cp);
          else if (cp < 0x800) { out += char(0xC0 | (cp >> 6)); out += char(0x80 | (cp & 63)); }
          else { out += char(0xE0 | (cp >> 12)); out += char(0x80 | ((cp >> 6) & 63)); out += char(0x80 | (cp & 63)); }
          break;
        }
        default: out += e;  // \" \\ \/
      }
    }
    if (i >= s.size()) fail("unterminated string", i);
    i++;
    return out;
  }
  static Json value(const std::string& s, size_t& i) {
    ws(s, i);
    if (i >= s.size()) fail("unexpected end", i);
    Json j;
    char c = s[i];
    if (c == '{') {
      j.type = Obj; i++; ws(s, i);
      if (i < s.size() && s[i] == '}') { i++; return j; }
      for (;;) {
        ws(s, i);
        if (i >= s.size() || s[i] != '"') fail("expected key", i);
        std::string k = string(s, i);
        ws(s, i);
        if (i >= s.size() || s[i] != ':') fail("expected ':'", i);
        i++;
        j.obj.emplace_back(std::move(k), value(s, i));
        ws(s, i);
        if (i < s.size() && s[i] == ',') { i++; continue; }
        if (i < s.size() && s[i] == '}') { i++; return j; }
        fail("expected ',' or '}'", i);
      }
    }
    if (c == '[') {
      j.type = Arr; i++; ws(s, i);
      if (i < s.size() && s[i] == ']') { i++; return j; }
      for (;;) {
        j.arr.push_back(value(s, i));
        ws(s, i);
        if (i < s.size() && s[i] == ',') { i++; continue; }
        if (i < s.size() && s[i] == ']') { i++; return j; }
        fail("expected ',' or ']'", i);
      }
    }
    if (c == '"') { j.type = Str; j.str = string(s, i); return j; }
    if (lit(s, i, "true")) { j.type = Bool; j.b = true; return j; }
    if (lit(s, i, "false")) { j.type = Bool; return j; }
    if (lit(s, i, "null")) return j;
    char* end = nullptr;
    j.num = std::strtod(s.c_str() + i, &end);
    if (end == s.c_str() + i) fail("unexpected character", i);
    j.type = Num;
    i = size_t(end - s.c_str());
    return j;
  }
};
