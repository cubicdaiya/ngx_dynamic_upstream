# ngx_dynamic_upstream

[![Build Status](https://drone.io/github.com/cubicdaiya/ngx_dynamic_upstream/status.png)](https://drone.io/github.com/cubicdaiya/ngx_dynamic_upstream/latest)

`ngx_dynamic_upstream` is the module for operating upstreams dynamically with HTTP APIs
such as [`ngx_http_upstream_conf`](http://nginx.org/en/docs/http/ngx_http_upstream_conf_module.html).

This module also supports stream upstreams manipulation.

# Requirements

`ngx_dynamic_upstream` requires the `zone` directive in the `upstream` context.

# Status

Production ready.

# Directives

## dynamic_upstream

|Syntax |dynamic_upstream|
|-------|----------------|
|Default|-|
|Context|location|

Now `ngx_dynamic_upstream` supports dynamic upstream under only `http` context.

# Quick Start

```nginx

http {
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
}

stream {
    upstream backends_stream {
        zone zone_for_backends_stream 1m;
        server 127.0.0.1:6001;
        server 127.0.0.1:6002;
        server 127.0.0.1:6003;
    }

    server {
        listen 6001;
        proxy_pass backends_stream;
    }
}
```

# HTTP APIs

You can operate upstreams dynamically with HTTP APIs.

## list

```bash
$ curl "http://127.0.0.1:6000/dynamic?upstream=zone_for_backends"
server 127.0.0.1:6001;
server 127.0.0.1:6002;
server 127.0.0.1:6003;
$
```

## verbose

```bash
$ curl "http://127.0.0.1:6000/dynamic?upstream=zone_for_backends&verbose="
server 127.0.0.1:6001 weight=1 max_fails=1 fail_timeout=10;
server 127.0.0.1:6002 weight=1 max_fails=1 fail_timeout=10;
server 127.0.0.1:6003 weight=1 max_fails=1 fail_timeout=10;
$
```

## update_parameters

```bash
$ curl "http://127.0.0.1:6000/dynamic?upstream=zone_for_backends&server=127.0.0.1:6003&weight=10&max_fails=5&fail_timeout=5"
server 127.0.0.1:6001 weight=1 max_fails=1 fail_timeout=10;
server 127.0.0.1:6002 weight=1 max_fails=1 fail_timeout=10;
server 127.0.0.1:6003 weight=10 max_fails=5 fail_timeout=5;
$
```

The supported parameters are blow.

 * weight
 * max_fails
 * fail_timeout

## down

```bash
$ curl "http://127.0.0.1:6000/dynamic?upstream=zone_for_backends&server=127.0.0.1:6003&down="
server 127.0.0.1:6001 weight=1 max_fails=1 fail_timeout=10;
server 127.0.0.1:6002 weight=1 max_fails=1 fail_timeout=10;
server 127.0.0.1:6003 weight=1 max_fails=1 fail_timeout=10 down;
$
```

## up

```bash
$ curl "http://127.0.0.1:6000/dynamic?upstream=zone_for_backends&server=127.0.0.1:6003&up="
server 127.0.0.1:6001 weight=1 max_fails=1 fail_timeout=10;
server 127.0.0.1:6002 weight=1 max_fails=1 fail_timeout=10;
server 127.0.0.1:6003 weight=1 max_fails=1 fail_timeout=10;
$
```

## add

```bash
$ curl "http://127.0.0.1:6000/dynamic?upstream=zone_for_backends&add=&server=127.0.0.1:6004"
server 127.0.0.1:6001;
server 127.0.0.1:6002;
server 127.0.0.1:6003;
server 127.0.0.1:6004;
$
```

## add backup

```bash
$ curl "http://127.0.0.1:6000/dynamic?upstream=zone_for_backends&add=&server=127.0.0.1:6004&backup="
server 127.0.0.1:6001;
server 127.0.0.1:6002;
server 127.0.0.1:6003;
server 127.0.0.1:6004 backup;
$
```

## remove

```bash
$ curl "http://127.0.0.1:6000/dynamic?upstream=zone_for_backends&remove=&server=127.0.0.1:6003"
server 127.0.0.1:6001;
server 127.0.0.1:6002;
server 127.0.0.1:6004;
$
```

## add stream

```bash
$ curl "http://127.0.0.1:6000/dynamic?upstream=zone_for_backends_stream&add=&server=127.0.0.1:6004&stream="
server 127.0.0.1:6001;
server 127.0.0.1:6002;
server 127.0.0.1:6003;
server 127.0.0.1:6004;
$
```

## add backup stream

```bash
$ curl "http://127.0.0.1:6000/dynamic?upstream=zone_for_backends_stream&add=&server=127.0.0.1:6004&backup=&stream="
server 127.0.0.1:6001;
server 127.0.0.1:6002;
server 127.0.0.1:6003;
server 127.0.0.1:6004 backup;
$
```

## remove stream

```bash
$ curl "http://127.0.0.1:6000/dynamic?upstream=zone_for_backends_stream&remove=&server=127.0.0.1:6003&stream="
server 127.0.0.1:6001;
server 127.0.0.1:6002;
server 127.0.0.1:6004;
$
```

# License

See [LICENSE](https://github.com/cubicdaiya/ngx_dynamic_upstream/blob/master/LICENSE).
