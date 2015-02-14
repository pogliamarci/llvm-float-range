#include <stdio.h>

double fun(double p1 __attribute__((float_range(-10, 20))),
        double p2 __attribute__((float_range(0, 55)))) {

    const double A = 3.25;
    const double B = 5.45;
    const double C = 2.50;
    const double D = 8.50;

    p1 = p1 + A;
    if(p2 > 5) {
        p2 += C;
        p1 += D;
        if(p2 <= 10 && p1 > 3) {
            p2 += D;
            p2 *= 0.1;
        }
    } else {
        p2 += D;
        p1 += p2;
    }

    p1 *= 4;
    return p1 + p2;
}

int main() {
    printf("%f %f %f\n", fun(20,55), fun(20, 4), fun(2, 20));
    printf("%f %f %f\n", fun(-10, -10), fun(20, 25), fun(-10,0));
}
