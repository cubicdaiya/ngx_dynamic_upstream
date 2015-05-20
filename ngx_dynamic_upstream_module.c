
#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>

#include "ngx_inet_slab.h"

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
} ngx_dynamic_upstream_op_t;

static ngx_int_t ngx_dynamic_upstream_op_add(ngx_http_request_t *r, ngx_dynamic_upstream_op_t *op,
                                             ngx_slab_pool_t *shpool, ngx_http_upstream_srv_conf_t *uscf);
static ngx_int_t ngx_dynamic_upstream_op_remove(ngx_http_request_t *r, ngx_dynamic_upstream_op_t *op,
                                                ngx_slab_pool_t *shpool, ngx_http_upstream_srv_conf_t *uscf);
static ngx_int_t ngx_dynamic_upstream_op_update_param(ngx_http_request_t *r, ngx_dynamic_upstream_op_t *op,
                                                      ngx_slab_pool_t *shpool, ngx_http_upstream_srv_conf_t *uscf);
/* static ngx_int_t ngx_dynamic_upstream_op_backup(ngx_http_request_t *r, ngx_dynamic_upstream_op_t *op, ngx_http_upstream_srv_conf_t *uscf); */

static ngx_http_upstream_srv_conf_t *ngx_dynamic_upstream_get_zone(ngx_http_request_t *r, ngx_dynamic_upstream_op_t *op);
static ngx_int_t ngx_dynamic_upstream_build_op(ngx_http_request_t *r, ngx_dynamic_upstream_op_t *op);
static ngx_int_t ngx_dynamic_upstream_op(ngx_http_request_t *r, ngx_dynamic_upstream_op_t *op,
                                         ngx_slab_pool_t *shpool, ngx_http_upstream_srv_conf_t *uscf);
static void ngx_dynamic_upstream_create_response_buf(ngx_http_upstream_rr_peers_t *peers, ngx_buf_t *b, size_t size, ngx_int_t verbose);
static ngx_int_t ngx_dynamic_upstream_handler(ngx_http_request_t *r);
static char *ngx_dynamic_upstream(ngx_conf_t *cf, ngx_command_t *cmd, void *conf);

static ngx_command_t ngx_dynamic_upstream_commands[] = {
    {
        ngx_string("dynamic_upstream"),
        NGX_HTTP_LOC_CONF|NGX_CONF_NOARGS,
        ngx_dynamic_upstream,
        0,
        0,
        NULL
    },

    ngx_null_command
};

static ngx_http_module_t ngx_dynamic_upstream_module_ctx = {
    NULL,                              /* preconfiguration */
    NULL,                              /* postconfiguration */

    NULL,                              /* create main configuration */
    NULL,                              /* init main configuration */

    NULL,                              /* create server configuration */
    NULL,                              /* merge server configuration */

    NULL,                              /* create location configuration */
    NULL                               /* merge location configuration */
};

ngx_module_t ngx_dynamic_upstream_module = {
    NGX_MODULE_V1,
    &ngx_dynamic_upstream_module_ctx, /* module context */
    ngx_dynamic_upstream_commands,    /* module directives */
    NGX_HTTP_MODULE,                  /* module type */
    NULL,                             /* init master */
    NULL,                             /* init module */
    NULL,                             /* init process */
    NULL,                             /* init thread */
    NULL,                             /* exit thread */
    NULL,                             /* exit process */
    NULL,                             /* exit master */
    NGX_MODULE_V1_PADDING
};


static const ngx_str_t ngx_dynamic_upstream_params[] = {
    ngx_string("arg_upstream"),
    ngx_string("arg_verbose"),
    ngx_string("arg_add"),
    ngx_string("arg_remove"),
    ngx_string("arg_backup"),
    ngx_string("arg_server"),
    ngx_string("arg_weight"),
    ngx_string("arg_max_fails"),
    ngx_string("arg_fail_timeout"),
    ngx_string("arg_up"),
    ngx_string("arg_down")
};


