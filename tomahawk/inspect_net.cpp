#include <cstdio>
#include <cstdint>
#include <cmath>
#include <algorithm>
#include <iostream>

#define INSIZE 768
#define KPSIZE 256
#define L1SIZE 256
#define L2SIZE 128
#define L3SIZE 64
#define OUTSIZE 1

int main(int argc, char** argv) {
    if (argc < 2) { printf("Usage: inspect_net <file>\n"); return 1; }

    FILE* f = fopen(argv[1], "rb");
    if (!f) { perror("Cannot open file"); return 1; }

    int16_t in_biases[KPSIZE];
    int16_t in_weights[INSIZE*KPSIZE];
    int32_t l1_biases[L2SIZE];
    int8_t l1_weights[L1SIZE*L2SIZE];
    float l2_biases[L3SIZE];
    float l2_weights[L2SIZE*L3SIZE];
    float l3_biases[OUTSIZE];
    float l3_weights[L3SIZE*OUTSIZE];

    if (fread(in_biases, sizeof(int16_t), KPSIZE, f) != KPSIZE) return 1;
    if (fread(in_weights, sizeof(int16_t), INSIZE*KPSIZE, f) != INSIZE*KPSIZE) return 1;
    if (fread(l1_biases, sizeof(int32_t), L2SIZE, f) != L2SIZE) return 1;
    if (fread(l1_weights, sizeof(int8_t), L1SIZE*L2SIZE, f) != L1SIZE*L2SIZE) return 1;
    if (fread(l2_biases, sizeof(float), L3SIZE, f) != L3SIZE) return 1;
    if (fread(l2_weights, sizeof(float), L2SIZE*L3SIZE, f) != L2SIZE*L3SIZE) return 1;
    if (fread(l3_biases, sizeof(float), OUTSIZE, f) != OUTSIZE) return 1;
    if (fread(l3_weights, sizeof(float), L3SIZE*OUTSIZE, f) != L3SIZE*OUTSIZE) return 1;

    fclose(f);

    auto print_stats = [](auto* arr, size_t n, const char* name){
        double minv = 1e30, maxv = -1e30, sum = 0;
        for (size_t i = 0; i < n; i++) {
            double v = arr[i];
            if (v < minv) minv = v;
            if (v > maxv) maxv = v;
            sum += std::abs(v);
        }
        std::cout << name << " : min=" << minv << " max=" << maxv 
                  << " avg_abs=" << sum/n << "\n";
    };

    print_stats(in_biases, KPSIZE, "in_biases");
    print_stats(in_weights, INSIZE*KPSIZE, "in_weights");
    print_stats(l1_biases, L2SIZE, "l1_biases");
    print_stats(l1_weights, L1SIZE*L2SIZE, "l1_weights");
    print_stats(l2_biases, L3SIZE, "l2_biases");
    print_stats(l2_weights, L2SIZE*L3SIZE, "l2_weights");
    print_stats(l3_biases, OUTSIZE, "l3_biases");
    print_stats(l3_weights, L3SIZE*OUTSIZE, "l3_weights");

    return 0;
}
