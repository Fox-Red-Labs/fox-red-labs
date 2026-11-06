/* Stand-in for Zephyr's logging header so TI's portable TRX host files compile without Zephyr.
 * Ours, not TI's. Logging is compiled out; arguments are still evaluated for type checks. */
#ifndef FRL_STUB_ZEPHYR_LOG_H
#define FRL_STUB_ZEPHYR_LOG_H
#define LOG_MODULE_REGISTER(...)
#define LOG_MODULE_DECLARE(...)
static inline void frl_log_sink(const char *fmt, ...) { (void)fmt; }
#define LOG_DBG(...) frl_log_sink(__VA_ARGS__)
#define LOG_INF(...) frl_log_sink(__VA_ARGS__)
#define LOG_WRN(...) frl_log_sink(__VA_ARGS__)
#define LOG_ERR(...) frl_log_sink(__VA_ARGS__)
#endif
