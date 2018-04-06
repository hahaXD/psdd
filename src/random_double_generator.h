//
// Created by Yujia Shen on 1/18/18.
//

#ifndef STRUCTURED_BAYESIAN_NETWORK_RANDOM_DOUBLE_GENERATOR_H
#define STRUCTURED_BAYESIAN_NETWORK_RANDOM_DOUBLE_GENERATOR_H
#include <random>

class RandomDoubleGenerator {
 public:
  virtual ~RandomDoubleGenerator() = default;
  virtual double generate() =0;
};

class RandomDoubleFromGammaGenerator: public RandomDoubleGenerator{
 public:
  RandomDoubleFromGammaGenerator(double alpha, double beta, uint seed);
  RandomDoubleFromGammaGenerator(double alpha, double beta);
  double  generate() override;
  ~RandomDoubleFromGammaGenerator() override = default;
 private:
  std::default_random_engine generator_;
  std::gamma_distribution<double> distribution_;
};

class RandomDoubleFromUniformGenerator: public RandomDoubleGenerator{
 public:
  RandomDoubleFromUniformGenerator(double min, double max, uint seed);
  double min() const;
  double max() const;
  double generate() override;
  ~RandomDoubleFromUniformGenerator() override = default;
 private:
  double min_;
  double max_;
  std::default_random_engine generator_;
  std::uniform_real_distribution<double> distribution_;
};

#endif //STRUCTURED_BAYESIAN_NETWORK_RANDOM_DOUBLE_GENERATOR_H
