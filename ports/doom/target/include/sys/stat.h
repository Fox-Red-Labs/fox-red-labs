/* bare metal: no POSIX */
struct stat { int st_mode; };
#define S_ISDIR(m) 0
static inline int stat(const char *p, struct stat *s) { (void)p; (void)s; return -1; }
static inline int mkdir(const char *p, int m) { (void)p; (void)m; return -1; }
