#include "content.hpp"

#include "json.hpp"
#include "util.hpp"

float Record::num(const std::string& k, float d) const {
  for (auto& [key, v] : nums) if (key == k) return v;
  return d;
}
std::string Record::str(const std::string& k, const std::string& d) const {
  for (auto& [key, v] : strs) if (key == k) return v;
  return d;
}
bool Record::has(const std::string& k) const {
  for (auto& [key, v] : nums) if (key == k) return true;
  for (auto& [key, v] : strs) if (key == k) return true;
  return false;
}

int RecordSet::find(const std::string& name) const {
  for (size_t i = 0; i < list.size(); i++) if (list[i].name == name) return int(i);
  return -1;
}

RecordSet loadRecords(const std::string& path) {
  RecordSet s;
  s.path = path;
  Json j = Json::parse(readFile(path));
  if (j.type != Json::Obj) throw std::runtime_error(path + ": expected an object of entries");
  for (auto& [name, e] : j.obj) {
    Record r;
    r.name = name;
    auto add = [&](const std::string& k, const Json& v) {
      if (v.type == Json::Num) r.nums.push_back({k, float(v.num)});
      else if (v.type == Json::Bool) r.nums.push_back({k, v.b ? 1.f : 0.f});
      else if (v.type == Json::Str) r.strs.push_back({k, v.str});
    };
    for (auto& [k, v] : e.obj) {
      if (v.type == Json::Obj) for (auto& [k2, v2] : v.obj) add(k + "." + k2, v2);
      else add(k, v);
    }
    s.list.push_back(std::move(r));
  }
  return s;
}

bool saveRecords(const RecordSet& s) {
  Json j;
  try { j = Json::parse(readFile(s.path)); } catch (const std::exception&) { j.type = Json::Obj; }
  for (auto& r : s.list) {
    Json& e = j.at(r.name);
    for (auto& [k, v] : r.nums) {
      size_t dot = k.find('.');
      Json& slot = dot == std::string::npos ? e.at(k) : e.at(k.substr(0, dot)).at(k.substr(dot + 1));
      if (slot.type == Json::Bool) slot.b = v != 0;
      else slot.setNum(v);
    }
  }
  std::string out = j.dump() + "\n";
  return platform::writeText(s.path, out);
}

Content loadContent(const std::string& dir) {
  return {loadRecords(dir + "/moves.json"), loadRecords(dir + "/enemies.json"), loadRecords(dir + "/items.json")};
}
