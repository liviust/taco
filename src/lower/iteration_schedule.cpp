#include "iteration_schedule.h"

#include <set>
#include <vector>
#include <queue>

#include "taco/tensor.h"
#include "taco/expr_nodes/expr_nodes.h"
#include "taco/expr_nodes/expr_visitor.h"
#include "iteration_schedule_forest.h"
#include "tensor_path.h"
#include "taco/util/strings.h"
#include "taco/util/collections.h"

using namespace std;

namespace taco {
namespace lower {

// class IterationSchedule
struct IterationSchedule::Content {
  Content(TensorBase tensor, IterationScheduleForest scheduleForest,
          TensorPath resultTensorPath, vector<TensorPath> tensorPaths,
          map<Expr,TensorPath> mapReadNodesToPaths)
      : tensor(tensor),
        scheduleForest(scheduleForest),
        resultTensorPath(resultTensorPath),
        tensorPaths(tensorPaths),
        mapReadNodesToPaths(mapReadNodesToPaths) {}
  TensorBase                             tensor;
  IterationScheduleForest                scheduleForest;
  TensorPath                             resultTensorPath;
  vector<TensorPath>                     tensorPaths;
  map<Expr,TensorPath>                   mapReadNodesToPaths;
};

IterationSchedule::IterationSchedule() {
}

IterationSchedule IterationSchedule::make(const TensorBase& tensor) {
  Expr expr = tensor.getExpr();

  vector<TensorPath> tensorPaths;

  // Create the tensor path formed by the result.
  TensorPath resultTensorPath = TensorPath(tensor, tensor.getIndexVars());

  // Create the paths formed by tensor reads in the given expression.
  struct CollectTensorPaths : public expr_nodes::ExprVisitor {
    using ExprVisitor::visit;
    vector<TensorPath> tensorPaths;
    map<Expr,TensorPath> mapReadNodesToPaths;
    void visit(const expr_nodes::ReadNode* op) {
      taco_iassert(op->tensor.getOrder() == op->indexVars.size()) <<
          "Tensor access " << Expr(op) << " but tensor format only has " <<
          op->tensor.getOrder() << " dimensions.";
      Format format = op->tensor.getFormat();

      // copy index variables to path
      vector<Var> path(op->indexVars.size());
      for (size_t i=0; i < op->indexVars.size(); ++i) {
        path[i] = op->indexVars[format.getLevels()[i].getDimension()];
      }

      auto tensorPath = TensorPath(op->tensor, path);
      mapReadNodesToPaths.insert({op, tensorPath});
      tensorPaths.push_back(tensorPath);
    }
  };
  CollectTensorPaths collect;
  expr.accept(&collect);
  util::append(tensorPaths, collect.tensorPaths);
  map<Expr,TensorPath> mapReadNodesToPaths = collect.mapReadNodesToPaths;

  // Construct a forest decomposition from the tensor path graph
  IterationScheduleForest forest =
      IterationScheduleForest(util::combine({resultTensorPath},tensorPaths));

  // Create the iteration schedule
  IterationSchedule schedule = IterationSchedule();
  schedule.content =
      make_shared<IterationSchedule::Content>(tensor,
                                              forest,
                                              resultTensorPath,
                                              tensorPaths,
                                              mapReadNodesToPaths);
  return schedule;
}

const TensorBase& IterationSchedule::getTensor() const {
  return content->tensor;
}

const std::vector<taco::Var>& IterationSchedule::getRoots() const {
  return content->scheduleForest.getRoots();
}

const taco::Var& IterationSchedule::getParent(const taco::Var& var) const {
  return content->scheduleForest.getParent(var);
}

const std::vector<taco::Var>&
IterationSchedule::getChildren(const taco::Var& var) const {
  return content->scheduleForest.getChildren(var);
}

vector<taco::Var> IterationSchedule::getAncestors(const taco::Var& var) const {
  std::vector<taco::Var> ancestors;
  ancestors.push_back(var);
  taco::Var parent = var;
  while (content->scheduleForest.hasParent(parent)) {
    parent = content->scheduleForest.getParent(parent);
    ancestors.push_back(parent);
  }
  return ancestors;
}

vector<taco::Var> IterationSchedule::getDescendants(const taco::Var& var) const{
  vector<taco::Var> descendants;
  descendants.push_back(var);
  for (auto& child : getChildren(var)) {
    util::append(descendants, getDescendants(child));
  }
  return descendants;
}

bool IterationSchedule::isLastFreeVariable(const taco::Var& var) const {
  return var.isFree() && !hasFreeVariableDescendant(var);
}

bool IterationSchedule::hasFreeVariableDescendant(const taco::Var& var) const {
  // Traverse the iteration schedule forest subtree of var to determine whether
  // it has any free variable descendants
  auto children = content->scheduleForest.getChildren(var);
  for (auto& child : children) {
    if (child.isFree()) {
      return true;
    }
    // Child is not free; check if it a free descendent
    if (hasFreeVariableDescendant(child)) {
      return true;
    }
  }
  return false;
}

bool
IterationSchedule::hasReductionVariableAncestor(const taco::Var& var) const {
  if (var.isReduction()) {
    return true;
  }

  Var parent = var;
  while (content->scheduleForest.hasParent(parent)) {
    parent = content->scheduleForest.getParent(parent);
    if (parent.isReduction()) {
      return true;
    }
  }
  return false;
}

const vector<TensorPath>& IterationSchedule::getTensorPaths() const {
  return content->tensorPaths;
}

const TensorPath&
IterationSchedule::getTensorPath(const taco::Expr& operand) const {
  taco_iassert(util::contains(content->mapReadNodesToPaths, operand));
  return content->mapReadNodesToPaths.at(operand);
}

const TensorPath& IterationSchedule::getResultTensorPath() const {
  return content->resultTensorPath;
}

std::ostream& operator<<(std::ostream& os, const IterationSchedule& schedule) {
  os << "Index Variable Forest" << std::endl;
  os << schedule.content->scheduleForest << std::endl;
  os << "Result tensor path" << std::endl;
  os << "  " << schedule.getResultTensorPath() << std::endl;
  os << "Tensor paths:" << std::endl;
  for (auto& tensorPath : schedule.getTensorPaths()) {
    os << "  " << tensorPath << std::endl;
  }
  return os;
}

}}
