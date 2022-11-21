/* Random number generator (LCG algorithm). */

// LCG parameters (glibc)
#define MULTIPLIER 1103515245
#define INCREMENT 12345
#define RAND_MAX 0x7FFFFFFF // Range 0..2147483647 (2^31-1)

int next_random = 1; // Seed

void srand(int seed)
{
	next_random = seed;
}

int rand()
{
  return next_random = (next_random * MULTIPLIER + INCREMENT) & RAND_MAX;
}
