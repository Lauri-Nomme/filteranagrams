#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <sys/time.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <pthread.h>

typedef char *t_partitionResult[16386 * 2];
typedef struct {
    int fd;
    int start;
    int end;
    char *charCounts;
    int needleLen;
    char **results;
} t_partition;

long duration(struct timeval *start, struct timeval *end);

void printResults(long duration, t_partitionResult *results, int partitions);

void *processPartition(t_partition *partition);

void findAnagrams(int fd, int start, int end, char needleCharCounts[256], int needleLen, char **results, bool alignStart);

#define min(l, r) ((l) < (r) ? (l) : (r))
#define RECORD_SEP '\r'
#define RECORD_SEP_LEN 2

int main(int argc, char **argv) {
    struct timeval startTime, endTime;
    gettimeofday(&startTime, NULL);

    FILE *f = fopen(argv[1], "rb");
    fseek(f, 0, SEEK_END);
    long dictSize = ftell(f);
    rewind(f);

    char *needle = argv[2];
    int needleLen = 0;
    char charCounts[256] = {};
    for (int i = 0; needle[i]; ++i) {
        ++charCounts[needle[i]];
        ++needleLen;
    }

    int partitions = sysconf(_SC_NPROCESSORS_ONLN);
    if (argc > 3)
        partitions = min(partitions, atoi(argv[3]));

    t_partitionResult results[partitions];
    pthread_t threads[partitions - 1];
    t_partition partitionSpecs[partitions - 1];

    int partitionSize = dictSize / partitions;

    for (int p = 1; p < partitions; ++p) {
        partitionSpecs[p - 1] = (t_partition) {
                .fd = f->_fileno,
                .start = p * partitionSize,
                .end = min((p + 1) * partitionSize, dictSize),
                .charCounts = charCounts,
                .needleLen = needleLen,
                .results = results[p]
        };
        pthread_create(&threads[p - 1], NULL, (void *(*)(void *)) processPartition, &partitionSpecs[p - 1]);

    }

    findAnagrams(f->_fileno, 0, partitionSize, charCounts, needleLen, results[0], false);

    for (int p = 0; p < partitions; ++p) {
        pthread_join(threads[p], NULL);
    }

    gettimeofday(&endTime, NULL);
    printResults(duration(&startTime, &endTime), results, partitions);
}

void *processPartition(t_partition *partition) {
    findAnagrams(partition->fd, partition->start, partition->end, partition->charCounts, partition->needleLen, partition->results, true);
}

void findAnagrams(int fd, int start, int end, char needleCharCounts[256], int needleLen, char **results, bool alignStart) {
    char charCounts[256];
    memcpy(charCounts, needleCharCounts, sizeof(charCounts));
    uint8_t *buf = malloc(end - start);
    uint8_t *bufStart = buf;
    uint8_t *bufEnd = &buf[end - start];

    FILE *f = fdopen(fd, "rb");
    fseek(f, start, SEEK_SET);
    fread(buf, 1, end - start, f);

    results[0] = 0;
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
                results[resultIndex] = 0;
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
            return;
        }
        bufStart = i;
        len = 0;
    }
}

void printResults(long duration, t_partitionResult *results, int partitions) {
    printf("%ld", duration);
    for (short p = 0; p < partitions; ++p) {
        for (short i = 0; results[p][i]; i += 2) {
            printf(",%.*s", *(int *) &results[p][i + 1], results[p][i]);
        }
    }
    printf("\n");
}

long duration(struct timeval *start, struct timeval *end) {
    return ((end->tv_sec - start->tv_sec) * 1000000) + (end->tv_usec - start->tv_usec);
}