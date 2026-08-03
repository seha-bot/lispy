module;

#include <compare>
#include <cstddef>
#include <functional>

export module id;

export namespace id {

enum class Domain {
  entity,
  tag,
  type,
};

template <Domain D> struct Id {
  static constexpr Domain domain = D;
  std::strong_ordering operator<=>(Id const &) const = default;
  std::size_t value;
};

struct EntityId : Id<Domain::entity> {
  bool operator==(EntityId const &) const = default;
};

struct TagId : Id<Domain::tag> {
  auto operator<=>(TagId const &) const = default;
};

struct TypeId : Id<Domain::type> {
  /// Special value which represents the unit type.
  static const TypeId unit_id;
};

constexpr TypeId TypeId::unit_id{{
    .value = static_cast<std::size_t>(-1),
}};

} // namespace id

template <> struct std::hash<id::EntityId> {
  static std::size_t operator()(id::EntityId const &id) noexcept { return id.value; }
};
