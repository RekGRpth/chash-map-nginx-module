# vim:set ft= ts=4 sw=4 et fdm=marker:

use lib 'lib';
use Test::Nginx::Socket 'no_plan';

repeat_each(10);

run_tests();

__DATA__

=== TEST 1: basic
--- http_config
    chash_map $request_uri $variant {
        one;
        two;
        three;
    }
--- config
    location /chash_map {
        add_header X-Variant $variant;
        return 200;
    }
--- request
    GET /chash_map
--- response_headers
X-Variant: one



=== TEST 2: access by different key
--- http_config
    chash_map $request_uri $variant {
        one;
        two;
        three;
    }
--- config
    location /chash_map {
        add_header X-Variant $variant;
        return 200;
    }
--- request
    GET /chash_map?foo=blah&bar=hi
--- response_headers
X-Variant: three
