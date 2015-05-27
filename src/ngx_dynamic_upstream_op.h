#ifndef NGX_DYNAMIC_UPSTREAM_OP_H
#define NGX_DYNAMIC_UPSTREAM_OP_H

#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>


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


ngx_int_t ngx_dynamic_upstream_build_op(ngx_http_request_t *r, ngx_dynamic_upstream_op_t *op);
ngx_int_t ngx_dynamic_upstream_op(ngx_http_request_t *r, ngx_dynamic_upstream_op_t *op,
                                  ngx_slab_pool_t *shpool, ngx_http_upstream_srv_conf_t *uscf);


#endif /* NGX_DYNAMIC_UPSTEAM_OP_H */
