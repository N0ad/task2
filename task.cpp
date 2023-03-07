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
double arr[1024][1024];
double new_arr[1024][1024];

int main(int argc, char *argv[]) {
    max_err = atof(argv[argc - 3]);
    size_arr = stoi(argv[argc - 2]);
    max_iterations = stoi(argv[argc - 1]);
    #pragma acc enter data create(arr[0:1024][0:1024], new_arr[0:1024][0:1024])

    auto start = chrono::high_resolution_clock::now();

    arr[0][0] = 10;
    arr[0][size_arr - 1] = 20;
    arr[size_arr - 1][0] = 20;
    arr[size_arr - 1][size_arr - 1] = 30;
    new_arr[0][0] = 10;
    new_arr[0][size_arr - 1] = 20;
    new_arr[size_arr - 1][0] = 20;
    new_arr[size_arr - 1][size_arr - 1] = 30;
    double step = 10.0 / (size_arr - 1);

    #pragma acc parallel loop present(arr[0:1024][0:1024], new_arr[0:1024][0:1024])
    for (int i = 1; i < size_arr - 1; i++) {
        arr[0][i] = 10 + i * step;
        arr[size_arr - 1][i] = 20 + i * step;
        arr[i][0] = 10 + i * step;
        arr[i][size_arr - 1] = 20 + i * step;
        new_arr[0][i] = 10 + i * step;
        new_arr[size_arr - 1][i] = 20 + i * step;
        new_arr[i][0] = 10 + i * step;
        new_arr[i][size_arr - 1] = 20 + i * step;
    }
    #pragma acc parallel loop present(arr[0:1024][0:1024], new_arr[0:1024][0:1024])
    for (int i = 1; i < size_arr - 1; i++) {
        for (int j = 1; j < size_arr - 1; j++) {
            arr[i][j] = 0;
            new_arr[i][j] = 0;
        }
    }

    auto elapsed = chrono::high_resolution_clock::now() - start;
	long long msec = chrono::duration_cast<chrono::microseconds>(elapsed).count();
    cout << "initialisation: " << msec << "\n";

    int iterations = 0;
    double err = 1;
    double* swap;
    double* a = &arr[0][0];
    double* na = &new_arr[0][0];

    auto nstart = chrono::high_resolution_clock::now();
    #pragma acc enter data copyin(a[:1024][:1024], na[:1024][:1024], err)
    while ((err > max_err) && (iterations < max_iterations))
    {
        iterations++;
        #pragma acc parallel present(err)
        err = 0;
        #pragma acc parallel loop collapse(2) present(a[:1024][:1024], na[:1024][:1024], err) reduction(max:err)
        for (int i = 1; i < size_arr - 1; i++) {
            for (int j = 1; j <= i; j++) {
                *(na + i * size_arr + j) = (*(a + i * size_arr + j + 1) + *(a + i * size_arr + j - 1) + *(a + (i + 1) * size_arr + j)
                 + *(a + (i - 1) * size_arr + j)) / 4;
                 *(na + j * size_arr + i) = (*(a + j * size_arr + i + 1) + *(a + j * size_arr + i - 1) + *(a + (j + 1) * size_arr + i)
                 + *(a + (j - 1) * size_arr + i)) / 4;
                err = max(err, *(na + i * size_arr + j) - *(a + i * size_arr + j));
                err = max(err, *(na + j * size_arr + i) - *(a + j * size_arr + i));
            }
        }

        swap = a;
        a = na;
        na = swap;

        #ifdef OPENACC__
            acc_attach((void**)a);
            acc_attach((void**)a);
        #endif
        #pragma acc update self(err) wait
    }

    #pragma acc exit data delete(na[:1024][:1024]) copyout(a[:1024][:1024], err)

    auto nelapsed = chrono::high_resolution_clock::now() - nstart;
	msec = chrono::duration_cast<chrono::microseconds>(nelapsed).count();
    cout << "While: " << msec << "\n";

    cout << "Result\n";
    cout << "iterations: " << iterations << " error: " << err << "\n";

    return 0;
}