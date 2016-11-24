#ifndef NGX_DYNAMIC_UPSTREAM_OP_H
#define NGX_DYNAMIC_UPSTREAM_OP_H

#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>

ngx_int_t ngx_dynamic_upstream_build_op(ngx_http_request_t *r, ngx_dynamic_upstream_op_t *op);
ngx_int_t ngx_dynamic_upstream_op(ngx_http_request_t *r, ngx_dynamic_upstream_op_t *op,
                                  ngx_slab_pool_t *shpool, ngx_http_upstream_srv_conf_t *uscf);


#define lock_peers(primary) { \
    ngx_http_upstream_rr_peers_wlock(primary); \
    if (primary->next) { \
        ngx_http_upstream_rr_peers_wlock(primary->next); \
    } \
}

#define unlock_peers(primary) { \
    if (primary->next && primary->next->rwlock) { \
        ngx_http_upstream_rr_peers_unlock(primary->next); \
    } \
    ngx_http_upstream_rr_peers_unlock(primary); \
}


#endif /* NGX_DYNAMIC_UPSTEAM_OP_H */
