use lib 'lib';
use Test::Nginx::Socket;

#repeat_each(2);

plan tests => repeat_each() * 2 * blocks();

run_tests();

__DATA__

=== TEST 1: update all parameters
--- http_config
    upstream backends {
        zone zone_for_backends 128k;
        server 127.0.0.1:6001;
        server 127.0.0.1:6002;
        server 127.0.0.1:6003;
    }
--- config
    location /dynamic {
        dynamic_upstream;
    }
--- request
    GET /dynamic?upstream=zone_for_backends&server=127.0.0.1:6003&weight=10&max_fails=5&fail_timeout=5
--- response_body
server 127.0.0.1:6001 weight=1 max_fails=1 fail_timeout=10;
server 127.0.0.1:6002 weight=1 max_fails=1 fail_timeout=10;
server 127.0.0.1:6003 weight=10 max_fails=5 fail_timeout=5;


=== TEST 2: update weight parameter
--- http_config
    upstream backends {
        zone zone_for_backends 128k;
        server 127.0.0.1:6001;
        server 127.0.0.1:6002;
        server 127.0.0.1:6003;
    }
--- config
    location /dynamic {
        dynamic_upstream;
    }
--- request
    GET /dynamic?upstream=zone_for_backends&server=127.0.0.1:6003&weight=10
--- response_body
server 127.0.0.1:6001 weight=1 max_fails=1 fail_timeout=10;
server 127.0.0.1:6002 weight=1 max_fails=1 fail_timeout=10;
server 127.0.0.1:6003 weight=10 max_fails=1 fail_timeout=10;


=== TEST 3: update max_fails parameter
--- http_config
    upstream backends {
        zone zone_for_backends 128k;
        server 127.0.0.1:6001;
        server 127.0.0.1:6002;
        server 127.0.0.1:6003;
    }
--- config
    location /dynamic {
        dynamic_upstream;
    }
--- request
    GET /dynamic?upstream=zone_for_backends&server=127.0.0.1:6003&max_fails=5
--- response_body
server 127.0.0.1:6001 weight=1 max_fails=1 fail_timeout=10;
server 127.0.0.1:6002 weight=1 max_fails=1 fail_timeout=10;
server 127.0.0.1:6003 weight=1 max_fails=5 fail_timeout=10;


=== TEST 4: update fail_timeout parameter
--- http_config
    upstream backends {
        zone zone_for_backends 128k;
        server 127.0.0.1:6001;
        server 127.0.0.1:6002;
        server 127.0.0.1:6003;
    }
--- config
    location /dynamic {
        dynamic_upstream;
    }
--- request
    GET /dynamic?upstream=zone_for_backends&server=127.0.0.1:6003&fail_timeout=5
--- response_body
server 127.0.0.1:6001 weight=1 max_fails=1 fail_timeout=10;
server 127.0.0.1:6002 weight=1 max_fails=1 fail_timeout=10;
server 127.0.0.1:6003 weight=1 max_fails=1 fail_timeout=5;


=== TEST 5: fail to update weight parameter
--- http_config
    upstream backends {
        zone zone_for_backends 128k;
        server 127.0.0.1:6001;
        server 127.0.0.1:6002;
        server 127.0.0.1:6003;
    }
--- config
    location /dynamic {
        dynamic_upstream;
    }
--- request
    GET /dynamic?upstream=zone_for_backends&server=127.0.0.1:6003&weight=abc
--- response_body_like: 400 Bad Request
--- error_code: 400


=== TEST 6: fail to update max_fails parameter
--- http_config
    upstream backends {
        zone zone_for_backends 128k;
        server 127.0.0.1:6001;
        server 127.0.0.1:6002;
        server 127.0.0.1:6003;
    }
--- config
    location /dynamic {
        dynamic_upstream;
    }
--- request
    GET /dynamic?upstream=zone_for_backends&server=127.0.0.1:6003&max_fails=abc
--- response_body_like: 400 Bad Request
--- error_code: 400


=== TEST 7: fail to update fail_timeout parameter
--- http_config
    upstream backends {
        zone zone_for_backends 128k;
        server 127.0.0.1:6001;
        server 127.0.0.1:6002;
        server 127.0.0.1:6003;
    }
--- config
    location /dynamic {
        dynamic_upstream;
    }
--- request
    GET /dynamic?upstream=zone_for_backends&server=127.0.0.1:6003&fail_timeout=abc
--- response_body_like: 400 Bad Request
--- error_code: 400
