//
// Created by Yujia Shen on 1/18/18.
//

#include "random_double_generator.h"

RandomDoubleFromGammaGenerator::RandomDoubleFromGammaGenerator(double alpha, double beta)
    : RandomDoubleFromGammaGenerator(alpha, beta, 0) {}


double RandomDoubleFromGammaGenerator::generate() {
  return distribution_(generator_);
}

RandomDoubleFromGammaGenerator::RandomDoubleFromGammaGenerator(double alpha, double beta, uint seed) : distribution_(
    alpha,
    beta), generator_(seed) {}

RandomDoubleFromUniformGenerator::RandomDoubleFromUniformGenerator(double min, double max, uint seed)
    : min_(min), max_(max), distribution_(min, max), generator_(seed) {}

double RandomDoubleFromUniformGenerator::min() const {
  return min_;
}

double RandomDoubleFromUniformGenerator::max() const {
  return max_;
}

double RandomDoubleFromUniformGenerator::generate() {
  return distribution_(generator_);
}
