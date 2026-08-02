module;

#include <cstddef>
#include <functional>

export module entity;

export namespace entity {

struct Id {
  bool operator==(Id const &) const = default;
  std::size_t value;
};

} // namespace entity

template <> struct std::hash<entity::Id> {
  static std::size_t operator()(entity::Id const &id) noexcept { return id.value; }
};
