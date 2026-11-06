/* Empty logging macros so TI's host driver (which includes Zephyr's log.h) builds without Zephyr. */
#ifndef FRL_LOG_SHIM_H
#define FRL_LOG_SHIM_H
#define LOG_MODULE_DECLARE(...)
#define LOG_MODULE_REGISTER(...)
#define LOG_DBG(...) ((void)0)
#define LOG_INF(...) ((void)0)
#define LOG_WRN(...) ((void)0)
#define LOG_ERR(...) ((void)0)
#endif
