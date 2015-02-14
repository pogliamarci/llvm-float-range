#include <stdio.h>

double fun(double p __attribute__((float_range(10, 20)))) {

    double a;
    int i;
    const double K = 4.52;

    a = p - K;

    for(i = 0; i < 10; ++i) {
        a += 4;
        p *= a;
    }

    return p;

}

int main(int argc, char** argv) {
    printf("%f %f %f\n", fun(15.124), fun(17.452), fun(20.345));
}
