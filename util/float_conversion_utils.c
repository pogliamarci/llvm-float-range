#include <stdio.h>
#include <stdint.h>

#define EXP_BIAS_DOUBLE 1023
#define EXP_BIAS_SINGLE 127

int64_t fixpoint_convert_double(double val, uint64_t dbw) {

    double* pVal = &val;
    uint64_t repr = *((uint64_t*) pVal);

    uint64_t sign = repr >> 63;
    uint64_t exponent = (repr & 0x7FF0000000000000) >> 52;
    uint64_t fraction = (repr & 0x000FFFFFFFFFFFFF) | (exponent != 0 ? 0x0010000000000000 : 0);

    //fprintf(stderr, "Sign: %lu, exponent: %lu, fraction: %lu\n", sign, exponent, fraction);

    int64_t true_exponent = (int64_t) exponent - EXP_BIAS_DOUBLE - 52 + dbw;
    int64_t true_exponent_wom = (int64_t) exponent - EXP_BIAS_DOUBLE + dbw;
    short exp_sign = true_exponent < 0;
    if(true_exponent < 0) {
        true_exponent = (-1) * true_exponent;
        true_exponent_wom = (-1) * true_exponent_wom;
    }

    int64_t fixpoint = (sign ? (-1) : 1) * (int64_t) ( (exp_sign ? (fraction >> true_exponent) : (fraction << true_exponent)));

    //fprintf(stderr, "Converted %f to %f", val, ((double) fixpoint) / ((uint64_t) 1 << dbw));

    return fixpoint;
}

int64_t fixpoint_convert_single(float val, uint64_t dbw) {
    float* pVal = &val;
    uint32_t repr = *((uint32_t*) pVal);

    uint32_t sign = repr >> 31;
    uint32_t exponent = (repr & 0x7F800000) >> 23;
    uint32_t fraction = (repr & 0x007FFFFF) | (exponent != 0 ? 0x00800000 : 0);

    int64_t true_exponent = (int64_t) exponent - EXP_BIAS_SINGLE - 23 + dbw;

    int64_t true_exponent_wom = (int64_t) exponent - EXP_BIAS_SINGLE + dbw;
    short exp_sign = true_exponent < 0;
    if(true_exponent < 0) {
        true_exponent = (-1) * true_exponent;
        true_exponent_wom = (-1) * true_exponent_wom;
    }

    return (sign ? (-1) : (1)) * (int64_t) ( (exp_sign ? (fraction >> true_exponent) : (fraction << true_exponent)));
}
