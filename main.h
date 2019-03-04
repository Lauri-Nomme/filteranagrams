#ifndef FA_MAIN_H
#define FA_MAIN_H

typedef char* t_partitionResult[16386];
typedef struct {
    int fd;
    int start;
    int end;
    char* charCounts;
    char* uniqueChars;
    int uniqueCharsLen;
    int needleLen;
    char** results;
} t_partition;

#if defined(__MINGW32__)
#warning using emulated pread
#define pread(fd, buf, cnt, start) { \
    lseek(fd, start, SEEK_SET); \
    read(fd, buf, cnt); \
}
#endif

#define min(l, r) ((l) < (r) ? (l) : (r))
#define RECORD_SEP '\r'
#define RECORD_SEP_LEN 2

#if defined(_SC_NPROCESSORS_ONLN)
#define nprocs sysconf(_SC_NPROCESSORS_ONLN)
#else
#define nprocs 1
#endif

#endif //FA_MAIN_H
