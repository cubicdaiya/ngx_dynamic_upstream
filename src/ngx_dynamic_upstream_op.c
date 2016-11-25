
#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>


#include <assert.h>


#include "ngx_dynamic_upstream_module.h"
#include "ngx_dynamic_upstream_op.h"
#include "ngx_inet_slab.h"


static const ngx_str_t ngx_dynamic_upstream_params[] = {
    ngx_string("arg_upstream"),
    ngx_string("arg_verbose"),
    ngx_string("arg_add"),
    ngx_string("arg_remove"),
    ngx_string("arg_backup"),
    ngx_string("arg_server"),
    ngx_string("arg_weight"),
    ngx_string("arg_max_fails"),
    ngx_string("arg_max_conns"),
    ngx_string("arg_fail_timeout"),
    ngx_string("arg_up"),
    ngx_string("arg_down"),
    ngx_string("arg_stream")
};


static ngx_int_t
ngx_dynamic_upstream_is_shpool_range(ngx_slab_pool_t *shpool, void *p);
static ngx_int_t
ngx_dynamic_upstream_http_op_add(ngx_log_t *log, ngx_dynamic_upstream_op_t *op,
                                 ngx_slab_pool_t *shpool, ngx_http_upstream_rr_peers_t *primary);
static ngx_int_t
ngx_dynamic_upstream_stream_op_add(ngx_log_t *log, ngx_dynamic_upstream_op_t *op,
                                   ngx_slab_pool_t *shpool, ngx_stream_upstream_rr_peers_t *primary);
static ngx_int_t
ngx_dynamic_upstream_http_op_remove(ngx_log_t *log, ngx_dynamic_upstream_op_t *op,
                                    ngx_slab_pool_t *shpool, ngx_http_upstream_rr_peers_t *primary);
static ngx_int_t
ngx_dynamic_upstream_stream_op_remove(ngx_log_t *log, ngx_dynamic_upstream_op_t *op,
                                      ngx_slab_pool_t *shpool, ngx_stream_upstream_rr_peers_t *primary);
static ngx_int_t
ngx_dynamic_upstream_http_op_update_param(ngx_log_t *log, ngx_dynamic_upstream_op_t *op,
                                          ngx_slab_pool_t *shpool, ngx_http_upstream_rr_peers_t *primary);
static ngx_int_t
ngx_dynamic_upstream_stream_op_update_param(ngx_log_t *log, ngx_dynamic_upstream_op_t *op,
                                            ngx_slab_pool_t *shpool, ngx_stream_upstream_rr_peers_t *primary);


static ngx_int_t
ngx_dynamic_upstream_is_shpool_range(ngx_slab_pool_t *shpool, void *p)
{
    if ((u_char *) p < shpool->start || (u_char *) p > shpool->end) {
        return 0;
    }

    return 1;
}


