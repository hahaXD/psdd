//
// Created by Jason Shen on 4/1/17.
//

#include <cmath>
#include <limits>
#include "psdd_parameter.h"
#define APPX_LEVEL 7

PsddParameter::PsddParameter() : PsddParameter(std::log(0)) {}

PsddParameter::PsddParameter(double parameter)
    : parameter_(parameter), hash_value_(static_cast<uintmax_t >(parameter * std::pow(10, APPX_LEVEL))) {}

PsddParameter PsddParameter::CreateFromDecimal(double num) {
  return PsddParameter(std::log(num));
}

PsddParameter PsddParameter::CreateFromLog(double num) {
  return PsddParameter(num);
}

PsddParameter PsddParameter::operator+(const PsddParameter &other) const {
  if (parameter_ == -std::numeric_limits<double>::infinity()) {
    // if this is zero
    return other;
  } else if (other.parameter_ == -std::numeric_limits<double>::infinity()) {
    return PsddParameter::CreateFromLog(parameter_);
  } else {
    if (parameter_ > other.parameter_) {
      return PsddParameter(parameter_ + std::log1p(std::exp(other.parameter_ - parameter_)));
    } else {
      return PsddParameter(other.parameter_ + std::log1p(std::exp(parameter_ - other.parameter_)));
    }
  }
}

PsddParameter PsddParameter::operator/(const PsddParameter &other) const {
  return PsddParameter(parameter_ - other.parameter_);
}

PsddParameter PsddParameter::operator*(const PsddParameter &other) const {
  return PsddParameter(parameter_ + other.parameter_);
}

double PsddParameter::parameter() const {
  return parameter_;
}

std::size_t PsddParameter::hash_value() const {
  return static_cast<std::size_t>(hash_value_);
}

bool PsddParameter::operator==(const PsddParameter &other) const {
  return hash_value_ == other.hash_value_;
}

bool PsddParameter::operator!=(const PsddParameter &other) const {
  return hash_value_ != other.hash_value_;
}
bool PsddParameter::operator<(const PsddParameter &other) const {
  if (*this == other){
    return false;
  }else{
    return parameter_ < other.parameter();
  }
}
bool PsddParameter::operator>(const PsddParameter &other) const {
  if (*this == other){
    return false;
  }else{
    return parameter_ > other.parameter();
  }
}



