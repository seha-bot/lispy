#ifndef EMITTER_HPP
#define EMITTER_HPP

#include "storage/resolved.hpp"

namespace emitter {

void emit(storage::ResolvedAST const &ast, storage::TypeEnv const &type_env, std::ostream &os);

}

#endif
