#ifndef TAG_STORAGE
#define TAG_STORAGE

#include <functional>
#include <unordered_map>
#include <utility>
#include <vector>

#include "ast.hpp"

namespace parser {

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
  using Hasher = decltype([](ast::Tag const &x) { return std::hash<std::string>{}(x.name); });
  std::unordered_map<ast::Tag, ast::TagId, Hasher> m_tags;
};

} // namespace parser

#endif