ngx_int_t
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
    op->status = NGX_HTTP_OK;
    ngx_str_null(&op->upstream);
    op->weight       = 1;
    op->max_fails    = 1;
    op->fail_timeout = 10;

    args = (ngx_str_t *)&ngx_dynamic_upstream_params;
    args_size = sizeof(ngx_dynamic_upstream_params) / sizeof(ngx_str_t);
    for (i=0;i<args_size;i++) {
        low = ngx_pnalloc(r->pool, args[i].len);
        if (low == NULL) {
            op->status = NGX_HTTP_INTERNAL_SERVER_ERROR;
            ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,
                          "failed to allocate memory from r->pool %s:%d",
                          __FUNCTION__,
                          __LINE__);
            return NGX_ERROR;
        }

        key = ngx_hash_strlow(low, args[i].data, args[i].len);
        var = ngx_http_get_variable(r, &args[i], key);

        if (!var->not_found) {
            if (ngx_strcmp("arg_upstream", args[i].data) == 0) {
                op->upstream.data = var->data;
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
                op->server.data = var->data;
                op->server.len = var->len;

            } else if (ngx_strcmp("arg_weight", args[i].data) == 0) {
                op->weight = ngx_atoi(var->data, var->len);
                if (op->weight == NGX_ERROR) {
                    op->status = NGX_HTTP_BAD_REQUEST;
                    ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,
                                  "weight is not number. %s:%d",
                                  __FUNCTION__,
                                  __LINE__);
                    return NGX_ERROR;
                }
                op->op |= NGX_DYNAMIC_UPSTEAM_OP_PARAM;
                op->op_param |= NGX_DYNAMIC_UPSTEAM_OP_PARAM_WEIGHT;
                op->verbose = 1;

            } else if (ngx_strcmp("arg_max_fails", args[i].data) == 0) {
                op->max_fails = ngx_atoi(var->data, var->len);
                if (op->max_fails == NGX_ERROR) {
                    op->status = NGX_HTTP_BAD_REQUEST;
                    ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,
                                  "max_fails is not number. %s:%d",
                                  __FUNCTION__,
                                  __LINE__);
                    return NGX_ERROR;
                }
                op->op |= NGX_DYNAMIC_UPSTEAM_OP_PARAM;
                op->op_param |= NGX_DYNAMIC_UPSTEAM_OP_PARAM_MAX_FAILS;
                op->verbose = 1;

            } else if (ngx_strcmp("arg_max_conns", args[i].data) == 0) {
                op->max_conns = ngx_atoi(var->data, var->len);
                if (op->max_conns == NGX_ERROR) {
                    op->status = NGX_HTTP_BAD_REQUEST;
                    ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,
                                  "max_conns is not number. %s:%d",
                                  __FUNCTION__,
                                  __LINE__);
                    return NGX_ERROR;
                }
                op->op |= NGX_DYNAMIC_UPSTEAM_OP_PARAM;
                op->op_param |= NGX_DYNAMIC_UPSTEAM_OP_PARAM_MAX_CONNS;
                op->verbose = 1;

            } else if (ngx_strcmp("arg_fail_timeout", args[i].data) == 0) {
                op->fail_timeout = ngx_atoi(var->data, var->len);
                if (op->fail_timeout == NGX_ERROR) {
                    op->status = NGX_HTTP_BAD_REQUEST;
                    ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,
                                  "fail_timeout is not number. %s:%d",
                                  __FUNCTION__,
                                  __LINE__);
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

            } else if (ngx_strcmp("arg_stream", args[i].data) == 0) {
                op->op_param |= NGX_DYNAMIC_UPSTEAM_OP_PARAM_STREAM;

            }
        }
    }

    /* can not add and remove at once */
    if ((op->op & NGX_DYNAMIC_UPSTEAM_OP_ADD) &&
        (op->op & NGX_DYNAMIC_UPSTEAM_OP_REMOVE))
    {
        op->status = NGX_HTTP_BAD_REQUEST;
        ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,
                      "add and remove at once are not allowed. %s:%d",
                      __FUNCTION__,
                      __LINE__);
        return NGX_ERROR;
    }

    if (op->op & NGX_DYNAMIC_UPSTEAM_OP_ADD) {
        op->op = NGX_DYNAMIC_UPSTEAM_OP_ADD;
    } else if (op->op & NGX_DYNAMIC_UPSTEAM_OP_REMOVE) {
        op->op = NGX_DYNAMIC_UPSTEAM_OP_REMOVE;
    }

    /* can not up and down at once */
    if (op->up && op->down) {
        op->status = NGX_HTTP_BAD_REQUEST;
        ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,
                      "down and up at once are not allowed. %s:%d",
                      __FUNCTION__,
                      __LINE__);
        return NGX_ERROR;
    }

    return NGX_OK;
}


