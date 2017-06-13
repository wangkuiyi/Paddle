#pragma once

#include <assert.h>
#include <stddef.h>

namespace paddle {
namespace expression {

template <typename Type>
struct Expr {
  // To make any type an expression, all Types are supposed to be
  // derived from Expr<Type>.  So *this is an Expr and Type.
  const Type& Value(void) const { return *static_cast<const Type*>(this); }
  Type& Value(void) { return *static_cast<Type*>(this); }
};

template <typename Type, typename ElemType>
struct ElementWiseExpr : public Expr<Type> {
  virtual size_t Size() const = 0;
  virtual const ElemType Elem(size_t index) const = 0;
  virtual ~ElementWiseExpr() {}
};

template <typename ElemType>
class Tensor : public ElementWiseExpr<Tensor<ElemType>, ElemType> {
public:
  Tensor(ElemType* data, int size) : size_(size), data_(data) {}

  virtual const ElemType Elem(size_t i) const {
    assert(i < size_);
    return data_[i];
  }

  virtual size_t Size() const { return size_; }

private:
  size_t size_;
  ElemType* data_;
};
/*

template <ExprType>
struct Evaluator {
    virtual void Eval() = 0;
};

template <>
struct Evaluator<Tensor<ElemType>> {
    void Eval() {};  // no-op with evaluate a tensor.
};

namespace element_wise {

struct mul {
    template <T>
    T operator()(T a, T b) {
        return a * b;
    }
};

struct max {
    template <T>
    T operator()(T a, T b) {
        return a > b ? a : b;
    }
};

}  // namespace functor


template<typename Functor, typename LHS, typename RHS>
struct ElementWiseBinary: public Exp<BinaryElementWise<Functor, LHS, RHS> >{
    BinaryMapExp(const LHS& lhs, const RHS& rhs) : lhs_(lhs), rhs_(rhs) {}
    const LHS& lhs_;
    const RHS& rhs_;
};

// template binary operation, works for any expressions
template<typename OP, typename LHS, typename RHS>
inline BinaryMapExp<OP, LHS, RHS>
F(const Exp<LHS>& lhs, const Exp<RHS>& rhs) {
  return BinaryMapExp<OP, LHS, RHS>(lhs.self(), rhs.self());
}

template<typename LHS, typename RHS>
inline BinaryMapExp<mul, LHS, RHS>
operator*(const Exp<LHS>& lhs, const Exp<RHS>& rhs) {
  return F<mul>(lhs, rhs);
}

// user defined operation
struct maximum{
  inline static float Map(float a, float b) {
    return a > b ? a : b;
  }
};
*/
}  // namespace expression
}  // namespace paddle
