
static unsigned int seed = 1;

void rand_seed(unsigned int s) {
    seed = s;
}

unsigned int rand_next() {
    seed = ((1103515245 * seed) + 12345) % 2147483648;

    return seed;
}