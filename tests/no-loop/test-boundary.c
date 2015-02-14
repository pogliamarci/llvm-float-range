#include <stdio.h>

double f(double p __attribute__((float_range(-31,31)))) {
    double q = p / 2;
    return q;
}

int main()
{
    printf("%f\n", f(31));
    printf("%f\n", f(-31));
}
