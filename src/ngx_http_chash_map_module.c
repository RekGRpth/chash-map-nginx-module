
/*
 * Copyright (C) Yuyang Chen (Wine93)
 */


#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>


typedef struct ngx_http_chash_map_rr_peer_s   ngx_http_chash_map_rr_peer_t;

struct ngx_http_chash_map_rr_peer_s {
    ngx_http_variable_value_t     *value;

    ngx_int_t                      current_weight;
    ngx_int_t                      effective_weight;
    ngx_int_t                      weight;

    ngx_uint_t                     down;

    ngx_http_chash_map_rr_peer_t  *next;
};


typedef struct {
    ngx_uint_t                     number;
    ngx_uint_t                     total_weight;

    ngx_http_chash_map_rr_peer_t  *peer;
} ngx_http_chash_map_rr_peers_t;


typedef struct {
    uint32_t                       hash;
    ngx_http_variable_value_t     *value;
} ngx_http_chash_map_point_t;


typedef struct {
    ngx_uint_t                     number;
    ngx_http_chash_map_point_t     point[1];
} ngx_http_chash_map_points_t;


typedef struct {
    ngx_http_variable_value_t      value;
    ngx_uint_t                     weight;
    ngx_uint_t                     down;
} ngx_http_chash_map_part_t;


typedef struct {
    ngx_http_complex_value_t        value;
    ngx_array_t                     parts;

    ngx_http_chash_map_points_t    *points;
    ngx_http_chash_map_rr_peers_t  *peers;
} ngx_http_chash_map_ctx_t;


static ngx_int_t ngx_http_chash_map_init(ngx_conf_t *cf,
    ngx_http_chash_map_ctx_t *ctx);
static ngx_int_t ngx_http_chash_map_init_round_robin(ngx_conf_t *cf,
    ngx_http_chash_map_ctx_t *ctx);
static int ngx_libc_cdecl ngx_http_chash_map_cmp_points(const void *one,
    const void *two);
static ngx_uint_t ngx_http_chash_map_find_point(
    ngx_http_chash_map_points_t *points, uint32_t hash);
static ngx_int_t ngx_http_chash_map_get_peer(ngx_http_chash_map_ctx_t *ctx,
    ngx_http_variable_value_t *v, uint32_t hash);
static char *ngx_conf_chash_map_block(ngx_conf_t *cf, ngx_command_t *cmd,
    void *conf);
static char *ngx_http_chash_map(ngx_conf_t *cf, ngx_command_t *dummy,
    void *conf);


static ngx_command_t  ngx_http_chash_map_commands[] = {

    { ngx_string("chash_map"),
      NGX_HTTP_MAIN_CONF|NGX_CONF_BLOCK|NGX_CONF_TAKE2,
      ngx_conf_chash_map_block,
      NGX_HTTP_MAIN_CONF_OFFSET,
      0,
      NULL },

      ngx_null_command
};


static ngx_http_module_t  ngx_http_chash_map_module_ctx = {
    NULL,                                  /* preconfiguration */
    NULL,                                  /* postconfiguration */

    NULL,                                  /* create main configuration */
    NULL,                                  /* init main configuration */

    NULL,                                  /* create server configuration */
    NULL,                                  /* merge server configuration */

    NULL,                                  /* create location configuration */
    NULL                                   /* merge location configuration */
};


ngx_module_t  ngx_http_chash_map_module = {
    NGX_MODULE_V1,
    &ngx_http_chash_map_module_ctx,        /* module context */
    ngx_http_chash_map_commands,           /* module directives */
    NGX_HTTP_MODULE,                       /* module type */
    NULL,                                  /* init master */
    NULL,                                  /* init module */
    NULL,                                  /* init process */
    NULL,                                  /* init thread */
    NULL,                                  /* exit thread */
    NULL,                                  /* exit process */
    NULL,                                  /* exit master */
    NGX_MODULE_V1_PADDING
};


static ngx_int_t
ngx_http_chash_map_variable(ngx_http_request_t *r,
    ngx_http_variable_value_t *v, uintptr_t data)
{
    ngx_http_chash_map_ctx_t *ctx = (ngx_http_chash_map_ctx_t *) data;

    uint32_t   hash;
    ngx_str_t  val;
    ngx_int_t  rc;

    *v = ngx_http_variable_null_value;

    if (ngx_http_complex_value(r, &ctx->value, &val) != NGX_OK) {
        return NGX_OK;
    }

    hash = ngx_crc32_long(val.data, val.len);

    hash = ngx_http_chash_map_find_point(ctx->points, hash);

    rc = ngx_http_chash_map_get_peer(ctx, v, hash);

    if (rc == NGX_BUSY) {
        ngx_log_error(NGX_LOG_ERR, r->connection->log, 0, "no available part");
    }

    return NGX_OK;
}


