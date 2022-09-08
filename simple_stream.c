#include "shmem_tests.h"
#ifdef USE_INTRINSICS
#include <immintrin.h>
#endif

double mysecond()
{
        struct timeval tp;
        struct timezone tzp;
        int i;

        i = gettimeofday(&tp,&tzp);
        return ( (double) tp.tv_sec + (double) tp.tv_usec * 1.e-6 );
}

int main(int argc, char** argv)
{

  double times[NTIMES];
  double avgtime = 0;
  double mintime = FLT_MAX;
  double maxtime = 0;  

  double *a, *b;
  posix_memalign((void *)&a, 32, BUF_SIZE * sizeof(double));
  posix_memalign((void *)&b, 32, BUF_SIZE * sizeof(double));

  printf("Array size: %d MB\n", BUF_SIZE / 1000000);
#ifdef USE_AVX      
  printf("Using omp SIMD (note: compiler might still optimize for memcpy)\n");
#elif defined USE_INTRINSICS
  #ifdef USE_STREAMING
  printf("Using intrinsics with non-temporal stores\n");
  #else
  printf("Using intrinsics\n");
  #endif
#else
  printf("Using memcpy\n");
#endif
  
  for (int i=0; i < BUF_SIZE; i++) {
    a[i] = i;
    b[i] = -1.0;
  }

  for (int k=0; k < NTIMES; k++)
    {
      times[k] = mysecond();

      // copy data
#ifdef USE_AVX      
      #pragma omp simd aligned(a, b : 32)
      for (int i=0; i < BUF_SIZE; i++)
        b[i] = a[i];
#elif defined USE_INTRINSICS
      for (int i=0; i < BUF_SIZE; i += 4) {
        __m256d src = _mm256_load_pd(&a[i]);
        #ifdef USE_STREAMING
        _mm256_stream_pd(&b[i], src);
        #else
        _mm256_store_pd(&b[i], src);
        #endif
      }
#else
      memcpy(b, a, sizeof(double) * BUF_SIZE);
#endif      
      
      times[k] = mysecond() - times[k];
    }
  
  for (int k=1; k < NTIMES; k++) /* note -- skip first iteration */
    {
      avgtime = avgtime + times[k];
      mintime = MIN(mintime, times[k]);
      maxtime = MAX(maxtime, times[k]);
    }

  size_t bytes = BW_CONVENTION * sizeof(double) * BUF_SIZE;
  
  printf("Function    Best Rate MB/s  Avg time     Min time     Max time\n");
  avgtime = avgtime / (double)(NTIMES-1);
  
  printf("%s%12.1f  %11.6f  %11.6f  %11.6f\n", "simple copy",
         1.0E-06 * bytes/mintime,
         avgtime,
         mintime,
         maxtime);

  // Use Kahan's algorithm for summation
  double checksum = b[0];
  double tmp_c = 0.0;
  for (size_t i=1; i < BUF_SIZE; i++) {
    double y = b[i] - tmp_c;
    double t = checksum + y;
    tmp_c = (t - checksum) - y;
    checksum = t;
  }
  checksum /= (double) BUF_SIZE;
  
  printf(HLINE);
  printf("check: %f %f\n", checksum, 0.5*(BUF_SIZE-1));

}
