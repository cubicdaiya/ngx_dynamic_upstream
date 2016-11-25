use lib 'lib';
use Test::Nginx::Socket;
use Test::Nginx::Socket::Lua::Stream;

#repeat_each(2);

plan tests => repeat_each() * 2 * blocks();

run_tests();

__DATA__

=== TEST 1: remove head
--- stream_config
    upstream backends {
        zone zone_for_backends 128k;
        server 127.0.0.1:6001;
        server 127.0.0.1:6002;
        server 127.0.0.1:6003;
    }
--- stream_server_config
    proxy_pass backends;
--- config
    location /dynamic {
        dynamic_upstream;
    }
--- request
    GET /dynamic?upstream=zone_for_backends&server=127.0.0.1:6001&remove=&stream=
--- response_body
server 127.0.0.1:6002;
server 127.0.0.1:6003;


=== TEST 2: remove tail
--- stream_config
    upstream backends {
        zone zone_for_backends 128k;
        server 127.0.0.1:6001;
        server 127.0.0.1:6002;
        server 127.0.0.1:6003;
    }
--- stream_server_config
    proxy_pass backends;
--- config
    location /dynamic {
        dynamic_upstream;
    }
--- request
    GET /dynamic?upstream=zone_for_backends&server=127.0.0.1:6003&remove=&stream=
--- response_body
server 127.0.0.1:6001;
server 127.0.0.1:6002;


=== TEST 3: remove middle
--- stream_config
    upstream backends {
        zone zone_for_backends 128k;
        server 127.0.0.1:6001;
        server 127.0.0.1:6002;
        server 127.0.0.1:6003;
    }
--- stream_server_config
    proxy_pass backends;
--- config
    location /dynamic {
        dynamic_upstream;
    }
--- request
    GET /dynamic?upstream=zone_for_backends&server=127.0.0.1:6002&remove=&stream=
--- response_body
server 127.0.0.1:6001;
server 127.0.0.1:6003;


=== TEST 4: fail to remove
--- stream_config
    upstream backends {
        zone zone_for_backends 128k;
        server 127.0.0.1:6001;
        server 127.0.0.1:6002;
        server 127.0.0.1:6003;
    }
--- stream_server_config
    proxy_pass backends;
--- config
    location /dynamic {
        dynamic_upstream;
    }
--- request
    GET /dynamic?upstream=zone_for_backends&server=127.0.0.1:6004&remove=&stream=
--- response_body_like: 400 Bad Request
--- error_code: 400

=== TEST 5: remove backup
--- stream_config
    upstream backends {
        zone zone_for_backends 128k;
        server 127.0.0.1:6001;
        server 127.0.0.1:6002;
        server 127.0.0.1:6003 backup;
    }
--- stream_server_config
    proxy_pass backends;
--- config
    location /dynamic {
        dynamic_upstream;
    }
--- request
    GET /dynamic?upstream=zone_for_backends&server=127.0.0.1:6003&remove=&stream=
--- response_body
server 127.0.0.1:6001;
server 127.0.0.1:6002;
