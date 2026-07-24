#pragma once

#include <vector>

#include "common/schema.hpp"
#include "common/value.hpp"
#include "parser/ast.hpp"
#include "utilities/status.hpp"

namespace minidb::executor {

util::Result<minidb::common::Value> Evaluate(const minidb::parser::Expr& expr,
                                              const minidb::common::Schema& schema,
                                              const std::vector<minidb::common::Value>& row);

util::Result<bool> EvaluatePredicate(const minidb::parser::Expr* expr, const minidb::common::Schema& schema,
                                      const std::vector<minidb::common::Value>& row);

}  
