#ifndef NGX_DYNAMIC_UPSTEAM_INET_SLAB_H
#define NGX_DYNAMIC_UPSTEAM_INET_SLAB_H

#include <ngx_config.h>
#include <ngx_core.h>

ngx_int_t ngx_parse_url_slab(ngx_slab_pool_t *pool, ngx_url_t *u);

#endif /* NGX_DYNAMIC_UPSTEAM_INET_SLAB_H */
