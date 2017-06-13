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
