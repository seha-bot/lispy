#include <algorithm>
#include <cassert>
#include <cstddef>
#include <expected>
#include <functional>
#include <iostream>
#include <memory>
#include <optional>
#include <ostream>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>
#include <catch2/catch_test_macros.hpp>

import constraint;
import entity;
import expr;
import formatter;
import id;
import resolved;
import tag;
import type;
import type_storage;
import typed_expr;

// std::expected<std::vector<TypedValueEntity>, Error>
// typecheck(storage::TypeStorage &ts, std::vector<tag::Tag> const &tags,
//           std::vector<entity::TypeFormDefinition> const &forms,
//           std::vector<entity::ModuleEntity<expr::Expr>> entities) noexcept;

TEST_CASE("Inference", "[typechecker]") {
  REQUIRE(0 == 1 - 1);
  //
}
