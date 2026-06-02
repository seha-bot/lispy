#ifndef RESOLVED
#define RESOLVED

#include "ast.hpp"
#include "type_storage.hpp"

namespace storage {

struct ResolvedAST {
  ast::ModuleDefinition global_module;
  std::unique_ptr<TypeStorage> ts;
  std::vector<ast::Entity> entities;
};

} // namespace storage

#endif
