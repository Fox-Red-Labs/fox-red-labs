#ifndef COMMISSIONING_H
#define COMMISSIONING_H
void commissioning_init(void);
void commissioning_stage(const char *stage);
void commissioning_loop(void) __attribute__((noreturn));
#endif
