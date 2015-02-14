#ifndef CTO_RANGE_H_
#define CTO_RANGE_H_

#include <ostream>
#include "llvm/Support/raw_ostream.h"

namespace cto {

class Range {
  friend std::ostream &operator<<(std::ostream &os, const Range &obj);
  friend llvm::raw_ostream &operator<<(llvm::raw_ostream &os, const Range &obj);

public:

  Range() : valid(false), bottom(false), min(0), max(0) {
  }

  Range(double k) : valid(true), bottom(false), min(k), max(k) {
  }

  Range(double min, double max) : valid(true), bottom(false), min(min), max(max) {
    if (min > max) {
      // To avoid breaking too many things, bottom is an INVALID range
      // with min and max equal to 0.0 (thus it has the minimum possible integer bitwidth);
      // differently than Top it propagates to arithmetic operators, but not to
      // the OR operation.
      min = 0.0;
      max = 0.0;
      valid = false;
      bottom = true;
    }
  }

  Range operator+(const Range &rhs);
  Range operator-(const Range &rhs);
  Range operator*(const Range &rhs);
  Range operator/(const Range &rhs);
  Range operator|(const Range &rhs);
  Range operator&(const Range &rhs);
  bool operator==(const Range &rhs) const;
  bool operator!=(const Range &rhs) const {
    return !(*this == rhs);
  }

  inline bool isValid() {
    return valid;
  }

  inline bool isBottom() {
    return bottom;
  }

  inline double getMin() {
    return min;
  }

  inline double getMax() {
    return max;
  }

  static Range Top;
  static Range Bottom() {
    Range r;
    r.bottom = true;
    return r;
  }

private:
  bool valid;
  bool bottom;
  double min;
  double max;

};

}

#endif