ngx_int_t
ngx_dynamic_upstream_op_impl(ngx_log_t *log, ngx_dynamic_upstream_op_t *op,
                             ngx_slab_pool_t *shpool, ngx_upstream_rr_peers_t *primary)
{
    ngx_int_t  rc;

    rc = NGX_OK;

    ngx_shmtx_lock(&shpool->mutex);

    switch (op->op) {
    case NGX_DYNAMIC_UPSTEAM_OP_ADD:
        rc = op->op_param & NGX_DYNAMIC_UPSTEAM_OP_PARAM_STREAM ? ngx_dynamic_upstream_stream_op_add(log, op, shpool, primary->stream)
                                                                : ngx_dynamic_upstream_http_op_add(log, op, shpool, primary->http);
        break;
    case NGX_DYNAMIC_UPSTEAM_OP_REMOVE:
        rc = op->op_param & NGX_DYNAMIC_UPSTEAM_OP_PARAM_STREAM ? ngx_dynamic_upstream_stream_op_remove(log, op, shpool, primary->stream)
                                                                : ngx_dynamic_upstream_http_op_remove(log, op, shpool, primary->http);
        break;
    case NGX_DYNAMIC_UPSTEAM_OP_PARAM:
        rc = op->op_param & NGX_DYNAMIC_UPSTEAM_OP_PARAM_STREAM ? ngx_dynamic_upstream_stream_op_update_param(log, op, shpool, primary->stream)
                                                                : ngx_dynamic_upstream_http_op_update_param(log, op, shpool, primary->http);
        break;
    case NGX_DYNAMIC_UPSTEAM_OP_LIST:
    default:
        rc = NGX_OK;
        break;
    }

    ngx_shmtx_unlock(&shpool->mutex);

    return rc;
}


static ngx_int_t
ngx_dynamic_upstream_http_op_add(ngx_log_t *log, ngx_dynamic_upstream_op_t *op,
                                 ngx_slab_pool_t *shpool, ngx_http_upstream_rr_peers_t *primary)
{
    ngx_http_upstream_rr_peer_t   *peer, *last = NULL, *new_peer;
    ngx_http_upstream_rr_peers_t  *peers, *backup;
    ngx_url_t                      u;

    lock_peers(primary);

    backup = primary->next;

    for (peers = primary; peers; peers = peers->next) {
        for (peer = peers->peer; peer; peer = peer->next) {
            if (op->server.len == peer->name.len && ngx_strncmp(op->server.data, peer->name.data, peer->name.len) == 0) {
                op->status = NGX_HTTP_BAD_REQUEST;
                ngx_log_error(NGX_LOG_ERR, log, 0,
                              "server %V already exists in upstream. %s:%d",
                              &op->server,
                              __FUNCTION__,
                              __LINE__);
                unlock_peers(primary);
                return NGX_ERROR;
            }
            if ( (op->backup == 0 && peers == primary) || (op->backup == 1 && peers == backup) ) {
                last = peer;
            }
        }
    }

    if (op->backup) {
        if (backup == NULL) {
            assert(last == NULL);
            backup = ngx_slab_calloc_locked(shpool, sizeof(ngx_http_upstream_rr_peers_t));
            if (backup == NULL) {
                op->status = NGX_HTTP_INTERNAL_SERVER_ERROR;
                ngx_log_error(NGX_LOG_ERR, log, 0,
                              "failed to allocate memory from slab %s:%d",
                              __FUNCTION__,
                              __LINE__);
                unlock_peers(primary);
                return NGX_ERROR;
            }
            backup->shpool = primary->shpool;
            backup->name = primary->name;
        }

        peers = backup;
    } else {
        peers = primary;
    }

    ngx_memzero(&u, sizeof(ngx_url_t));

    u.url.data = ngx_slab_alloc_locked(shpool, op->server.len);
    if (u.url.data == NULL) {
        if (backup && primary->next == NULL) {
            ngx_slab_free_locked(shpool, backup);
        }
        op->status = NGX_HTTP_INTERNAL_SERVER_ERROR;
        ngx_log_error(NGX_LOG_ERR, log, 0,
                      "failed to allocate memory from slab %s:%d",
                      __FUNCTION__,
                      __LINE__);
        unlock_peers(primary);
        return NGX_ERROR;
    }
    ngx_cpystrn(u.url.data, op->server.data, op->server.len + 1);
    u.url.len      = op->server.len;
    u.default_port = 80;

    if (ngx_parse_url_slab(shpool, &u) != NGX_OK) {
        if (u.err) {
            ngx_log_error(NGX_LOG_ERR, log, 0,
                          "%s in upstream \"%V\"", u.err, &u.url);
        }
        if (backup && primary->next == NULL) {
            ngx_slab_free_locked(shpool, backup);
        }
        op->status = NGX_HTTP_INTERNAL_SERVER_ERROR;
        unlock_peers(primary);
        return NGX_ERROR;
    }

    new_peer = ngx_slab_calloc_locked(shpool, sizeof(ngx_http_upstream_rr_peer_t));
    if (new_peer == NULL) {
        if (backup && primary->next == NULL) {
            ngx_slab_free_locked(shpool, backup);
        }
        op->status = NGX_HTTP_INTERNAL_SERVER_ERROR;
        ngx_log_error(NGX_LOG_ERR, log, 0,
                      "failed to allocate memory from slab %s:%d",
                      __FUNCTION__,
                      __LINE__);
        unlock_peers(primary);
        return NGX_ERROR;
    }

    new_peer->name     = u.url;
    new_peer->server   = u.url;
    new_peer->sockaddr = u.addrs[0].sockaddr;
    new_peer->socklen  = u.addrs[0].socklen;

    if (op->op_param & NGX_DYNAMIC_UPSTEAM_OP_PARAM_WEIGHT) {
        new_peer->weight = op->weight;
        new_peer->effective_weight = op->weight;
        new_peer->current_weight = 0;
    } else {
        new_peer->weight = 1;
        new_peer->effective_weight = 1;
        new_peer->current_weight = 0;
    }

    if (op->op_param & NGX_DYNAMIC_UPSTEAM_OP_PARAM_MAX_FAILS) {
        new_peer->max_fails = op->max_fails;
    } else {
        new_peer->max_fails = 1;
    }

    if (op->op_param & NGX_DYNAMIC_UPSTEAM_OP_PARAM_FAIL_TIMEOUT) {
        new_peer->fail_timeout = op->fail_timeout;
    } else {
        new_peer->fail_timeout = 10;
    }

    if (op->op_param & NGX_DYNAMIC_UPSTEAM_OP_PARAM_DOWN) {
        new_peer->down = op->down;
    }

    if (last == NULL) {
        peers->peer = new_peer;
    } else {
        last->next = new_peer;
    }

    peers->total_weight += new_peer->weight;
    peers->single = (peers->number == 0);
    peers->weighted = (peers->total_weight != peers->number);
    peers->number++;

    if (backup && primary->next == NULL) {
        primary->next = backup;
    }

    unlock_peers(primary);

    ngx_log_error(NGX_LOG_NOTICE, log, 0,
                  "added server %V", &op->server);

    return NGX_OK;
}


