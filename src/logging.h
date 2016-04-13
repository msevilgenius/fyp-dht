#ifndef LOGGING_H
#define LOGGING_H

typedef enum {ERROR, WARN, INFO, OTHER} loglevel_t;

#define log_err(M, ...) printf("[ERROR] (%s:%d) " M "\n", __FILE__, __LINE__, ##__VA_ARGS__)

#define log_warn(M, ...) printf("[WARN] (%s:%d) " M "\n", __FILE__, __LINE__, ##__VA_ARGS__)

#ifdef NDEBUG
#define log_info(M, ...)
#else
#define log_info(M, ...) printf("[INFO] (%s:%d) " M "\n", __FILE__, __LINE__, ##__VA_ARGS__)
#endif // DEBUG

#endif // LOGGING_H
