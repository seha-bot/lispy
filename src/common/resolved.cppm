module;

#include <memory>
#include <vector>

// TODO: This is a stupid module. Get rid of it.
export module resolved;

import entity;
import expr;
import tag;
import type_storage;

export namespace storage {

struct ResolvedAST {
  entity::ModuleDefinition global_module;
  std::unique_ptr<TypeStorage> ts;
  std::vector<entity::ModuleEntity<expr::Expr>> entities;
  std::vector<entity::TypeFormDefinition> forms;
  std::vector<tag::Tag> tags;
};

} // namespace storage
