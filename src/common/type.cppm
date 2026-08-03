module;

#include <cstddef>
#include <variant>
#include <vector>

export module type;

import id;

export namespace type {

struct Arrow {
  id::TypeId from_id;
  id::TypeId to_id;
};

// This is indexed by De Bruijn indices to simplify merging.
struct ForAll {
  id::TypeId type_id;
};

struct DeBruijnIndex {
  std::size_t value;
};

struct Element {
  id::TagId tag_id;
  id::TypeId type_id;
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
  id::TypeId function_id;
  id::TypeId argument_id;
};

// Each object represents a unique variable.
struct Variable {};

struct NamedTypeReference {
  id::FormId definition_id;
};

using TypeBase = std::variant<Arrow, ForAll, DeBruijnIndex, Variant, Struct, Application, Variable,
                              NamedTypeReference>;
struct Type : TypeBase {
  using TypeBase::TypeBase;
};

} // namespace type