static ngx_int_t
ngx_dynamic_upstream_build_op(ngx_http_request_t *r, ngx_dynamic_upstream_op_t *op)
{
    ngx_uint_t                  i;
    size_t                      args_size;
    u_char                     *low;
    ngx_uint_t                  key;
    ngx_str_t                  *args;
    ngx_http_variable_value_t  *var;

    ngx_memzero(op, sizeof(ngx_dynamic_upstream_op_t));

    /* default setting for op */
    op->op = NGX_DYNAMIC_UPSTEAM_OP_LIST;
    ngx_str_null(&op->upstream);
    op->weight       = 1;
    op->max_fails    = 1;
    op->fail_timeout = 10;

    args = (ngx_str_t *)&ngx_dynamic_upstream_params;
    args_size = sizeof(ngx_dynamic_upstream_params) / sizeof(ngx_str_t);
    for (i=0;i<args_size;i++) {
        low = ngx_pnalloc(r->pool, args[i].len);
        if (low == NULL) {
            return NGX_ERROR;
        }

        key = ngx_hash_strlow(low, args[i].data, args[i].len);
        var = ngx_http_get_variable(r, &args[i], key);

        if (!var->not_found) {
            if (ngx_strcmp("arg_upstream", args[i].data) == 0) {
                op->upstream.data = ngx_palloc(r->pool, var->len + 1);
                if (op->upstream.data == NULL) {
                    return NGX_ERROR;
                }
                ngx_cpystrn(op->upstream.data, var->data, var->len + 1);
                op->upstream.len = var->len;

            } else if (ngx_strcmp("arg_verbose", args[i].data) == 0) {
                op->verbose = 1;

            } else if (ngx_strcmp("arg_add", args[i].data) == 0) {
                op->op |= NGX_DYNAMIC_UPSTEAM_OP_ADD;

            } else if (ngx_strcmp("arg_remove", args[i].data) == 0) {
                op->op |= NGX_DYNAMIC_UPSTEAM_OP_REMOVE;

            } else if (ngx_strcmp("arg_backup", args[i].data) == 0) {
                op->backup = 1;

            } else if (ngx_strcmp("arg_server", args[i].data) == 0) {
                op->server.data = ngx_palloc(r->pool, var->len + 1);
                if (op->server.data == NULL) {
                    return NGX_ERROR;
                }
                ngx_cpystrn(op->server.data, var->data, var->len + 1);
                op->server.len = var->len;

            } else if (ngx_strcmp("arg_weight", args[i].data) == 0) {
                op->weight = ngx_atoi(var->data, var->len);
                if (op->weight == NGX_ERROR) {
                    return NGX_ERROR;
                }
                op->op |= NGX_DYNAMIC_UPSTEAM_OP_PARAM;
                op->op_param |= NGX_DYNAMIC_UPSTEAM_OP_PARAM_WEIGHT;
                op->verbose = 1;

            } else if (ngx_strcmp("arg_max_fails", args[i].data) == 0) {
                op->max_fails = ngx_atoi(var->data, var->len);
                if (op->max_fails == NGX_ERROR) {
                    return NGX_ERROR;
                }
                op->op |= NGX_DYNAMIC_UPSTEAM_OP_PARAM;
                op->op_param |= NGX_DYNAMIC_UPSTEAM_OP_PARAM_MAX_FAILS;
                op->verbose = 1;

            } else if (ngx_strcmp("arg_fail_timeout", args[i].data) == 0) {
                op->fail_timeout = ngx_atoi(var->data, var->len);
                if (op->fail_timeout == NGX_ERROR) {
                    return NGX_ERROR;
                }
                op->op |= NGX_DYNAMIC_UPSTEAM_OP_PARAM;
                op->op_param |= NGX_DYNAMIC_UPSTEAM_OP_PARAM_FAIL_TIMEOUT;
                op->verbose = 1;

            } else if (ngx_strcmp("arg_up", args[i].data) == 0) {
                op->up = 1;
                op->op |= NGX_DYNAMIC_UPSTEAM_OP_PARAM;
                op->op_param |= NGX_DYNAMIC_UPSTEAM_OP_PARAM_UP;
                op->verbose = 1;

            } else if (ngx_strcmp("arg_down", args[i].data) == 0) {
                op->down = 1;
                op->op |= NGX_DYNAMIC_UPSTEAM_OP_PARAM;
                op->op_param |= NGX_DYNAMIC_UPSTEAM_OP_PARAM_DOWN;
                op->verbose = 1;
                
            }
        }
    }

    /* can not add and remove at once */
    if ((op->op & NGX_DYNAMIC_UPSTEAM_OP_ADD) &&
        (op->op & NGX_DYNAMIC_UPSTEAM_OP_REMOVE))
    {
        return NGX_ERROR;
    }

    if (op->op & NGX_DYNAMIC_UPSTEAM_OP_ADD) {
        op->op = NGX_DYNAMIC_UPSTEAM_OP_ADD;
    } else if (op->op & NGX_DYNAMIC_UPSTEAM_OP_REMOVE) {
        op->op = NGX_DYNAMIC_UPSTEAM_OP_REMOVE;
    }

    /* can not up and down at once */
    if (op->up && op->down) {
        return NGX_ERROR;
    }

    return NGX_OK;
}


