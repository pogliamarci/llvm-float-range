#include <stdio.h>

double f(double p __attribute__((float_range(-3, 3)))) {
    double a = 2.0;
    for(int i = 0; i < 10; ++i) {
        p *= a;
    }
    p /= (4.0 * a);
    return p;
}

int main(int argc, char** argv) {
    printf("%f\n", f(2));
    printf("%f\n", f(0.3));
    printf("%f\n", f(-0.3));
}
