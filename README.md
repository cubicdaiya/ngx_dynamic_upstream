# ngx_dynamic_upstream

`ngx_dynamic_upstream` is the module for operating upstreams dynamically with HTTP APIs
such as [`ngx_http_upstream_conf`](http://nginx.org/en/docs/http/ngx_http_upstream_conf_module.html).

# Required

`ngx_dynamic_upstream` requires the `zone` directive in the `upstream` context.
This directive is possible to nginx-1.9.0-plus.

# Status

This module is significantly experimental and still under early develpment phase.

# Directives

## dynamic_upstream

|Syntax |dynamic_upstream|
|-------|----------------|
|Default|-|
|Context|location|

# Quick Start

```nginx
upstream backends {
    zone zone_for_backends 1m;
    server 127.0.0.1:6001;
    server 127.0.0.1:6002;
    server 127.0.0.1:6003;
}

server {
    listen 6000;

    location /dynamic {
		allow 127.0.0.1;
	    deny all;
        dynamic_upstream;
    }

    location / {
	    proxy_pass http://backends;
    }
}
```

# HTTP APIs

You can operate upstreams dynamically with HTTP APIs.

## list

```bash
$ curl "http://127.0.0.1:6000/dynamic?upstream=backends"
127.0.0.1:6001; #id=0
127.0.0.1:6002; #id=1
127.0.0.1:6003; #id=2
$
```

## add

```bash
$ curl "http://127.0.0.1:6000/dynamic?upstream=backends&add=&server=127.0.0.1:6004"
127.0.0.1:6001; #id=0
127.0.0.1:6002; #id=1
127.0.0.1:6003; #id=2
127.0.0.1:6004; #id=3
$
```

## remove

```bash
$ curl "http://127.0.0.1:6000/dynamic?upstream=backends&remove=&id=3"
127.0.0.1:6001; #id=0
127.0.0.1:6002; #id=1
127.0.0.1:6004; #id=2
$
```

# License

See [LICENSE](https://github.com/cubicdaiya/ngx_dynamic_upstream/blob/master/LICENSE).
