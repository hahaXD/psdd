//
// Created by Yujia Shen on 4/1/17.
//

#ifndef PSDD_PSDD_PARAMETER_HPP
#define PSDD_PSDD_PARAMETER_HPP

#include <cstdint>
#include <cstddef>

class PsddParameter {
 public:
  PsddParameter();
  static PsddParameter CreateFromDecimal(double num);
  static PsddParameter CreateFromLog(double num);
  std::size_t hash_value() const;
  bool operator==(const PsddParameter &other) const;
  bool operator!=(const PsddParameter &other) const;
  bool operator<(const PsddParameter& other) const;
  bool operator>(const PsddParameter& other) const;
  PsddParameter operator+(const PsddParameter &other) const;
  PsddParameter operator/(const PsddParameter &other) const;
  PsddParameter operator*(const PsddParameter &other) const;
  double parameter() const;
 private:
  explicit PsddParameter(double parameter);
  double parameter_;
  uintmax_t hash_value_;
};

typedef PsddParameter Probability;
#endif //PSDD_PSDD_PARAMETER_HPP
