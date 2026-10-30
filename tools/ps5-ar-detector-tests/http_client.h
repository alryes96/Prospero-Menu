#ifndef PS5_AR_HTTP_CLIENT_H
#define PS5_AR_HTTP_CLIENT_H
#include <stddef.h>
int shadowmount_get_kernel_log(char *json_out, size_t json_cap);
int shadowmount_get_games(char *json_out, size_t json_cap);
#endif
