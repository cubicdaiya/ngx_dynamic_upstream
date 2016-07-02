/**
 * these original functions are brought from
 * nginx/src/core/ngx_inet.c
 */


#include <ngx_config.h>
#include <ngx_core.h>


static ngx_int_t ngx_parse_unix_domain_url(ngx_slab_pool_t *pool, ngx_url_t *u);
static ngx_int_t ngx_parse_inet_url(ngx_slab_pool_t *pool, ngx_url_t *u);
static ngx_int_t ngx_parse_inet6_url(ngx_slab_pool_t *pool, ngx_url_t *u);
static ngx_int_t ngx_inet_resolve_host_slab(ngx_slab_pool_t *pool, ngx_url_t *u);


ngx_int_t
ngx_parse_url_slab(ngx_slab_pool_t *pool, ngx_url_t *u)
{
    u_char  *p;

    p = u->url.data;

    if (ngx_strncasecmp(p, (u_char *) "unix:", 5) == 0) {
        return ngx_parse_unix_domain_url(pool, u);
    }

    if (p[0] == '[') {
        return ngx_parse_inet6_url(pool, u);
    }

    return ngx_parse_inet_url(pool, u);
}


static ngx_int_t
ngx_parse_unix_domain_url(ngx_slab_pool_t *pool, ngx_url_t *u)
{
#if (NGX_HAVE_UNIX_DOMAIN)
    u_char              *path, *uri, *last;
    size_t               len;
    struct sockaddr_un  *saun;

    len = u->url.len;
    path = u->url.data;

    path += 5;
    len -= 5;

    if (u->uri_part) {

        last = path + len;
        uri = ngx_strlchr(path, last, ':');

        if (uri) {
            len = uri - path;
            uri++;
            u->uri.len = last - uri;
            u->uri.data = uri;
        }
    }

    if (len == 0) {
        u->err = "no path in the unix domain socket";
        return NGX_ERROR;
    }

    u->host.len = len++;
    u->host.data = path;

    if (len > sizeof(saun->sun_path)) {
        u->err = "too long path in the unix domain socket";
        return NGX_ERROR;
    }

    u->socklen = sizeof(struct sockaddr_un);
    saun = (struct sockaddr_un *) &u->sockaddr;
    saun->sun_family = AF_UNIX;
    (void) ngx_cpystrn((u_char *) saun->sun_path, path, len);

    u->addrs = ngx_slab_calloc_locked(pool, sizeof(ngx_addr_t));
    if (u->addrs == NULL) {
        return NGX_ERROR;
    }

    saun = ngx_slab_calloc_locked(pool, sizeof(struct sockaddr_un));
    if (saun == NULL) {
        return NGX_ERROR;
    }

    u->family = AF_UNIX;
    u->naddrs = 1;

    saun->sun_family = AF_UNIX;
    (void) ngx_cpystrn((u_char *) saun->sun_path, path, len);

    u->addrs[0].sockaddr = (struct sockaddr *) saun;
    u->addrs[0].socklen = sizeof(struct sockaddr_un);
    u->addrs[0].name.len = len + 4;
    u->addrs[0].name.data = u->url.data;

    return NGX_OK;

#else

    u->err = "the unix domain sockets are not supported on this platform";

    return NGX_ERROR;

#endif
}


