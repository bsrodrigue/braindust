#include "tunables.hpp"

#include <iterator>

#include "json.hpp"
#include "util.hpp"

Tunables loadTunables(const std::string& path) {
  Tunables t;
  if (!platform::exists(path)) return t;
  try {
    Json j = Json::parse(readFile(path));
    for (auto& f : TUNABLE_FIELDS) t.*f.field = j.numOr(f.name, t.*f.field);
  } catch (const std::exception& e) { TraceLog(LOG_WARNING, "%s: %s", path.c_str(), e.what()); }
  return t;
}

bool saveTunables(const std::string& path, const Tunables& t) {
  std::string out = "{\n";
  for (size_t i = 0; i < std::size(TUNABLE_FIELDS); i++)
    out += TextFormat(" \"%s\": %g%s\n", TUNABLE_FIELDS[i].name, t.*TUNABLE_FIELDS[i].field, i + 1 < std::size(TUNABLE_FIELDS) ? "," : "");
  out += "}\n";
  return platform::writeText(path, out);
}
