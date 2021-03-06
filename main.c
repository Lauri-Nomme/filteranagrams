#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <sys/time.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <pthread.h>
#include <emmintrin.h>
#include <smmintrin.h>
#include "main.h"

void findAnagrams(int fd, int start, int end, char needleCharCounts[256], char *uniqueChars, int uniqueCharsLen,
                  int needleLen, char **results, bool alignStart) {
    char charCounts[256];
    memcpy(charCounts, needleCharCounts, sizeof(charCounts));
    uint8_t *buf = malloc(end - start);
    uint8_t *bufStart = buf;
    uint8_t *bufEnd = &buf[end - start];

	pread(fd, buf, end - start, start);

    int resultIndex = 0;
    uint8_t *i = bufStart;
    if (alignStart)
        goto nextRecord;

    int len = 0;
    while (true) {
        uint8_t c = *i;
        if (c == RECORD_SEP) {
            if (len == needleLen) {
                results[resultIndex++] = (char *) bufStart;
                *(int *) &results[resultIndex++] = len;
            }

            goto advance;
        } else {
            if (0 > --charCounts[c]) {
                goto advance;
            } else {
                if (++len > needleLen) {
                    goto advance;
                }
                ++i;
            }
        }
        continue;
        advance:
        for (uint8_t *j = bufStart; j <= i; ++j) {
            ++charCounts[*j];
        }
        nextRecord:
        while (i < bufEnd && *i != RECORD_SEP) {
            ++i;
        }
        i += RECORD_SEP_LEN;
        if (i > bufEnd) {
            results[resultIndex] = 0;
            return;
        }
        bufStart = i;
        len = 0;
    }
}

void findAnagramsStni(int fd, int start, int end, char needleCharCounts[256], char *uniqueChars, int uniqueCharsLen,
                      int needleLen, char **results, bool alignStart) {
#ifdef DEBUG
    fprintf(stderr, "start partition@%016X-%016X\n", start, end);
#endif
    __m128i uniqueCharsB = _mm_loadu_si128((const __m128i *) uniqueChars);
    __m128i recordSepB = _mm_set1_epi8(RECORD_SEP);
    char charCounts[256];
    memcpy(charCounts, needleCharCounts, sizeof(charCounts));
    uint8_t *buf = malloc(end - start + 64);
    uint8_t *bufStart = buf;
    uint8_t *bufEnd = &buf[end - start];

	pread(fd, buf, end - start + 64, start);

    int resultIndex = 0;
    uint8_t *i = bufStart;

    if (alignStart)
        goto nextRecord;

    while (true) {
        __m128i haystackB;
        haystackB = _mm_loadu_si128((const __m128i *) i);
        int index = _mm_cmpestri(uniqueCharsB, uniqueCharsLen, haystackB, 16,
                                 _SIDD_UBYTE_OPS | _SIDD_CMP_EQUAL_ANY | _SIDD_LEAST_SIGNIFICANT |
                                 _SIDD_NEGATIVE_POLARITY);

        // @todo matching >= 16 chars
        if (i[index] == RECORD_SEP) {
            char *recordStart = i;
            int len = 0;
            do {
                char c = *i;
                if (c == RECORD_SEP) {
                    if (len == needleLen) {
#ifdef DEBUG
                        fprintf(stderr, "partition@%016X inserting result@%08X[%i] = %016X (%i)\n", start, results, resultIndex, recordStart, len);
#endif
                        results[resultIndex++] = recordStart;
                        *(int *) &results[resultIndex++] = len;
                    }
                    for (uint8_t *j = recordStart; j < i; ++j) {
                        ++charCounts[*j];
                    }
                    i += RECORD_SEP_LEN;
                    goto begin;
                } else if (0 > --charCounts[c] || ++len > needleLen) {
                    for (uint8_t *j = recordStart; j <= i; ++j) {
                        ++charCounts[*j];
                    }
                    ++i;
                    goto nextRecord;
                }
                ++i;
            } while (true);
        } else {
            do {
                index = _mm_cmpestri(recordSepB, 1, haystackB, 16,
                                     _SIDD_UBYTE_OPS | _SIDD_CMP_EQUAL_ANY | _SIDD_LEAST_SIGNIFICANT);
                if (index < 16) {
                    i += index + RECORD_SEP_LEN;
                    break;
                } else {
                    i += 16;
                    nextRecord:
                    haystackB = _mm_loadu_si128((const __m128i *) i);
                }
            } while (true);
        }
        begin:
        if (i >= bufEnd) {
            results[resultIndex] = 0;
            return;
        }
    }
}