static ngx_int_t
ngx_http_chash_map_init(ngx_conf_t *cf, ngx_http_chash_map_ctx_t *ctx)
{
    size_t                          size;
    uint32_t                        hash, base_hash;
    ngx_uint_t                      npoints, i, j;
    ngx_http_variable_value_t      *value;
    ngx_http_chash_map_points_t    *points;
    ngx_http_chash_map_rr_peer_t   *peer;
    ngx_http_chash_map_rr_peers_t  *peers;
    union {
        uint32_t                    value;
        u_char                      byte[4];
    } prev_hash;

    if (ngx_http_chash_map_init_round_robin(cf, ctx) != NGX_OK) {
        return NGX_ERROR;
    }

    peers = ctx->peers;
    npoints = peers->total_weight * 160;

    size = sizeof(ngx_http_chash_map_points_t)
           + sizeof(ngx_http_chash_map_point_t) * (npoints - 1);

    points = ngx_palloc(cf->pool, size);
    if (points == NULL) {
        return NGX_ERROR;
    }

    points->number = 0;

    for (peer = peers->peer; peer; peer = peer->next) {
        value = peer->value;

        ngx_crc32_init(base_hash);
        ngx_crc32_update(&base_hash, value->data, value->len);

        prev_hash.value = 0;
        npoints = peer->weight * 160;

        for (j = 0; j < npoints; j++) {
            hash = base_hash;

            ngx_crc32_update(&hash, prev_hash.byte, 4);
            ngx_crc32_final(hash);

            points->point[points->number].hash = hash;
            points->point[points->number].value = value;
            points->number++;

#if (NGX_HAVE_LITTLE_ENDIAN)
            prev_hash.value = hash;
#else
            prev_hash.byte[0] = (u_char) (hash & 0xff);
            prev_hash.byte[1] = (u_char) ((hash >> 8) & 0xff);
            prev_hash.byte[2] = (u_char) ((hash >> 16) & 0xff);
            prev_hash.byte[3] = (u_char) ((hash >> 24) & 0xff);
#endif
        }
    }

    ngx_qsort(points->point,
              points->number,
              sizeof(ngx_http_chash_map_point_t),
              ngx_http_chash_map_cmp_points);

    for (i = 0, j = 1; j < points->number; j++) {
        if (points->point[i].hash != points->point[j].hash) {
            points->point[++i] = points->point[j];
        }
    }

    points->number = i + 1;

    ctx->points = points;

    return NGX_OK;
}


static ngx_int_t
ngx_http_chash_map_init_round_robin(ngx_conf_t *cf,
    ngx_http_chash_map_ctx_t *ctx)
{
    ngx_uint_t                      i, n, w;
    ngx_http_chash_map_part_t      *part;
    ngx_http_chash_map_rr_peer_t   *peer, **peerp;
    ngx_http_chash_map_rr_peers_t  *peers;

    n = ctx->parts.nelts;

    if (n == 0) {
        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0, "no part in chash_map");
        return NGX_ERROR;
    }

    peers = ngx_pcalloc(cf->pool, sizeof(ngx_http_chash_map_rr_peers_t));
    if (peers == NULL) {
        return NGX_ERROR;
    }

    peer = ngx_pcalloc(cf->pool, sizeof(ngx_http_chash_map_rr_peer_t) * n);
    if (peer == NULL) {
        return NGX_ERROR;
    }

    part = ctx->parts.elts;
    peerp = &peers->peer;
    w = 0;

    for (i = 0; i < ctx->parts.nelts; i++) {
        peer[i].value = &part[i].value;
        peer[i].weight = part[i].weight;
        peer[i].effective_weight = part[i].weight;
        peer[i].current_weight = 0;
        peer[i].down = part[i].down;

        *peerp = &peer[i];
        peerp = &peer[i].next;
        w += part[i].weight;
    }

    peers->number = n;
    peers->total_weight = w;

    ctx->peers = peers;

    return NGX_OK;
}


static int ngx_libc_cdecl
ngx_http_chash_map_cmp_points(const void *one, const void *two)
{
    ngx_http_chash_map_point_t *first = (ngx_http_chash_map_point_t *) one;
    ngx_http_chash_map_point_t *second = (ngx_http_chash_map_point_t *) two;

    if (first->hash < second->hash) {
        return -1;

    } else if (first->hash > second->hash) {
        return 1;

    } else {
        return 0;
    }
}


static ngx_uint_t
ngx_http_chash_map_find_point(ngx_http_chash_map_points_t *points,
    uint32_t hash)
{
    ngx_uint_t                   i, j, k;
    ngx_http_chash_map_point_t  *point;

    /* find first point >= hash */

    point = &points->point[0];

    i = 0;
    j = points->number;

    while (i < j) {
        k = (i + j) / 2;

        if (hash > point[k].hash) {
            i = k + 1;

        } else if (hash < point[k].hash) {
            j = k;

        } else {
            return k;
        }
    }

    return i;
}


