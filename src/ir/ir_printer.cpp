#include <sstream>
#include <iostream>

#include "ir.h"
#include "ir_printer.h"
#include "simplify.h"
#include "taco/util/strings.h"

using namespace std;

namespace taco {
namespace ir {

const std::string magenta="\033[38;5;204m";
const std::string blue="\033[38;5;67m";
const std::string green="\033[38;5;70m";
const std::string orange="\033[38;5;214m";
const std::string nc="\033[0m";

template <class T>
static inline void acceptJoin(IRPrinter* printer, ostream& stream,
                              vector<T> nodes, string sep) {
  if (nodes.size() > 0) {
    nodes[0].accept(printer);
  }
  for (size_t i=1; i < nodes.size(); ++i) {
    stream << sep;
    nodes[i].accept(printer);
  }
}

IRPrinter::IRPrinter(ostream &s) : IRPrinter(s, false, false) {
}

IRPrinter::IRPrinter(ostream &s, bool color, bool simplify)
    : stream(s), indent(0), color(color), simplify(simplify),
      omitNextParen(false) {
}

IRPrinter::~IRPrinter() {
}

void IRPrinter::print(Stmt stmt) {
  if (isa<Scope>(stmt)) {
    stmt = to<Scope>(stmt)->scopedStmt;
  }
  if (simplify) {
    stmt = ir::simplify(stmt);
  }
  stmt.accept(this);
}

void IRPrinter::visit(const Literal* op) {
  if (color) {
    stream << blue ;
  }

  switch (op->type.kind) {
    case Type::UInt:
      if (op->type.bits == 1) {
        stream << (bool)op->value;
      }
      else {
        stream << op->value;
      }
      break;
    case Type::Int:
      stream << op->value;
      break;
    case Type::Float:
      stream << (double)(op->dbl_value);
      break;
  }

  if (color) {
    stream << nc;
  }
}

void IRPrinter::visit(const Var* op) {
  stream << op->name;
}

void IRPrinter::visit(const Neg* op) {
  omitNextParen = false;
  stream << "-";
  op->a.accept(this);
}

void IRPrinter::visit(const Sqrt* op) {
  omitNextParen = false;
  stream << "sqrt(";
  op->a.accept(this);
  stream << ")";
}

void IRPrinter::visit(const Add* op) {
  printBinOp(op->a, op->b, "+");
}

void IRPrinter::visit(const Sub* op) {
  printBinOp(op->a, op->b, "-");
}

void IRPrinter::visit(const Mul* op) {
  printBinOp(op->a, op->b, "*");
}

void IRPrinter::visit(const Div* op) {
  printBinOp(op->a, op->b, "/");
}

void IRPrinter::visit(const Rem* op) {
  printBinOp(op->a, op->b, "%");
}

void IRPrinter::visit(const Min* op) {
  omitNextParen = false;
  stream << "min(";
  for (size_t i=0; i<op->operands.size(); i++) {
    op->operands[i].accept(this);
    if (i < op->operands.size()-1)
      stream << ", ";
  }
  stream << ")";
}

void IRPrinter::visit(const Max* op){
  omitNextParen = false;
  stream << "max(";
  op->a.accept(this);
  stream << ", ";
  op->b.accept(this);
  stream << ")";
}

void IRPrinter::visit(const BitAnd* op){
  printBinOp(op->a, op->b, "&");
}

void IRPrinter::visit(const Eq* op){
  printBinOp(op->a, op->b, "==");
}

void IRPrinter::visit(const Neq* op) {
  printBinOp(op->a, op->b, "!=");
}

void IRPrinter::visit(const Gt* op) {
  printBinOp(op->a, op->b, ">");
}

void IRPrinter::visit(const Lt* op) {
  printBinOp(op->a, op->b, "<");
}

void IRPrinter::visit(const Gte* op) {
  printBinOp(op->a, op->b, ">=");
}

void IRPrinter::visit(const Lte* op) {
  printBinOp(op->a, op->b, "<=");
}

void IRPrinter::visit(const And* op) {
  printBinOp(op->a, op->b, keywordString("&&"));
}

void IRPrinter::visit(const Or* op) {
  printBinOp(op->a, op->b, keywordString("||"));
}

void IRPrinter::visit(const IfThenElse* op) {
  taco_iassert(op->cond.defined());
  taco_iassert(op->then.defined());

  doIndent();
  stream << keywordString("if ");
  op->cond.accept(this);

  Stmt scopedStmt = Stmt(to<Scope>(op->then)->scopedStmt);
  if (isa<Block>(scopedStmt)) {
    stream << " {" << endl;
    op->then.accept(this);
    doIndent();
    stream << "}" << endl;
  }
  else if (isa<VarAssign>(scopedStmt)) {
    int tmp = indent;
    indent = 0;
    stream << " ";
    scopedStmt.accept(this);
    indent = tmp;
  }
  else {
    stream << endl;
    op->then.accept(this);
  }

  if (op->otherwise.defined()) {
    stream << "\n";
    doIndent();
    stream << "else {\n";

    doIndent();
    stream << "\n";
    op->otherwise.accept(this);
    doIndent();
    stream << "\n";
    doIndent();
    stream << "}";
  }
}

void IRPrinter::visit(const Case* op) {
  for (size_t i=0; i < op->clauses.size(); ++i) {
    auto clause = op->clauses[i];
    if (i != 0) stream << "\n";
    doIndent();
    if (i == 0) {
      stream << keywordString("if ");
      clause.first.accept(this);
    }
    else if (i < op->clauses.size()-1 || !op->alwaysMatch) {
      stream << keywordString("else if ");
      clause.first.accept(this);
    }
    else {
      stream << keywordString("else");
    }
    stream << " {\n";
    clause.second.accept(this);
    stream << "\n";
    doIndent();
    stream << "}";
  }
}

void IRPrinter::visit(const Load* op) {
  op->arr.accept(this);
  stream << "[";
  op->loc.accept(this);
  stream << "]";
}

void IRPrinter::visit(const Store* op) {
  doIndent();
  op->arr.accept(this);
  stream << "[";
  op->loc.accept(this);
  stream << "] = ";
  omitNextParen = true;
  op->data.accept(this);
  omitNextParen = false;
  stream << ";";
}

void IRPrinter::visit(const For* op) {
  doIndent();
  stream << keywordString("for") << " (int ";
  op->var.accept(this);
  stream << " = ";
  op->start.accept(this);
  stream << keywordString("; ");
  op->var.accept(this);
  stream << " < ";
  op->end.accept(this);
  stream << keywordString("; ");
  op->var.accept(this);

  auto literal = op->increment.as<Literal>();
  if (literal != nullptr && literal->value == 1) {
    stream << "++";
  }
  else {
    stream << " += ";
    op->increment.accept(this);
  }
  stream << ") {\n";

  op->contents.accept(this);
  stream << "\n";
  doIndent();
  stream << "}";
}

void IRPrinter::visit(const While* op) {
  doIndent();
  stream << keywordString("while ");
  op->cond.accept(this);
  stream << " {\n";

  op->contents.accept(this);
  stream << "\n";
  doIndent();
  stream << "}";
}

void IRPrinter::visit(const Block* op) {
  acceptJoin(this, stream, op->contents, "\n");
}

void IRPrinter::visit(const Scope* op) {
  indent++;
  op->scopedStmt.accept(this);
  indent--;
}

void IRPrinter::visit(const Function* op) {
  stream << keywordString("void ") << op->name;
  stream << "(";
  if (op->outputs.size() > 0) stream << "Tensor ";
  acceptJoin(this, stream, op->outputs, ", Tensor ");
  if (op->outputs.size() > 0 && op->inputs.size()) stream << ", ";
  if (op->inputs.size() > 0) stream << "Tensor ";
  acceptJoin(this, stream, op->inputs, ", Tensor ");
  stream << ") {\n";
  op->body.accept(this);
  stream << "\n";
  doIndent();
  stream << "}";
}

void IRPrinter::visit(const VarAssign* op) {
  doIndent();
  if (op->is_decl) {
    stream << keywordString(util::toString(op->lhs.type())) << " ";
  }
  op->lhs.accept(this);
  omitNextParen = true;
  bool printed = false;
  if (simplify) {
    const Add* add = op->rhs.as<Add>();
    if (add != nullptr && add->a == op->lhs) {
      const Literal* lit = add->b.as<Literal>();
      if (lit != nullptr && lit->type == Type::Int && lit->value == 1){
        stream << "++";
      }
      else {
        stream << " += ";
        add->b.accept(this);
      }
      printed = true;
    }
  }
  if (!printed) {
    stream << " = ";
    op->rhs.accept(this);
  }

  omitNextParen = false;
  stream << ";";
}

void IRPrinter::visit(const Allocate* op) {
  doIndent();
  if (op->is_realloc)
    stream << "reallocate ";
  else
    stream << "allocate ";
  op->var.accept(this);
  stream << "[ ";
  op->num_elements.accept(this);
  stream << "]";
}

void IRPrinter::visit(const Comment* op) {
  doIndent();
  stream << commentString(op->text);
}

void IRPrinter::visit(const BlankLine*) {
}

void IRPrinter::visit(const Print* op) {
  doIndent();
  stream << "printf(";
  stream << "\"" << op->fmt << "\"";
  for (auto e: op->params) {
    stream << ", ";
    e.accept(this);
  }
  stream << ");";
}

void IRPrinter::visit(const GetProperty* op) {
  op->tensor.accept(this);
  if (op->property == TensorProperty::Values) {
    stream << ".vals";
  } else {
    stream << ".d" << op->dim+1;
    if (op->property == TensorProperty::Index)
      stream << ".idx";
    if (op->property == TensorProperty::Pointer)
      stream << ".pos";
  }
}


void IRPrinter::doIndent() {
  for (int i=0; i<indent; i++)
    stream << "  ";
}

void IRPrinter::printBinOp(Expr a, Expr b, string op) {
  bool omitParen = omitNextParen;
  omitNextParen = false;

  if (!omitParen)
    stream << "(";
  a.accept(this);
  stream << " " << op << " ";
  b.accept(this);
  if (!omitParen)
    stream << ")";
}


std::string IRPrinter::keywordString(std::string keyword) {
  if (color) {
    return magenta + keyword + nc;
  }
  else {
    return keyword;
  }
}

std::string IRPrinter::commentString(std::string comment) {
  if (color) {
    return green + "/* " + comment + " */" + nc;
  }
  else {
    return "/* " + comment + " */";
  }
}

}}
