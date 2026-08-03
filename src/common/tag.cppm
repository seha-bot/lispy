module;

#include <cstddef>
#include <string>

export module tag;

export namespace tag {

struct Tag {
  bool operator==(Tag const &) const = default;
  std::string name;
};

} // namespace tag

template <> struct std::hash<tag::Tag> {
  static std::size_t operator()(tag::Tag const &tag) noexcept {
    return std::hash<std::string>{}(tag.name);
  }
};