static ngx_int_t
ngx_parse_inet_url(ngx_slab_pool_t *pool, ngx_url_t *u)
{
    u_char               *p, *host, *port, *last, *uri, *args;
    size_t                len;
    ngx_int_t             n;
    struct sockaddr_in   *sin;
#if (NGX_HAVE_INET6)
    struct sockaddr_in6  *sin6;
#endif

    u->socklen = sizeof(struct sockaddr_in);
    sin = (struct sockaddr_in *) &u->sockaddr;
    sin->sin_family = AF_INET;

    u->family = AF_INET;

    host = u->url.data;

    last = host + u->url.len;

    port = ngx_strlchr(host, last, ':');

    uri = ngx_strlchr(host, last, '/');

    args = ngx_strlchr(host, last, '?');

    if (args) {
        if (uri == NULL || args < uri) {
            uri = args;
        }
    }

    if (uri) {
        if (u->listen || !u->uri_part) {
            u->err = "invalid host";
            return NGX_ERROR;
        }

        u->uri.len = last - uri;
        u->uri.data = uri;

        last = uri;

        if (uri < port) {
            port = NULL;
        }
    }

    if (port) {
        port++;

        len = last - port;

        n = ngx_atoi(port, len);

        if (n < 1 || n > 65535) {
            u->err = "invalid port";
            return NGX_ERROR;
        }

        u->port = (in_port_t) n;
        sin->sin_port = htons((in_port_t) n);

        u->port_text.len = len;
        u->port_text.data = port;

        last = port - 1;

    } else {
        if (uri == NULL) {

            if (u->listen) {

                /* test value as port only */

                n = ngx_atoi(host, last - host);

                if (n != NGX_ERROR) {

                    if (n < 1 || n > 65535) {
                        u->err = "invalid port";
                        return NGX_ERROR;
                    }

                    u->port = (in_port_t) n;
                    sin->sin_port = htons((in_port_t) n);

                    u->port_text.len = last - host;
                    u->port_text.data = host;

                    u->wildcard = 1;

                    return NGX_OK;
                }
            }
        }

        u->no_port = 1;
        u->port = u->default_port;
        sin->sin_port = htons(u->default_port);
    }

    len = last - host;

    if (len == 0) {
        u->err = "no host";
        return NGX_ERROR;
    }

    u->host.len = len;
    u->host.data = host;

    if (u->listen && len == 1 && *host == '*') {
        sin->sin_addr.s_addr = INADDR_ANY;
        u->wildcard = 1;
        return NGX_OK;
    }

    sin->sin_addr.s_addr = ngx_inet_addr(host, len);

    if (sin->sin_addr.s_addr != INADDR_NONE) {

        if (sin->sin_addr.s_addr == INADDR_ANY) {
            u->wildcard = 1;
        }

        u->naddrs = 1;

        u->addrs = ngx_slab_calloc_locked(pool, sizeof(ngx_addr_t));
        if (u->addrs == NULL) {
            return NGX_ERROR;
        }

        sin = ngx_slab_calloc_locked(pool, sizeof(struct sockaddr_in));
        if (sin == NULL) {
            return NGX_ERROR;
        }

#if (nginx_version < 1011000)
        ngx_memcpy(sin, u->sockaddr, sizeof(struct sockaddr_in));
#else
        ngx_memcpy(sin, &u->sockaddr, sizeof(struct sockaddr_in));
#endif

        u->addrs[0].sockaddr = (struct sockaddr *) sin;
        u->addrs[0].socklen = sizeof(struct sockaddr_in);

        p = ngx_slab_alloc_locked(pool, u->host.len + sizeof(":65535") - 1);
        if (p == NULL) {
            return NGX_ERROR;
        }

        u->addrs[0].name.len = ngx_sprintf(p, "%V:%d",
                                           &u->host, u->port) - p;
        u->addrs[0].name.data = p;

        return NGX_OK;
    }

    if (u->no_resolve) {
        return NGX_OK;
    }

    if (ngx_inet_resolve_host_slab(pool, u) != NGX_OK) {
        return NGX_ERROR;
    }

    u->family = u->addrs[0].sockaddr->sa_family;
    u->socklen = u->addrs[0].socklen;
#if (nginx_version < 1011000)
    ngx_memcpy(u->sockaddr, u->addrs[0].sockaddr, u->addrs[0].socklen);
#else
    ngx_memcpy(&u->sockaddr, u->addrs[0].sockaddr, u->addrs[0].socklen);
#endif

    switch (u->family) {

#if (NGX_HAVE_INET6)
    case AF_INET6:
        sin6 = (struct sockaddr_in6 *) &u->sockaddr;

        if (IN6_IS_ADDR_UNSPECIFIED(&sin6->sin6_addr)) {
            u->wildcard = 1;
        }

        break;
#endif

    default: /* AF_INET */
        sin = (struct sockaddr_in *) &u->sockaddr;

        if (sin->sin_addr.s_addr == INADDR_ANY) {
            u->wildcard = 1;
        }

        break;
    }

    return NGX_OK;
}


