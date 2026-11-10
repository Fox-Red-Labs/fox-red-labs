/* TI's C library has no usable strcasecmp for C++ (its strings.h does not compile as C++); board.c supplies it. */
#ifdef __cplusplus
extern "C" {
#endif
int strcasecmp(const char *a, const char *b);
#ifdef __cplusplus
}
#endif