static ngx_http_upstream_srv_conf_t *
ngx_dynamic_upstream_get_zone(ngx_http_request_t *r, ngx_dynamic_upstream_op_t *op)
{
    ngx_uint_t                      i;
    ngx_http_upstream_srv_conf_t   *uscf, **uscfp;
    ngx_http_upstream_main_conf_t  *umcf;

    umcf  = ngx_http_get_module_main_conf(r, ngx_http_upstream_module);
    uscfp = umcf->upstreams.elts;

    for (i = 0; i < umcf->upstreams.nelts; i++) {
        uscf = uscfp[i];
        if (uscf->shm_zone != NULL &&
            uscf->shm_zone->shm.name.len == op->upstream.len &&
            ngx_strncmp(uscf->shm_zone->shm.name.data, op->upstream.data, op->upstream.len) == 0)
        {
            return uscf;
        }
    }

    return NULL;
}    


static ngx_int_t
ngx_dynamic_upstream_op_add(ngx_http_request_t *r, ngx_dynamic_upstream_op_t *op, ngx_slab_pool_t *shpool, ngx_http_upstream_srv_conf_t *uscf)
{
    ngx_http_upstream_rr_peer_t   *peer, *last;
    ngx_http_upstream_rr_peers_t  *peers;
    ngx_url_t                      u;

    peers = uscf->peer.data;
    for (peer = peers->peer, last = peer; peer; peer = peer->next) {
        if (op->server.len == peer->name.len && ngx_strncmp(op->server.data, peer->name.data, peer->name.len) == 0) {
            return NGX_ERROR;
        }
        last = peer;
    }

    ngx_memzero(&u, sizeof(ngx_url_t));

    u.url.data = ngx_slab_alloc_locked(shpool, op->server.len);
    if (u.url.data == NULL) {
        return NGX_ERROR;
    }
    ngx_cpystrn(u.url.data, op->server.data, op->server.len + 1);
    u.url.len      = op->server.len;
    u.default_port = 80;

    if (ngx_parse_url_slab(shpool, &u) != NGX_OK) {
        if (u.err) {
            ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,
                          "%s in upstream \"%V\"", u.err, &u.url);
        }

        return NGX_ERROR;
    }

    last->next = ngx_slab_calloc_locked(shpool, sizeof(ngx_http_upstream_rr_peer_t));
    if (last->next == NULL) {
        return NGX_ERROR;
    }
    peers->number++;

    last->next->name     = u.url;
    last->next->server   = u.url;
    last->next->sockaddr = u.addrs[0].sockaddr;
    last->next->socklen  = u.addrs[0].socklen;

    if (op->op_param & NGX_DYNAMIC_UPSTEAM_OP_PARAM_WEIGHT) {
        last->next->weight = op->weight;
    } else {
        last->next->weight = 1;
    }

    if (op->op_param & NGX_DYNAMIC_UPSTEAM_OP_PARAM_MAX_FAILS) {
        last->next->max_fails = op->max_fails;
    } else {
        last->next->max_fails = 1;
    }

    if (op->op_param & NGX_DYNAMIC_UPSTEAM_OP_PARAM_FAIL_TIMEOUT) {
        last->next->fail_timeout = op->fail_timeout;
    } else {
        last->next->fail_timeout = 10;
    }

    if (op->op_param & NGX_DYNAMIC_UPSTEAM_OP_PARAM_DOWN) {
        last->next->down = op->down;
    }

    return NGX_OK;
}