void findAnagramsDispatch(int fd, int start, int end, char needleCharCounts[256], char *uniqueChars, int uniqueCharsLen,
                          int needleLen, char **results, bool alignStart) {
    if (uniqueCharsLen <= 16 && 0 == getenv("NO_STNI")) {
        findAnagramsStni(fd, start, end, needleCharCounts, uniqueChars, uniqueCharsLen, needleLen, results, alignStart);
    } else {
        findAnagrams(fd, start, end, needleCharCounts, uniqueChars, uniqueCharsLen, needleLen, results, alignStart);
    }
}

void *processPartition(t_partition *partition) {
    findAnagramsDispatch(partition->fd, partition->start, partition->end, partition->charCounts, partition->uniqueChars,
                         partition->uniqueCharsLen, partition->needleLen, partition->results, true);
}

void printResults(long duration, t_partitionResult *results, int partitions) {
    printf("%ld", duration);
    for (short p = 0; p < partitions; ++p) {
#ifdef DEBUG
        fprintf(stderr, "results for partition %i at %016X\n", p, results[p]);
#endif
        for (short i = 0; results[p][i]; i += 2) {
#ifdef DEBUG
            fprintf(stderr, "[%i] = %016X\n", i, results[p][i]);
#endif
            printf(",%.*s", *(int *) &results[p][i + 1], results[p][i]);
        }
    }
    printf("\n");
}

long duration(struct timeval *start, struct timeval *end) {
    return ((end->tv_sec - start->tv_sec) * 1000000) + (end->tv_usec - start->tv_usec);
}

int main(int argc, char **argv) {
    struct timeval startTime, endTime;
    gettimeofday(&startTime, NULL);

	int f = open(argv[1], O_RDONLY);
	struct stat s;
	fstat(f, &s);
    long dictSize = s.st_size; 

    char *needle = argv[2];
    int needleLen = 0;
    char charCounts[256] = {};
    char uniqueChars[256] = {};
    int uniqueCharsLen = 0;
    for (int i = 0; needle[i]; ++i) {
        if (!charCounts[needle[i]]++) {
            uniqueChars[uniqueCharsLen++] = needle[i];
        }
        ++needleLen;
    }

    int partitions = nprocs;
    if (argc > 3) {
        fprintf(stderr, "using %i partitions instead of %i\n", atoi(argv[3]), partitions);
        partitions = atoi(argv[3]);
    }

    t_partitionResult results[partitions];
    pthread_t threads[partitions - 1];
    t_partition partitionSpecs[partitions - 1];

    int partitionSize = dictSize / partitions;

    for (int p = 1; p < partitions; ++p) {
        partitionSpecs[p - 1] = (t_partition) {
                .fd = f,
                .start = p * partitionSize,
                .end = min((p + 1) * partitionSize, dictSize),
                .charCounts = charCounts,
                .uniqueChars = uniqueChars,
                .uniqueCharsLen = uniqueCharsLen,
                .needleLen = needleLen,
                .results = results[p]
        };
        pthread_create(&threads[p - 1], NULL, (void *(*)(void *)) processPartition, &partitionSpecs[p - 1]);

    }

    findAnagramsDispatch(f, 0, partitionSize, charCounts, uniqueChars, uniqueCharsLen, needleLen, results[0],
                         false);

    for (int p = 1; p < partitions; ++p) {
        pthread_join(threads[p - 1], NULL);
    }

    gettimeofday(&endTime, NULL);
    printResults(duration(&startTime, &endTime), results, partitions);
}
