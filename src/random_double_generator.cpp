//
// Created by Yujia Shen on 1/18/18.
//

#include <psdd/random_double_generator.h>

RandomDoubleFromGammaGenerator::RandomDoubleFromGammaGenerator(double alpha,
                                                               double beta)
    : RandomDoubleFromGammaGenerator(alpha, beta, 0) {}

double RandomDoubleFromGammaGenerator::generate() {
  return distribution_(generator_);
}

RandomDoubleFromGammaGenerator::RandomDoubleFromGammaGenerator(double alpha,
                                                               double beta,
                                                               uint seed)
    : generator_(seed), distribution_(alpha, beta) {}

RandomDoubleFromUniformGenerator::RandomDoubleFromUniformGenerator(double min,
                                                                   double max,
                                                                   uint seed)
    : min_(min), max_(max), generator_(seed), distribution_(min, max) {}

double RandomDoubleFromUniformGenerator::min() const { return min_; }

double RandomDoubleFromUniformGenerator::max() const { return max_; }

double RandomDoubleFromUniformGenerator::generate() {
  return distribution_(generator_);
}
