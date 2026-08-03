module;

#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

export module tag_storage;

import id;
import tag;

export namespace parser {

struct TagStorage {
  id::TagId get_tag(std::string name) {
    id::TagId const tag_id{m_tags.size()};
    tag::Tag tag{std::move(name)};

    auto [iter, did_insert] = m_tags.insert({std::move(tag), tag_id});
    if (did_insert) {
      iter->second = tag_id;
    }
    return iter->second;
  }

  std::vector<tag::Tag> finalize() && {
    std::vector<tag::Tag> tags(m_tags.size());
    while (not m_tags.empty()) {
      auto handle = m_tags.extract(m_tags.begin());
      tags[handle.mapped().value] = std::move(handle.key());
    }
    return tags;
  }

private:
  std::unordered_map<tag::Tag, id::TagId> m_tags;
};

} // namespace parser