static ngx_int_t
ngx_dynamic_upstream_op_remove(ngx_http_request_t *r, ngx_dynamic_upstream_op_t *op, ngx_slab_pool_t *shpool, ngx_http_upstream_srv_conf_t *uscf)
{
    ngx_http_upstream_rr_peer_t   *peer, *target, *prev;
    ngx_http_upstream_rr_peers_t  *peers;

    peers = uscf->peer.data;

    if (peers->number < 2) {
        return NGX_ERROR;
    }

    target = NULL;
    prev = NULL;
    for (peer = peers->peer; peer ; peer = peer->next) {
        if (op->server.len == peer->name.len && ngx_strncmp(op->server.data, peer->name.data, peer->name.len) == 0) {
            target = peer;
            peer = peer->next;
            break;
        }
        prev = peer;
    }

    /* not found */
    if (target == NULL) {
        return NGX_ERROR;
    }

    /* released removed peer */
    ngx_slab_free_locked(shpool, target);

    /* found head */
    if (prev == NULL) {
        peers->peer = peer;
        goto ok;
    }

    /* found tail */
    if (peer == NULL) {
        prev->next = NULL;
        goto ok;
    }

    /* found inside */
    prev->next = peer;

 ok:
    peers->number--;
    return NGX_OK;
}


static ngx_int_t
ngx_dynamic_upstream_op_update_param(ngx_http_request_t *r, ngx_dynamic_upstream_op_t *op,
                                                      ngx_slab_pool_t *shpool, ngx_http_upstream_srv_conf_t *uscf)
{
    ngx_http_upstream_rr_peer_t   *peer, *target;
    ngx_http_upstream_rr_peers_t  *peers;

    peers = uscf->peer.data;

    target = NULL;
    for (peer = peers->peer; peer ; peer = peer->next) {
        if (op->server.len == peer->name.len && ngx_strncmp(op->server.data, peer->name.data, peer->name.len) == 0) {
            target = peer;
            break;
        }
    }

    if (target == NULL) {
        return NGX_ERROR;
    }

    if (op->op_param & NGX_DYNAMIC_UPSTEAM_OP_PARAM_WEIGHT) {
        target->weight = op->weight;
        target->current_weight = op->weight;
        target->effective_weight = op->weight;
    }

    if (op->op_param & NGX_DYNAMIC_UPSTEAM_OP_PARAM_MAX_FAILS) {
        target->max_fails = op->max_fails;
    }

    if (op->op_param & NGX_DYNAMIC_UPSTEAM_OP_PARAM_FAIL_TIMEOUT) {
        target->fail_timeout = op->fail_timeout;
    }

    if (op->op_param & NGX_DYNAMIC_UPSTEAM_OP_PARAM_UP) {
        target->down = 0;
    }

    if (op->op_param & NGX_DYNAMIC_UPSTEAM_OP_PARAM_DOWN) {
        target->down = 1;
    }

    return NGX_OK;
}


static ngx_int_t
ngx_dynamic_upstream_op(ngx_http_request_t *r, ngx_dynamic_upstream_op_t *op, ngx_slab_pool_t *shpool, ngx_http_upstream_srv_conf_t *uscf)
{
    ngx_int_t  rc;

    rc = NGX_OK;

    switch (op->op) {
    case NGX_DYNAMIC_UPSTEAM_OP_ADD:
        rc = ngx_dynamic_upstream_op_add(r, op, shpool, uscf);
        break;
    case NGX_DYNAMIC_UPSTEAM_OP_REMOVE:
        rc = ngx_dynamic_upstream_op_remove(r, op, shpool, uscf);
        break;
#if 0
    case NGX_DYNAMIC_UPSTEAM_OP_BACKUP:
        break;
#endif
    case NGX_DYNAMIC_UPSTEAM_OP_PARAM:
        rc = ngx_dynamic_upstream_op_update_param(r, op, shpool, uscf);
        break;
    case NGX_DYNAMIC_UPSTEAM_OP_LIST:
    default:
        rc = NGX_OK;
        break;
    }

    if (rc != NGX_OK) {
        return NGX_ERROR;
    }

    return rc;
}


