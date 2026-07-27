#ifndef RESOLVED
#define RESOLVED

#include "ast.hpp"
#include "type_storage.hpp"

namespace storage {

struct ResolvedAST {
  ast::entity::ModuleDefinition global_module;
  std::unique_ptr<TypeStorage> ts;
  std::vector<ast::entity::ModuleEntity> entities;
  std::vector<ast::Tag> tags;
};

} // namespace storage

#endif