static ngx_int_t
ngx_dynamic_upstream_stream_op_add(ngx_log_t *log, ngx_dynamic_upstream_op_t *op,
                                   ngx_slab_pool_t *shpool, ngx_stream_upstream_rr_peers_t *primary)
{
    ngx_stream_upstream_rr_peer_t   *peer, *last = NULL, *new_peer;
    ngx_stream_upstream_rr_peers_t  *peers, *backup;
    ngx_url_t                        u;

    lock_peers(primary);

    backup = primary->next;

    for (peers = primary; peers; peers = peers->next) {
        for (peer = peers->peer; peer; peer = peer->next) {
            if (op->server.len == peer->name.len && ngx_strncmp(op->server.data, peer->name.data, peer->name.len) == 0) {
                op->status = NGX_HTTP_BAD_REQUEST;
                ngx_log_error(NGX_LOG_ERR, log, 0,
                              "server %V already exists in upstream. %s:%d",
                              &op->server,
                              __FUNCTION__,
                              __LINE__);
                unlock_peers(primary);
                return NGX_ERROR;
            }
            if ( (op->backup == 0 && peers == primary) || (op->backup == 1 && peers == backup) ) {
                last = peer;
            }
        }
    }

    if (op->backup) {
        if (backup == NULL) {
            assert(last == NULL);
            backup = ngx_slab_calloc_locked(shpool, sizeof(ngx_stream_upstream_rr_peers_t));
            if (backup == NULL) {
                op->status = NGX_HTTP_INTERNAL_SERVER_ERROR;
                ngx_log_error(NGX_LOG_ERR, log, 0,
                              "failed to allocate memory from slab %s:%d",
                              __FUNCTION__,
                              __LINE__);
                unlock_peers(primary);
                return NGX_ERROR;
            }
            backup->shpool = primary->shpool;
            backup->name = primary->name;
        }

        peers = backup;
    } else {
        peers = primary;
    }

    ngx_memzero(&u, sizeof(ngx_url_t));

    u.url.data = ngx_slab_alloc_locked(shpool, op->server.len);
    if (u.url.data == NULL) {
        if (backup && primary->next == NULL) {
            ngx_slab_free_locked(shpool, backup);
        }
        op->status = NGX_HTTP_INTERNAL_SERVER_ERROR;
        ngx_log_error(NGX_LOG_ERR, log, 0,
                      "failed to allocate memory from slab %s:%d",
                      __FUNCTION__,
                      __LINE__);
        unlock_peers(primary);
        return NGX_ERROR;
    }
    ngx_cpystrn(u.url.data, op->server.data, op->server.len + 1);
    u.url.len      = op->server.len;
    u.default_port = 80;

    if (ngx_parse_url_slab(shpool, &u) != NGX_OK) {
        if (u.err) {
            ngx_log_error(NGX_LOG_ERR, log, 0,
                          "%s in upstream \"%V\"", u.err, &u.url);
        }
        if (backup && primary->next == NULL) {
            ngx_slab_free_locked(shpool, backup);
        }
        op->status = NGX_HTTP_INTERNAL_SERVER_ERROR;
        unlock_peers(primary);
        return NGX_ERROR;
    }

    new_peer = ngx_slab_calloc_locked(shpool, sizeof(ngx_stream_upstream_rr_peer_t));
    if (new_peer == NULL) {
        if (backup && primary->next == NULL) {
            ngx_slab_free_locked(shpool, backup);
        }
        op->status = NGX_HTTP_INTERNAL_SERVER_ERROR;
        ngx_log_error(NGX_LOG_ERR, log, 0,
                      "failed to allocate memory from slab %s:%d",
                      __FUNCTION__,
                      __LINE__);
        unlock_peers(primary);
        return NGX_ERROR;
    }

    new_peer->name     = u.url;
    new_peer->server   = u.url;
    new_peer->sockaddr = u.addrs[0].sockaddr;
    new_peer->socklen  = u.addrs[0].socklen;

    if (op->op_param & NGX_DYNAMIC_UPSTEAM_OP_PARAM_WEIGHT) {
        new_peer->weight = op->weight;
        new_peer->effective_weight = op->weight;
        new_peer->current_weight = 0;
    } else {
        new_peer->weight = 1;
        new_peer->effective_weight = 1;
        new_peer->current_weight = 0;
    }

    if (op->op_param & NGX_DYNAMIC_UPSTEAM_OP_PARAM_MAX_FAILS) {
        new_peer->max_fails = op->max_fails;
    } else {
        new_peer->max_fails = 1;
    }

    if (op->op_param & NGX_DYNAMIC_UPSTEAM_OP_PARAM_FAIL_TIMEOUT) {
        new_peer->fail_timeout = op->fail_timeout;
    } else {
        new_peer->fail_timeout = 10;
    }

    if (op->op_param & NGX_DYNAMIC_UPSTEAM_OP_PARAM_DOWN) {
        new_peer->down = op->down;
    }

    if (last == NULL) {
        peers->peer = new_peer;
    } else {
        last->next = new_peer;
    }

    peers->total_weight += new_peer->weight;
    peers->single = (peers->number == 0);
    peers->weighted = (peers->total_weight != peers->number);
    peers->number++;

    if (backup && primary->next == NULL) {
        primary->next = backup;
    }

    unlock_peers(primary);

    ngx_log_error(NGX_LOG_NOTICE, log, 0,
                  "added server %V", &op->server);

    return NGX_OK;
}


