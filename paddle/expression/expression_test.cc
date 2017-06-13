#include "paddle/expression/expression.h"

#include <sstream>

#include "gtest/gtest.h"

TEST(Expression, TensorAsExprValue) {
  float data[] = {0, 1, 2};
  paddle::expression::Tensor<float> a(data, 3);

  for (int i = 0; i < 3; i++) {
    EXPECT_EQ(static_cast<float>(i), a.Value().Elem(i));
  }
}

TEST(Expression, OperatorAssign) {
  using namespace paddle::expression;
  Tensor<float> dst(1);
  float data[] = {0, 1, 2};
  Tensor<float> src(data, 3);

  Assign<float, Tensor<float>> assign = (dst = src);
  EXPECT_EQ(1, assign.Value().dst_.Size());
  EXPECT_EQ(3, assign.Value().src_.Size());
}
