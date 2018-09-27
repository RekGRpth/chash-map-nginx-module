Name
====

**ngx_chash_map** - Creates variables whose values are mapped to group by consistent hashing method

*This module is not distributed with the Nginx source.* See [the installation instructions](#installation).

Table of Contents
=================

* [Name](#name)
* [Status](#status)
* [Version](#version)
* [Synopsis](#synopsis)
* [Description](#description)
* [Directives](#directives)
    * [chash_map](#chash_map)
* [Installation](#installation)
* [Test Suite](#test-suite)
* [Author](#author)
* [Copyright & License](#copyright--license)
* [See Also](#see-also)

Status
======

This module is production ready.

Version
=======
This document describes chash_map [v0.01](https://github.com/Wine93/chash-map-nginx-module/tags) released on 27 Sep 2018.

Synopsis
========

```nginx

http {
    ...

    chash_map  $host$request_uri $hdd_cache {
               hdd_cache00 weight=1;
               hdd_cache01 weight=2 down;
               hdd_cache02 weight=3;
    }

    proxy_cache_path  /hdd00/data/nginx/cache levels=1:2 keys_zone=hdd_cache00:10m;
    proxy_cache_path  /hdd01/data/nginx/cache levels=1:2 keys_zone=hdd_cache01:10m;
    proxy_cache_path  /hdd02/data/nginx/cache levels=1:2 keys_zone=hdd_cache02:10m;

    server {
        ...

        location / {
            ...

            proxy_pass   https://localhost:8000;
            proxy_cache  $hdd_cache;
        }
    }
}
```

[Back to TOC](#table-of-contents)

Description
===========

This module allows you to create variables whose values are mapped to group by consistent hashing method.

Also you can use [chash_map](#chash_map) directive to distribute cached content to multiple disk dirves,
the consistent hashing method ensures that only a few cached contents will be redistributed to different disk dirves
when a disk drive is added to or removed from the group.

[Back to TOC](#table-of-contents)

Directives
==========

chash_map
---------

**syntax:** *chash_map key $variable { ... }*

**default:** *-*

**context:** *http*

Creates a new variable whose value is mapped to group by consistent hashing method, for example:

```nginx

chash_map "${remote_addr}AAA" $variant {
    one;
    two;
    three;
}
```

The mapping is based on the hashed `key` value, the `key` can contain text, variables, and their combinations.

Parameters inside the `map` block specify a `part` in the group.

The `part` value must be text, for each `part`, the following special parameters are also supported:

*weight=number*
>    sets the weight of the part, by default, 1.

*down*
>    marks the part as permanently unavailable.

[Back to TOC](#table-of-contents)

Installation
============

Grab the nginx source code from [nginx.org](http://nginx.org/), for example,
the version 1.13.10, and then build the source with this module:

```bash

 $ wget 'http://nginx.org/download/nginx-1.13.10.tar.gz'
 $ tar -xzvf nginx-1.13.10.tar.gz
 $ cd nginx-1.13.10/

 # Here we assume you would install you nginx under /opt/nginx/.
 $ ./configure --prefix=/opt/nginx \
     --add-module=/path/to/chash-map-nginx-module

 $ make -j2
 $ make install
```


[Back to TOC](#table-of-contents)


Test Suite
==========

This module comes with a Perl-driven test suite. The [test cases](http://github.com/Wine93/chash-map-nginx-module/tree/master/t/) are
[declarative](http://github.com/Wine93/chash-map-nginx-module/blob/master/t/sanity.t) too. Thanks to the [Test::Nginx](http://search.cpan.org/perldoc?Test::Nginx) module in the Perl world.

To run it on your side:

```bash

 $ PATH=/path/to/your/nginx-with-chash-map-module:$PATH prove -r t
```

Because a single nginx server (by default, `localhost:1984`) is used across all the test scripts (`.t` files), it's meaningless to run the test suite in parallel by specifying `-jN` when invoking the `prove` utility.

[Back to TOC](#table-of-contents)


Author
======

Yuyang Chen (Wine93) *&lt;wine93.info@gmail.com&gt;*.

[Back to TOC](#table-of-contents)

Copyright & License
===================
The implementation of the module has borrowed a lot of code from Roman Arutyunyan's upstream_hash module. This part of code is copyrighted by Roman Arutyunyan.

Copyright (c) 2017-2018, Yuyang Chen (Wine93) <wine93.info@gmail.com>.

This module is licensed under the terms of the BSD license.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions
are met:

* Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.
* Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
"AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED
TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

[Back to TOC](#table-of-contents)

See Also
========

* The standard [split_clients](http://nginx.org/en/docs/http/ngx_http_split_clients_module.html) module.
* This wiki page style borrowed from [headers-more-nginx-module](https://github.com/openresty/headers-more-nginx-module).

[Back to TOC](#table-of-contents)
