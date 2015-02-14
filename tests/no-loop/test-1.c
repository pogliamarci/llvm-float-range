#include <stdio.h>

double f(double param __attribute__((float_range(-10, 10))))
{
    double j = param + 15.75;
    double h = param + 200.98;
    double k = h / 500;
    return (k * 20) * param + 0.5;
}

int main(int argc, char** argv)
{
    printf("%f   %f   %f\n", f(4), f(8), f(10));
    printf("%f   %f   %f\n", f(-4), f(-8), f(-10));
    printf("%f (this should overflow)\n", f(200));
}
