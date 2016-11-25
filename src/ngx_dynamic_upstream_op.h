#ifndef NGX_DYNAMIC_UPSTREAM_OP_H
#define NGX_DYNAMIC_UPSTREAM_OP_H

#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>
#include <ngx_stream.h>


typedef union {
    ngx_http_upstream_rr_peer_t *http;
    ngx_stream_upstream_rr_peer_t *stream;
} ngx_upstream_rr_peer_t;

typedef union {
    ngx_http_upstream_rr_peers_t *http;
    ngx_stream_upstream_rr_peers_t *stream;
} ngx_upstream_rr_peers_t;


ngx_int_t ngx_dynamic_upstream_build_op(ngx_http_request_t *r, ngx_dynamic_upstream_op_t *op);
ngx_int_t ngx_dynamic_upstream_op_impl(ngx_log_t *log, ngx_dynamic_upstream_op_t *op,
                                       ngx_slab_pool_t *shpool, ngx_upstream_rr_peers_t *primary);

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
