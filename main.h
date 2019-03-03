#ifndef FA_MAIN_H
#define FA_MAIN_H

typedef char *t_partitionResult[16386 * 2];
typedef struct {
    int fd;
    int start;
    int end;
    char *charCounts;
    char *uniqueChars;
    int uniqueCharsLen;
    int needleLen;
    char **results;
} t_partition;

#define min(l, r) ((l) < (r) ? (l) : (r))
#define RECORD_SEP '\r'
#define RECORD_SEP_LEN 2

#if defined(_SC_NPROCESSORS_ONLN)
#define nprocs sysconf(_SC_NPROCESSORS_ONLN)
#else
#define nprocs 1
#endif

#endif //FA_MAIN_H
