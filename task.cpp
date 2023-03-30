#include <iostream>
#include <string>
#include <chrono>
using namespace std;

#ifdef OPENACC__
#include <openacc.h>
#endif

int max_iterations;
double max_err;
int size_arr;

int main(int argc, char *argv[]) {
    max_err = atof(argv[argc - 3]);
    size_arr = stoi(argv[argc - 2]);
    max_iterations = stoi(argv[argc - 1]);

    double *arr{new double[size_arr * size_arr]{}};
    double *new_arr{new double[size_arr * size_arr]{}};

    #pragma acc enter data create(arr[0:size_arr * size_arr], new_arr[0:size_arr * size_arr])

    auto start = chrono::high_resolution_clock::now();

    arr[0] = 10;
    arr[size_arr - 1] = 20;
    arr[(size_arr - 1) * size_arr] = 20;
    arr[size_arr * size_arr - 1] = 30;
    new_arr[0] = 10;
    new_arr[size_arr - 1] = 20;
    new_arr[(size_arr - 1) * size_arr] = 20;
    new_arr[size_arr * size_arr - 1] = 30;
    double step = 10.0 / (size_arr - 1);

    #pragma acc parallel loop present(arr[0:size_arr * size_arr], new_arr[0:size_arr * size_arr])
    for (int i = 1; i < size_arr - 1; i++) {
        arr[i] = 10 + i * step;
        arr[(size_arr - 1) * size_arr + i] = 20 + i * step;
        arr[i * size_arr] = 10 + i * step;
        arr[i * size_arr + size_arr - 1] = 20 + i * step;
        new_arr[i] = 10 + i * step;
        new_arr[(size_arr - 1) * size_arr + i] = 20 + i * step;
        new_arr[i * size_arr] = 10 + i * step;
        new_arr[i * size_arr + size_arr - 1] = 20 + i * step;
    }
    #pragma acc parallel loop present(arr[0:size_arr * size_arr], new_arr[0:size_arr * size_arr])
    for (int i = 1; i < size_arr - 1; i++) {
        for (int j = 1; j < size_arr - 1; j++) {
            arr[i * size_arr + j] = 0;
            new_arr[i * size_arr + j] = 0;
        }
    }

    auto elapsed = chrono::high_resolution_clock::now() - start;
	long long msec = chrono::duration_cast<chrono::microseconds>(elapsed).count();
    cout << "initialisation: " << msec << "\n";

    int iterations = 0;
    double err = 1;
    double* swap;
    double* a = arr;
    double* na = new_arr;

    auto nstart = chrono::high_resolution_clock::now();
    #pragma acc enter data copyin(a[0:size_arr * size_arr], na[0:size_arr * size_arr], err)
    while ((err > max_err) && (iterations < max_iterations))
    {
        iterations++;
        #pragma acc parallel present(err)
        err = 0;
        #pragma acc parallel loop collapse(1) present(a[0:size_arr * size_arr], na[0:size_arr * size_arr], err) reduction(max:err)
        for (int i = 0; i < (size_arr - 2) * (size_arr - 2); i++) {
            na[(i / (size_arr - 2) + 1) * size_arr + 1 + i % (size_arr - 2)] = (a[(i / (size_arr - 2) + 2) * size_arr + 1 + i % (size_arr - 2)] + a[(i / (size_arr - 2)) * size_arr + 1 + i % (size_arr - 2)] + a[(i / (size_arr - 2) + 1) * size_arr + 2 + i % (size_arr - 2)] + a[(i / (size_arr - 2) + 1) * size_arr + i % (size_arr - 2)]) / 4;
            na[(i % (size_arr - 2) + 1) * size_arr + 1 + i / (size_arr - 2)] = na[(i / (size_arr - 2) + 1) * size_arr + 1 + i % (size_arr - 2)];
            err = max(err, na[(i / (size_arr - 2) + 1) * size_arr + 1 + i % (size_arr - 2)] - a[(i / (size_arr - 2) + 1) * size_arr + 1 + i % (size_arr - 2)]);
                // *(na + i * size_arr + j) = (*(a + i * size_arr + j + 1) + *(a + i * size_arr + j - 1) + *(a + (i + 1) * size_arr + j)
                //  + *(a + (i - 1) * size_arr + j)) / 4;
                //  *(na + j * size_arr + i) = (*(a + j * size_arr + i + 1) + *(a + j * size_arr + i - 1) + *(a + (j + 1) * size_arr + i)
                //  + *(a + (j - 1) * size_arr + i)) / 4;
                // err = max(err, *(na + i * size_arr + j) - *(a + i * size_arr + j));
        }

        swap = a;
        a = na;
        na = swap;

        #ifdef OPENACC__
            acc_attach((void*)a);
            acc_attach((void*)na);
        #endif
        #pragma acc update self(err) wait
    }

    #pragma acc exit data delete(na[0:size_arr * size_arr]) copyout(a[0:size_arr * size_arr], err)

    auto nelapsed = chrono::high_resolution_clock::now() - nstart;
	msec = chrono::duration_cast<chrono::microseconds>(nelapsed).count();
    cout << "While: " << msec << "\n";

    cout << "Result\n";
    cout << "iterations: " << iterations << " error: " << err << "\n";

    return 0;
}