static void
ngx_dynamic_upstream_create_response_buf(ngx_http_upstream_rr_peers_t *peers, ngx_buf_t *b, size_t size, ngx_int_t verbose)
{
    ngx_http_upstream_rr_peer_t    *peer;

    for (peer = peers->peer; peer; peer = peer->next) {
        if (verbose) {
            b->last = ngx_snprintf(b->last, size, "%s weight=%d max_fails=%d fail_timeout=%d",
                                   peer->name.data, peer->weight, peer->max_fails, peer->fail_timeout, peer->down);

        } else {
            b->last = ngx_snprintf(b->last, size, "%s", peer->name.data);

        }
        b->last = peer->down ? ngx_snprintf(b->last, size, " down;\n") : ngx_snprintf(b->last, size, ";\n");
    }
}


static ngx_int_t
ngx_dynamic_upstream_handler(ngx_http_request_t *r)
{
    size_t                          size;
    ngx_int_t                       rc;
    ngx_chain_t                     out;
    ngx_dynamic_upstream_op_t       op;
    ngx_buf_t                      *b;
    ngx_http_upstream_srv_conf_t   *uscf;
    ngx_slab_pool_t                *shpool;
    
    if (r->method != NGX_HTTP_GET && r->method != NGX_HTTP_HEAD) {
        return NGX_HTTP_NOT_ALLOWED;
    }

    rc = ngx_http_discard_request_body(r);

    if (rc != NGX_OK) {
        return rc;
    }
    
    r->headers_out.content_type_len = sizeof("text/plain") - 1;
    ngx_str_set(&r->headers_out.content_type, "text/plain");
    r->headers_out.content_type_lowcase = NULL;

    if (r->method == NGX_HTTP_HEAD) {
        r->headers_out.status = NGX_HTTP_OK;

        rc = ngx_http_send_header(r);

        if (rc == NGX_ERROR || rc > NGX_OK || r->header_only) {
            return rc;
        }
    }

    rc = ngx_dynamic_upstream_build_op(r, &op);
    if (rc != NGX_OK) {
        return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }
    
    uscf = ngx_dynamic_upstream_get_zone(r, &op);
    if (uscf == NULL) {
        return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }

    shpool = (ngx_slab_pool_t *) uscf->shm_zone->shm.addr;
    ngx_shmtx_lock(&shpool->mutex);
    
    rc = ngx_dynamic_upstream_op(r, &op, shpool, uscf);
    if (rc != NGX_OK) {
        ngx_shmtx_unlock(&shpool->mutex);
        return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }

    ngx_shmtx_unlock(&shpool->mutex);

    size = uscf->shm_zone->shm.size;

    b = ngx_create_temp_buf(r->pool, size);
    if (b == NULL) {
        return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }

    out.buf = b;
    out.next = NULL;

    ngx_dynamic_upstream_create_response_buf((ngx_http_upstream_rr_peers_t *)uscf->peer.data, b, size, op.verbose);

    r->headers_out.status = NGX_HTTP_OK;
    r->headers_out.content_length_n = b->last - b->pos;

    b->last_buf = (r == r->main) ? 1 : 0;
    b->last_in_chain = 1;

    rc = ngx_http_send_header(r);

    if (rc == NGX_ERROR || rc > NGX_OK || r->header_only) {
        return rc;
    }

    return ngx_http_output_filter(r, &out);
}


static char *
ngx_dynamic_upstream(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    ngx_http_core_loc_conf_t  *clcf;

    clcf = ngx_http_conf_get_module_loc_conf(cf, ngx_http_core_module);
    clcf->handler = ngx_dynamic_upstream_handler;

    return NGX_CONF_OK;
}
