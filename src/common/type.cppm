module;

#include <cstddef>
#include <variant>
#include <vector>

export module type;

import entity;
import tag;

export namespace type {

struct Id {
  /// Special value which represents the unit type.
  static const Id unit_id;

  std::size_t value;
};

constexpr Id Id::unit_id = {.value = static_cast<std::size_t>(-1)};

struct Arrow {
  Id from_id;
  Id to_id;
};

// This is indexed by De Bruijn indices to simplify merging.
struct ForAll {
  Id type_id;
};

struct DeBruijnIndex {
  std::size_t value;
};

struct Element {
  tag::Id tag_id;
  Id type_id;
};

struct Variant {
  // FIX: DO NOT RELY ON THIS PLEASE!
  // It is safe to assume that this is sorted by tag_id.
  std::vector<Element> elements;
};

struct Struct {
  // FIX: DO NOT RELY ON THIS PLEASE!
  // It is safe to assume that this is sorted by tag_id.
  std::vector<Element> elements;
};

struct Application {
  Id function_id;
  Id argument_id;
};

// Each object represents a unique variable.
struct Variable {};

struct NamedTypeReference {
  // TODO: Strongly type this so that it's guaranteed it represents a TypeFormDefinition.
  entity::Id definition_id;
};

using TypeBase = std::variant<Arrow, ForAll, DeBruijnIndex, Variant, Struct, Application, Variable,
                              NamedTypeReference>;
struct Type : TypeBase {
  using TypeBase::variant;
};

} // namespace type
