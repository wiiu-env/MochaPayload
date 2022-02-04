#ifndef __LOGGER_H_
#define __LOGGER_H_

#ifdef __cplusplus
extern "C" {
#endif

#ifdef LOG_IP
int log_init(unsigned int ip);
void log_deinit();
void log_printf(const char *format, ...);
#else
#define log_init(x)
#define log_deinit()
#define log_printf(x, ...)
#endif

#define DEBUG_FUNCTION_LINE(FMT, ARGS...)                                                  \
    do {                                                                                   \
        log_printf("[%23s]%30s@L%04d: " FMT "", __FILE__, __FUNCTION__, __LINE__, ##ARGS); \
    } while (0)


#ifdef __cplusplus
}
#endif

#endif
