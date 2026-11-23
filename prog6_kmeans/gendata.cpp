// Generates ./data.dat for Program 6.
//
// The original CS149 handout reads an ~800 MB file from Stanford AFS
// (/afs/ir.stanford.edu/class/cs149/data/data.dat). This program recreates
// an equivalent file locally using the generator code that ships commented
// out in main.cpp: M = 1e6 points, N = 100 dimensions, K = 3 clusters,
// epsilon = 0.1.
//
// Usage: ./gendata [-m M] [-n N] [-k K] [-o file]
//   Defaults match the original dataset. Use a smaller -m (e.g. 200000)
//   if your machine is short on RAM or disk.

#include <getopt.h>
#include <math.h>
#include <random>
#include <stdio.h>
#include <stdlib.h>
#include <string>

#define SEED 7

using namespace std;

extern void writeData(string filename, double *data, double *clusterCentroids,
                      int *clusterAssignments, int *M_p, int *N_p, int *K_p,
                      double *epsilon_p);

static double randDouble() {
  return static_cast<double>(rand()) / static_cast<double>(RAND_MAX);
}

static double l2(const double *x, const double *y, int nDim) {
  double accum = 0.0;
  for (int i = 0; i < nDim; i++) {
    double d = x[i] - y[i];
    accum += d * d;
  }
  return sqrt(accum);
}

static void initData(double *data, int M, int N) {
  int K = 10;
  double *centers = new double[K * N];

  std::default_random_engine generator;
  std::normal_distribution<double> normal_dist(0.0, 0.5);

  for (int k = 0; k < K; k++)
    for (int n = 0; n < N; n++)
      centers[k * N + n] = randDouble();

  for (int m = 0; m < M; m++) {
    int startingPoint = rand() % K;
    for (int n = 0; n < N; n++)
      data[(size_t)m * N + n] = centers[startingPoint * N + n] + normal_dist(generator);
  }
  delete[] centers;
}

static void initCentroids(double *clusterCentroids, int K, int N) {
  for (int n = 0; n < N; n++)
    clusterCentroids[n] = randDouble();
  for (int k = 1; k < K; k++)
    for (int n = 0; n < N; n++)
      clusterCentroids[k * N + n] = clusterCentroids[n] + (randDouble() - 0.5) * 0.1;
}

int main(int argc, char **argv) {
  int M = 1000000, N = 100, K = 3;
  double epsilon = 0.1;
  string out = "./data.dat";

  int opt;
  while ((opt = getopt(argc, argv, "m:n:k:o:h")) != -1) {
    switch (opt) {
    case 'm': M = atoi(optarg); break;
    case 'n': N = atoi(optarg); break;
    case 'k': K = atoi(optarg); break;
    case 'o': out = optarg; break;
    default:
      printf("Usage: %s [-m M] [-n N] [-k K] [-o file]\n", argv[0]);
      return 1;
    }
  }
  if ((long long)M * N > 2147483647LL) {
    fprintf(stderr, "M*N must fit in a 32-bit int (main.cpp indexes with int)\n");
    return 1;
  }

  srand(SEED);
  printf("Generating M=%d N=%d K=%d (%.0f MB) -> %s\n", M, N, K,
         (double)M * N * sizeof(double) / 1e6, out.c_str());

  double *data = new double[(size_t)M * N];
  double *centroids = new double[(size_t)K * N];
  int *assign = new int[M];

  initData(data, M, N);
  initCentroids(centroids, K, N);

  for (int m = 0; m < M; m++) {
    double minDist = 1e30;
    int best = -1;
    for (int k = 0; k < K; k++) {
      double d = l2(&data[(size_t)m * N], &centroids[k * N], N);
      if (d < minDist) { minDist = d; best = k; }
    }
    assign[m] = best;
  }

  writeData(out, data, centroids, assign, &M, &N, &K, &epsilon);
  printf("Done.\n");

  delete[] data;
  delete[] centroids;
  delete[] assign;
  return 0;
}