static ngx_int_t
ngx_parse_inet6_url(ngx_slab_pool_t *pool, ngx_url_t *u)
{
#if (NGX_HAVE_INET6)
    u_char               *p, *host, *port, *last, *uri;
    size_t                len;
    ngx_int_t             n;
    struct sockaddr_in6  *sin6;

    u->socklen = sizeof(struct sockaddr_in6);
    sin6 = (struct sockaddr_in6 *) &u->sockaddr;
    sin6->sin6_family = AF_INET6;

    host = u->url.data + 1;

    last = u->url.data + u->url.len;

    p = ngx_strlchr(host, last, ']');

    if (p == NULL) {
        u->err = "invalid host";
        return NGX_ERROR;
    }

    if (last - p) {

        port = p + 1;

        uri = ngx_strlchr(port, last, '/');

        if (uri) {
            if (u->listen || !u->uri_part) {
                u->err = "invalid host";
                return NGX_ERROR;
            }

            u->uri.len = last - uri;
            u->uri.data = uri;

            last = uri;
        }

        if (*port == ':') {
            port++;

            len = last - port;

            n = ngx_atoi(port, len);

            if (n < 1 || n > 65535) {
                u->err = "invalid port";
                return NGX_ERROR;
            }

            u->port = (in_port_t) n;
            sin6->sin6_port = htons((in_port_t) n);

            u->port_text.len = len;
            u->port_text.data = port;

        } else {
            u->no_port = 1;
            u->port = u->default_port;
            sin6->sin6_port = htons(u->default_port);
        }
    }

    len = p - host;

    if (len == 0) {
        u->err = "no host";
        return NGX_ERROR;
    }

    u->host.len = len + 2;
    u->host.data = host - 1;

    if (ngx_inet6_addr(host, len, sin6->sin6_addr.s6_addr) != NGX_OK) {
        u->err = "invalid IPv6 address";
        return NGX_ERROR;
    }

    if (IN6_IS_ADDR_UNSPECIFIED(&sin6->sin6_addr)) {
        u->wildcard = 1;
    }

    u->family = AF_INET6;
    u->naddrs = 1;

    u->addrs = ngx_slab_calloc_locked(pool, sizeof(ngx_addr_t));
    if (u->addrs == NULL) {
        return NGX_ERROR;
    }

    sin6 = ngx_slab_calloc_locked(pool, sizeof(struct sockaddr_in6));
    if (sin6 == NULL) {
        return NGX_ERROR;
    }

#if (nginx_version < 1011000)
    ngx_memcpy(sin6, u->sockaddr, sizeof(struct sockaddr_in6));
#else
    ngx_memcpy(sin6, &u->sockaddr, sizeof(struct sockaddr_in6));
#endif
    u->addrs[0].sockaddr = (struct sockaddr *) sin6;
    u->addrs[0].socklen = sizeof(struct sockaddr_in6);

    p = ngx_slab_alloc(pool, u->host.len + sizeof(":65535") - 1);
    if (p == NULL) {
        return NGX_ERROR;
    }

    u->addrs[0].name.len = ngx_sprintf(p, "%V:%d",
                                       &u->host, u->port) - p;
    u->addrs[0].name.data = p;

    return NGX_OK;

#else

    u->err = "the INET6 sockets are not supported on this platform";

    return NGX_ERROR;

#endif
}


#if (NGX_HAVE_GETADDRINFO && NGX_HAVE_INET6)

static ngx_int_t
ngx_inet_resolve_host_slab(ngx_slab_pool_t *pool, ngx_url_t *u)
{
    u_char               *p, *host;
    size_t                len;
    in_port_t             port;
    ngx_uint_t            i;
    struct addrinfo       hints, *res, *rp;
    struct sockaddr_in   *sin;
    struct sockaddr_in6  *sin6;

    port = htons(u->port);

    host = ngx_slab_alloc_locked(pool, u->host.len + 1);
    if (host == NULL) {
        return NGX_ERROR;
    }

    (void) ngx_cpystrn(host, u->host.data, u->host.len + 1);

    ngx_memzero(&hints, sizeof(struct addrinfo));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
#ifdef AI_ADDRCONFIG
    hints.ai_flags = AI_ADDRCONFIG;
#endif

    if (getaddrinfo((char *) host, NULL, &hints, &res) != 0) {
        u->err = "host not found";
        ngx_slab_free_locked(pool, host);
        return NGX_ERROR;
    }

    ngx_slab_free_locked(pool, host);

    for (i = 0, rp = res; rp != NULL; rp = rp->ai_next) {

        switch (rp->ai_family) {

        case AF_INET:
        case AF_INET6:
            break;

        default:
            continue;
        }

        i++;
    }

    if (i == 0) {
        u->err = "host not found";
        goto failed;
    }

    /* MP: ngx_shared_palloc() */

    u->addrs = ngx_slab_calloc_locked(pool, i * sizeof(ngx_addr_t));
    if (u->addrs == NULL) {
        goto failed;
    }

    u->naddrs = i;

    i = 0;

    /* AF_INET addresses first */

    for (rp = res; rp != NULL; rp = rp->ai_next) {

        if (rp->ai_family != AF_INET) {
            continue;
        }

        sin = ngx_slab_calloc_locked(pool, rp->ai_addrlen);
        if (sin == NULL) {
            goto failed;
        }

        ngx_memcpy(sin, rp->ai_addr, rp->ai_addrlen);

        sin->sin_port = port;

        u->addrs[i].sockaddr = (struct sockaddr *) sin;
        u->addrs[i].socklen = rp->ai_addrlen;

        len = NGX_INET_ADDRSTRLEN + sizeof(":65535") - 1;

        p = ngx_slab_alloc(pool, len);
        if (p == NULL) {
            goto failed;
        }

        len = ngx_sock_ntop((struct sockaddr *) sin, rp->ai_addrlen, p, len, 1);

        u->addrs[i].name.len = len;
        u->addrs[i].name.data = p;

        i++;
    }

    for (rp = res; rp != NULL; rp = rp->ai_next) {

        if (rp->ai_family != AF_INET6) {
            continue;
        }

        sin6 = ngx_slab_calloc_locked(pool, rp->ai_addrlen);
        if (sin6 == NULL) {
            goto failed;
        }

        ngx_memcpy(sin6, rp->ai_addr, rp->ai_addrlen);

        sin6->sin6_port = port;

        u->addrs[i].sockaddr = (struct sockaddr *) sin6;
        u->addrs[i].socklen = rp->ai_addrlen;

        len = NGX_INET6_ADDRSTRLEN + sizeof("[]:65535") - 1;

        p = ngx_slab_alloc(pool, len);
        if (p == NULL) {
            goto failed;
        }

        len = ngx_sock_ntop((struct sockaddr *) sin6, rp->ai_addrlen, p,
                            len, 1);

        u->addrs[i].name.len = len;
        u->addrs[i].name.data = p;

        i++;
    }

    freeaddrinfo(res);
    return NGX_OK;

failed:

    freeaddrinfo(res);
    return NGX_ERROR;
}