static ngx_int_t
ngx_dynamic_upstream_http_op_remove(ngx_log_t *log, ngx_dynamic_upstream_op_t *op,
                                    ngx_slab_pool_t *shpool, ngx_http_upstream_rr_peers_t *primary)
{
    ngx_http_upstream_rr_peer_t   *peer, *target, *prev;
    ngx_http_upstream_rr_peers_t  *peers, *backup;
    ngx_uint_t                     weight;

    lock_peers(primary);

    backup = primary->next;

    target = NULL;
    for (peers = primary; peers; peers = peers->next) {
       prev = NULL;
       for (peer = peers->peer; peer; peer = peer->next) {
           if (op->server.len == peer->name.len && ngx_strncmp(op->server.data, peer->name.data, peer->name.len) == 0) {
               if (peers == primary && peers->number == 1) {
                  op->status = NGX_HTTP_BAD_REQUEST;
                  unlock_peers(primary);
                  return NGX_ERROR;
               }
               target = peer;
               peer = peer->next;
               goto c;
            }
            prev = peer;
        }
    }

c:

    /* not found */
    if (target == NULL) {
        op->status = NGX_HTTP_BAD_REQUEST;
        ngx_log_error(NGX_LOG_ERR, log, 0,
                      "server %V is not found. %s:%d",
                      &op->server,
                      __FUNCTION__,
                      __LINE__);
        unlock_peers(primary);
        return NGX_ERROR;
    }
    weight = target->weight;
    /* released removed peer and attributes */
    if (ngx_dynamic_upstream_is_shpool_range(shpool, target->name.data)) {
        ngx_slab_free_locked(shpool, target->name.data);
    }

    if (ngx_dynamic_upstream_is_shpool_range(shpool, target->sockaddr)) {
        ngx_slab_free_locked(shpool, target->sockaddr);
    }

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
    peers->total_weight -= weight;
    peers->single = (peers->number == 1);
    peers->weighted = (peers->total_weight != peers->number);

    if (peers->number == 0) {
        assert(peers == backup);
        primary->next = NULL;
        ngx_slab_free_locked(shpool, backup);
    }

    unlock_peers(primary);

    ngx_log_error(NGX_LOG_NOTICE, log, 0,
                  "removed server %V", &op->server);

    return NGX_OK;
}


