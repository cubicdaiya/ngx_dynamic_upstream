#ifndef NGX_DYNAMIC_UPSTREAM_H
#define NGX_DYNAMIC_UPSTREAM_H


#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>


#define NGX_DYNAMIC_UPSTEAM_OP_LIST   0
#define NGX_DYNAMIC_UPSTEAM_OP_ADD    1
#define NGX_DYNAMIC_UPSTEAM_OP_REMOVE 2
#define NGX_DYNAMIC_UPSTEAM_OP_BACKUP 4
#define NGX_DYNAMIC_UPSTEAM_OP_PARAM  8


#define NGX_DYNAMIC_UPSTEAM_OP_PARAM_WEIGHT       1
#define NGX_DYNAMIC_UPSTEAM_OP_PARAM_MAX_FAILS    2
#define NGX_DYNAMIC_UPSTEAM_OP_PARAM_FAIL_TIMEOUT 4
#define NGX_DYNAMIC_UPSTEAM_OP_PARAM_UP           8
#define NGX_DYNAMIC_UPSTEAM_OP_PARAM_DOWN         16


typedef struct ngx_dynamic_upstream_op_t {
    ngx_int_t verbose;
    ngx_int_t op;
    ngx_int_t op_param;
    ngx_int_t backup;
    ngx_int_t weight;
    ngx_int_t max_fails;
    ngx_int_t fail_timeout;
    ngx_int_t up;
    ngx_int_t down;
    ngx_str_t upstream;
    ngx_str_t server;
    ngx_uint_t status;
} ngx_dynamic_upstream_op_t;


#endif /* NGX_DYNAMIC_UPSTEAM_H */
