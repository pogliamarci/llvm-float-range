#ifndef CTO_UTIL_H_
#define CTO_UTIL_H_

#include "llvm/Support/raw_ostream.h"
#include <cmath>

namespace cto {

template <typename T>
struct OptionalValue {

  OptionalValue<T>(T value) : valid(true), value(value) {}

  static OptionalValue invalid() {
    OptionalValue val;
    val.valid = false;
    return val;
  }

  OptionalValue operator*(const OptionalValue &other) const {
    if (valid && other.valid) {
      return OptionalValue(this->value * other.value);
    }
    return invalid();
  }

  OptionalValue operator-(const OptionalValue &other) const {
    if (valid && other.valid) {
      return OptionalValue(this->value - other.value);
    }
    return invalid();
  }

  OptionalValue operator+(const OptionalValue &other) const {
    if (valid && other.valid) {
      return OptionalValue(this->value + other.value);
    }
    return invalid();
  }

  OptionalValue operator/(const OptionalValue &other) const {
    if (valid && other.valid) {
      return OptionalValue(this->value / other.value);
    }
    return invalid();
  }

  bool operator==(const OptionalValue &other) const {
    return (!valid && !other.valid) || (valid && other.valid && value == other.value);
  }

  bool operator!=(const OptionalValue &other) const {
    return !(*this == other);
  }

  inline bool isValid() const {
    return valid;
  }

  inline T get() const {
    assert(valid && "Trying to get() a value from an invalid OptionalValue");
    return value;
  }

private:
  bool valid;
  T value;
  OptionalValue() : valid(false), value(0) {}
};

template <typename T>
OptionalValue<T> max(const OptionalValue<T> &x, const OptionalValue<T> &y) {
  if (x.isValid() && y.isValid()) {
    return x.get() > y.get() ? x : y;
  }
  return OptionalValue<T>::invalid();
}

inline OptionalValue<double> max(const OptionalValue<double> &x, const OptionalValue<double> &y) {
  if (x.isValid() && y.isValid()) {
    return OptionalValue<double>(::fmax(x.get(), y.get()));
  }
  return OptionalValue<double>::invalid();
}

inline OptionalValue<double> pow(const OptionalValue<double> &x, const double y) {
  if (x.isValid())
    return OptionalValue<double>(std::pow(x.get(), y));
  return OptionalValue<double>::invalid();
}

template <typename T>
inline std::ostream &operator<<(std::ostream &stream, const OptionalValue<T> &val) {
  const std::string invalid_str = "<Invalid>";
  if (val.isValid()) {
    return stream << val.get();
  }
  return stream << invalid_str;
}

template <typename T>
inline llvm::raw_ostream &operator<<(llvm::raw_ostream &stream, const OptionalValue<T> &val) {
  const std::string invalid_str = "<Invalid>";
  if (val.isValid()) {
    return stream << val.get();
  }
  return stream << invalid_str;
}

template<typename T>
OptionalValue<T> operator/(const T &lhs, const OptionalValue<T> &rhs) {
  if (rhs.isValid()) {
    return OptionalValue<T>(lhs / rhs.get());
  }
  return OptionalValue<T>::invalid();
}

}

#endif