static ngx_int_t
ngx_dynamic_upstream_stream_op_remove(ngx_log_t *log, ngx_dynamic_upstream_op_t *op,
                                      ngx_slab_pool_t *shpool, ngx_stream_upstream_rr_peers_t *primary)
{
    ngx_stream_upstream_rr_peer_t   *peer, *target, *prev;
    ngx_stream_upstream_rr_peers_t  *peers, *backup;
    ngx_uint_t                       weight;

    lock_peers(primary);

    backup = primary->next;

    target = NULL;
    for (peers = primary; peers; peers = peers->next) {
       prev = NULL;
       for (peer = peers->peer; peer; peer = peer->next) {
           if (op->server.len == peer->name.len && ngx_strncmp(op->server.data, peer->name.data, peer->name.len) == 0) {
               if (peers == primary && peers->number == 1) {
                  op->status = NGX_HTTP_BAD_REQUEST;
                  unlock_peers(primary);
                  return NGX_ERROR;
               }
               target = peer;
               peer = peer->next;
               goto c;
            }
            prev = peer;
        }
    }

c:

    /* not found */
    if (target == NULL) {
        op->status = NGX_HTTP_BAD_REQUEST;
        ngx_log_error(NGX_LOG_ERR, log, 0,
                      "server %V is not found. %s:%d",
                      &op->server,
                      __FUNCTION__,
                      __LINE__);
        unlock_peers(primary);
        return NGX_ERROR;
    }
    weight = target->weight;
    /* released removed peer and attributes */
    if (ngx_dynamic_upstream_is_shpool_range(shpool, target->name.data)) {
        ngx_slab_free_locked(shpool, target->name.data);
    }

    if (ngx_dynamic_upstream_is_shpool_range(shpool, target->sockaddr)) {
        ngx_slab_free_locked(shpool, target->sockaddr);
    }

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
    peers->total_weight -= weight;
    peers->single = (peers->number == 1);
    peers->weighted = (peers->total_weight != peers->number);

    if (peers->number == 0) {
        assert(peers == backup);
        primary->next = NULL;
        ngx_slab_free_locked(shpool, backup);
    }

    unlock_peers(primary);

    ngx_log_error(NGX_LOG_NOTICE, log, 0,
                  "removed server %V", &op->server);

    return NGX_OK;
}


