module;

#include <memory>
#include <vector>

export module resolved;

import ast;
import type_storage;

export namespace storage {

struct ResolvedAST {
  ast::entity::ModuleDefinition global_module;
  std::unique_ptr<TypeStorage> ts;
  std::vector<ast::entity::ModuleEntity> entities;
  std::vector<ast::Tag> tags;
};

} // namespace storage
