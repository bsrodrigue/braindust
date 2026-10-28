// Moves, enemy types and items: data/moves.json, data/enemies.json, data/items.json.
//
// Each entry is a record of named numbers and strings. Code reads a field by name with a default, so a file can
// set any field the code knows; the F2 tune menu lists every number and saves it back. One level of nesting is
// flattened with a dot: { "resist": { "slash": 0 } } is the number "resist.slash".
#pragma once
#include <string>
#include <utility>
#include <vector>

struct Record {
  std::string name;
  std::vector<std::pair<std::string, float>> nums;  // file order, so the tune menu shows them as written
  std::vector<std::pair<std::string, std::string>> strs;

  float num(const std::string& k, float d = 0) const;
  std::string str(const std::string& k, const std::string& d = "") const;
  bool has(const std::string& k) const;
};

struct RecordSet {
  std::string path;
  std::vector<Record> list;
  int find(const std::string& name) const;  // -1 when missing
};

RecordSet loadRecords(const std::string& path);  // throws on a missing or malformed file
bool saveRecords(const RecordSet& s);            // writes the numbers back into the file, keeping everything else

struct Content {
  RecordSet moves, enemies, items;
};
Content loadContent(const std::string& dir);  // dir/moves.json, dir/enemies.json, dir/items.json; throws
