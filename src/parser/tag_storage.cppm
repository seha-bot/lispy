module;

#include <functional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

export module tag_storage;

import ast;

export namespace parser {

struct TagStorage {
  ast::TagId get_tag(std::string name) {
    ast::TagId const tag_id{m_tags.size()};
    ast::Tag tag{std::move(name)};

    auto [iter, did_insert] = m_tags.insert({std::move(tag), tag_id});
    if (did_insert) {
      iter->second = tag_id;
    }
    return iter->second;
  }

  std::vector<ast::Tag> finalize() && {
    std::vector<ast::Tag> tags(m_tags.size());
    while (not m_tags.empty()) {
      auto handle = m_tags.extract(m_tags.begin());
      tags.push_back(std::move(handle.key()));
    }
    return tags;
  }

private:
  std::unordered_map<ast::Tag, ast::TagId> m_tags;
};

} // namespace parser
