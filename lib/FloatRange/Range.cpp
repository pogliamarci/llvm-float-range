#include "Range.h"

#include <iostream>
#include <cmath>

using namespace cto;

Range Range::Top;

Range Range::operator+(const Range &rhs) {
  if (bottom || rhs.bottom)
    return Bottom();
  if (valid && rhs.valid) {
    Range r(min, max);
    r.min += rhs.min;
    r.max += rhs.max;
    return r;
  } else {
    return Range::Top;
  }
}

Range Range::operator-(const Range &rhs) {
  if (bottom || rhs.bottom)
    return Bottom();
  if (valid && rhs.valid) {
    double a = this->min - rhs.min;
    double b = this->max - rhs.max;
    Range r(fmin(a, b), fmax(a, b));
    return r;
  } else {
    return Range::Top;
  }
}

Range Range::operator*(const Range &rhs) {
  if (bottom || rhs.bottom)
    return Bottom();
  if (valid && rhs.valid) {
    Range r(min, max);
    r.min = fmin(
              fmin(min * rhs.min, max * rhs.min),
              fmin(min * rhs.max, max * rhs.max));
    r.max = fmax(
              fmax(min * rhs.min, max * rhs.min),
              fmax(min * rhs.max, max * rhs.max));
    return r;
  } else {
    return Range::Top;
  }
}

Range Range::operator/(const Range &rhs) {
  if (bottom || rhs.bottom)
    return Bottom();
  if (valid && rhs.valid) {
    Range r(min, max);
    r.min = fmin(
              fmin(min / rhs.min, max / rhs.min),
              fmin(min / rhs.max, max / rhs.max));
    r.max = fmax(
              fmax(min / rhs.min, max / rhs.min),
              fmax(min / rhs.max, max / rhs.max));
    return r;
  } else {
    return Range::Top;
  }
}

Range Range::operator|(const Range &rhs) {
  if (bottom)
    return rhs;
  if (rhs.bottom)
    return *this;
  if (valid && rhs.valid) {
    Range r(min, max);
    r.min = fmin(min, rhs.min);
    r.max = fmax(max, rhs.max);
    return r;
  } else {
    return Range::Top;
  }
}

Range Range::operator&(const Range &rhs) {
  if (bottom || rhs.bottom)
    return Bottom();
  if (valid && rhs.valid) {
    Range r(min, max);
    if (max < rhs. min || min > rhs.max)
      return Bottom(); // intersection of overlapping ranges
    r.min = fmax(min, rhs.min);
    r.max = fmin(max, rhs.max);
    return r;
  } else {
    if (valid) return *this;
    if (rhs.valid) return rhs;
    if (bottom && rhs.bottom)
      return Bottom();
    return Range::Top;
  }
}

// Note: this implements floating point comparison with ==, so it should be
// used carefully (primarily, we use this where this problem does not matter,
// namely to understand whether two ranges are both valid \ not valid)

bool Range::operator==(const Range &rhs) const {
  if (this->valid != rhs.valid)
    return false;
  if (!this->valid && !rhs.valid)
    return true;
  if (this->bottom && rhs.bottom)
    return true;
  return this->min == rhs.min && this->max == rhs.max;
}

namespace cto {

std::ostream &operator<<(std::ostream &os, const Range &obj) {
  if (obj.valid) {
    os << '[' << obj.min << ", " << obj.max << ']';
  } else if (obj.bottom) {
    os << "[Bottom]";
  } else {
    os << "[Top]";
  }
  return os;
}

llvm::raw_ostream &operator<<(llvm::raw_ostream &os, const Range &obj) {
  if (obj.valid) {
    os << '[' << obj.min << ", " << obj.max << ']';
  } else if (obj.bottom) {
    os << "[Bottom]";
  } else {
    os << "[Top]";
  }
  return os;
}

}