static ngx_int_t
ngx_http_chash_map_get_peer(ngx_http_chash_map_ctx_t *ctx,
    ngx_http_variable_value_t *v, uint32_t hash)
{
    ngx_int_t                      total;
    ngx_uint_t                     tries;
    ngx_http_variable_value_t     *value;
    ngx_http_chash_map_point_t    *point;
    ngx_http_chash_map_points_t   *points;
    ngx_http_chash_map_rr_peer_t  *peer, *best;

    tries = 0;

    points = ctx->points;
    point = &points->point[0];

    for ( ;; ) {
        value = point[hash % points->number].value;

        best = NULL;
        total = 0;

        for (peer = ctx->peers->peer; peer; peer = peer->next) {

            if (peer->down) {
                continue;
            }

            if (peer->value->len != value->len
                || ngx_strncmp(peer->value->data, value->data, value->len)
                   != 0)
            {
                continue;
            }

            peer->current_weight += peer->effective_weight;
            total += peer->effective_weight;

            if (peer->effective_weight < peer->weight) {
                peer->effective_weight++;
            }

            if (best == NULL || peer->current_weight > best->current_weight) {
                best = peer;
            }
        }

        if (best) {
            *v = *best->value;
            best->current_weight -= total;
            return NGX_OK;
        }

        hash++;
        tries++;

        if (tries > points->number) {
            *v = *ctx->peers->peer->value;
            return NGX_BUSY;
        }
    }

    return NGX_OK;
}


static char *
ngx_conf_chash_map_block(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    char                              *rv;
    ngx_str_t                         *value, name;
    ngx_conf_t                         save;
    ngx_http_variable_t               *var;
    ngx_http_chash_map_ctx_t          *ctx;
    ngx_http_compile_complex_value_t   ccv;

    ctx = ngx_pcalloc(cf->pool, sizeof(ngx_http_chash_map_ctx_t));
    if (ctx == NULL) {
        return NGX_CONF_ERROR;
    }

    value = cf->args->elts;

    ngx_memzero(&ccv, sizeof(ngx_http_compile_complex_value_t));

    ccv.cf = cf;
    ccv.value = &value[1];
    ccv.complex_value = &ctx->value;

    if (ngx_http_compile_complex_value(&ccv) != NGX_OK) {
        return NGX_CONF_ERROR;
    }

    name = value[2];

    if (name.data[0] != '$') {
        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                           "invalid variable name \"%V\"", &name);
        return NGX_CONF_ERROR;
    }

    name.len--;
    name.data++;

    var = ngx_http_add_variable(cf, &name, NGX_HTTP_VAR_CHANGEABLE);
    if (var == NULL) {
        return NGX_CONF_ERROR;
    }

    var->get_handler = ngx_http_chash_map_variable;
    var->data = (uintptr_t) ctx;

    if (ngx_array_init(&ctx->parts, cf->pool, 2,
                       sizeof(ngx_http_chash_map_part_t))
        != NGX_OK)
    {
        return NGX_CONF_ERROR;
    }

    save = *cf;
    cf->ctx = ctx;
    cf->handler = ngx_http_chash_map;
    cf->handler_conf = conf;

    rv = ngx_conf_parse(cf, NULL);

    *cf = save;

    if (rv != NGX_CONF_OK) {
        return rv;
    }

    if (ngx_http_chash_map_init(cf, ctx) != NGX_OK) {
        return NGX_CONF_ERROR;
    }

    return NGX_CONF_OK;
}


static char *
ngx_http_chash_map(ngx_conf_t *cf, ngx_command_t *dummy, void *conf)
{
    ngx_str_t                  *value;
    ngx_int_t                   weight, down;
    ngx_uint_t                  i;
    ngx_http_chash_map_ctx_t   *ctx;
    ngx_http_chash_map_part_t  *part;

    ctx = cf->ctx;

    part = ngx_array_push(&ctx->parts);
    if (part == NULL) {
        return NGX_CONF_ERROR;
    }

    value = cf->args->elts;

    weight = 1;
    down = 0;

    for (i = 1; i < cf->args->nelts; i++) {

        if (ngx_strncmp(value[i].data, "weight=", 7) == 0) {

            weight = ngx_atoi(&value[i].data[7], value[i].len - 7);

            if (weight == NGX_ERROR || weight == 0) {
                goto invalid;
            }

            continue;
        }

        if (ngx_strcmp(value[i].data, "down") == 0) {

            down = 1;

            continue;
        }

        goto invalid;
    }

    part->value.len = value[0].len;
    part->value.valid = 1;
    part->value.no_cacheable = 0;
    part->value.not_found = 0;
    part->value.data = value[0].data;
    part->weight = weight;
    part->down = down;

    return NGX_CONF_OK;

invalid:

    ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                       "invalid parameter \"%V\"", &value[i]);

    return NGX_CONF_ERROR;
}