#else /* !NGX_HAVE_GETADDRINFO || !NGX_HAVE_INET6 */

static ngx_int_t
ngx_inet_resolve_host_slab(ngx_slab_pool_t *pool, ngx_url_t *u)
{
    u_char              *p, *host;
    size_t               len;
    in_port_t            port;
    in_addr_t            in_addr;
    ngx_uint_t           i;
    struct hostent      *h;
    struct sockaddr_in  *sin;

    /* AF_INET only */

    port = htons(u->port);

    in_addr = ngx_inet_addr(u->host.data, u->host.len);

    if (in_addr == INADDR_NONE) {
        host = ngx_slab_alloc_locked(pool, u->host.len + 1);
        if (host == NULL) {
            return NGX_ERROR;
        }

        (void) ngx_cpystrn(host, u->host.data, u->host.len + 1);

        h = gethostbyname((char *) host);

        ngx_slab_free_locked(pool, host);

        if (h == NULL || h->h_addr_list[0] == NULL) {
            u->err = "host not found";
            return NGX_ERROR;
        }

        for (i = 0; h->h_addr_list[i] != NULL; i++) { /* void */ }

        /* MP: ngx_shared_palloc() */

        u->addrs = ngx_slab_calloc_locked(pool, i * sizeof(ngx_addr_t));
        if (u->addrs == NULL) {
            return NGX_ERROR;
        }

        u->naddrs = i;

        for (i = 0; i < u->naddrs; i++) {

            sin = ngx_slab_calloc_locked(pool, sizeof(struct sockaddr_in));
            if (sin == NULL) {
                return NGX_ERROR;
            }

            sin->sin_family = AF_INET;
            sin->sin_port = port;
            sin->sin_addr.s_addr = *(in_addr_t *) (h->h_addr_list[i]);

            u->addrs[i].sockaddr = (struct sockaddr *) sin;
            u->addrs[i].socklen = sizeof(struct sockaddr_in);

            len = NGX_INET_ADDRSTRLEN + sizeof(":65535") - 1;

            p = ngx_slab_alloc_locked(pool, len);
            if (p == NULL) {
                return NGX_ERROR;
            }

            len = ngx_sock_ntop((struct sockaddr *) sin,
                                sizeof(struct sockaddr_in), p, len, 1);

            u->addrs[i].name.len = len;
            u->addrs[i].name.data = p;
        }

    } else {

        /* MP: ngx_shared_palloc() */

        u->addrs = ngx_slab_calloc_locked(pool, sizeof(ngx_addr_t));
        if (u->addrs == NULL) {
            return NGX_ERROR;
        }

        sin = ngx_slab_calloc_locked(pool, sizeof(struct sockaddr_in));
        if (sin == NULL) {
            return NGX_ERROR;
        }

        u->naddrs = 1;

        sin->sin_family = AF_INET;
        sin->sin_port = port;
        sin->sin_addr.s_addr = in_addr;

        u->addrs[0].sockaddr = (struct sockaddr *) sin;
        u->addrs[0].socklen = sizeof(struct sockaddr_in);

        p = ngx_slab_alloc_locked(pool, u->host.len + sizeof(":65535") - 1);
        if (p == NULL) {
            return NGX_ERROR;
        }

        u->addrs[0].name.len = ngx_sprintf(p, "%V:%d",
                                           &u->host, ntohs(port)) - p;
        u->addrs[0].name.data = p;
    }

    return NGX_OK;
}

#endif /* NGX_HAVE_GETADDRINFO && NGX_HAVE_INET6 */