static ngx_int_t
ngx_dynamic_upstream_http_op_update_param(ngx_log_t *log, ngx_dynamic_upstream_op_t *op,
                                          ngx_slab_pool_t *shpool, ngx_http_upstream_rr_peers_t *primary)
{
    ngx_http_upstream_rr_peer_t   *peer, *target;
    ngx_http_upstream_rr_peers_t  *peers;

    lock_peers(primary);

    target = NULL;
    for (peers = primary; peers; peers = peers->next) {
        for (peer = peers->peer; peer ; peer = peer->next) {
            if (op->server.len == peer->name.len && ngx_strncmp(op->server.data, peer->name.data, peer->name.len) == 0) {
                target = peer;
                break;
            }
        }
    }

    if (target == NULL) {
        op->status = NGX_HTTP_BAD_REQUEST;
        ngx_log_error(NGX_LOG_ERR, log, 0,
                      "server %V is not found. %s:%d",
                      &op->server,
                      __FUNCTION__,
                      __LINE__);
        unlock_peers(primary)
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

    if (op->op_param & NGX_DYNAMIC_UPSTEAM_OP_PARAM_MAX_CONNS) {
        target->max_conns = op->max_conns;
    }

    if (op->op_param & NGX_DYNAMIC_UPSTEAM_OP_PARAM_FAIL_TIMEOUT) {
        target->fail_timeout = op->fail_timeout;
    }

    if (op->op_param & NGX_DYNAMIC_UPSTEAM_OP_PARAM_UP) {
        target->down = 0;
        ngx_log_error(NGX_LOG_NOTICE, log, 0,
                      "downed server %V", &op->server);
    }

    if (op->op_param & NGX_DYNAMIC_UPSTEAM_OP_PARAM_DOWN) {
        target->down = 1;
        ngx_log_error(NGX_LOG_NOTICE, log, 0,
                      "upped server %V", &op->server);
    }

    unlock_peers(primary)

    return NGX_OK;
}


static ngx_int_t
ngx_dynamic_upstream_stream_op_update_param(ngx_log_t *log, ngx_dynamic_upstream_op_t *op,
                                            ngx_slab_pool_t *shpool, ngx_stream_upstream_rr_peers_t *primary)
{
    ngx_stream_upstream_rr_peer_t   *peer, *target;
    ngx_stream_upstream_rr_peers_t  *peers;

    lock_peers(primary);

    target = NULL;
    for (peers = primary; peers; peers = peers->next) {
        for (peer = peers->peer; peer ; peer = peer->next) {
            if (op->server.len == peer->name.len && ngx_strncmp(op->server.data, peer->name.data, peer->name.len) == 0) {
                target = peer;
                break;
            }
        }
    }

    if (target == NULL) {
        op->status = NGX_HTTP_BAD_REQUEST;
        ngx_log_error(NGX_LOG_ERR, log, 0,
                      "server %V is not found. %s:%d",
                      &op->server,
                      __FUNCTION__,
                      __LINE__);
        unlock_peers(primary)
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

    if (op->op_param & NGX_DYNAMIC_UPSTEAM_OP_PARAM_MAX_CONNS) {
        target->max_conns = op->max_conns;
    }

    if (op->op_param & NGX_DYNAMIC_UPSTEAM_OP_PARAM_FAIL_TIMEOUT) {
        target->fail_timeout = op->fail_timeout;
    }

    if (op->op_param & NGX_DYNAMIC_UPSTEAM_OP_PARAM_UP) {
        target->down = 0;
        ngx_log_error(NGX_LOG_NOTICE, log, 0,
                      "downed server %V", &op->server);
    }

    if (op->op_param & NGX_DYNAMIC_UPSTEAM_OP_PARAM_DOWN) {
        target->down = 1;
        ngx_log_error(NGX_LOG_NOTICE, log, 0,
                      "upped server %V", &op->server);
    }

    unlock_peers(primary)

    return NGX_OK;
}
