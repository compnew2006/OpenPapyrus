/*
 * Copyright (C) Igor Sysoev
 * Copyright (C) Nginx, Inc.
 */
#include <ngx_config.h>
#include <ngx_core.h>
#pragma hdrstop

typedef struct ngx_http_header_val_s ngx_http_header_val_t;
typedef ngx_int_t (*ngx_http_set_header_pt)(ngx_http_request_t * r, ngx_http_header_val_t * hv, ngx_str_t * value);

typedef struct {
	ngx_str_t name;
	ngx_uint_t offset;
	ngx_http_set_header_pt handler;
} ngx_http_set_header_t;

struct ngx_http_header_val_s {
	ngx_http_complex_value_t value;
	ngx_str_t key;
	ngx_http_set_header_pt handler;
	ngx_uint_t offset;
	ngx_uint_t always; /* unsigned  always:1 */
};

typedef enum {
	NGX_HTTP_EXPIRES_OFF,
	NGX_HTTP_EXPIRES_EPOCH,
	NGX_HTTP_EXPIRES_MAX,
	NGX_HTTP_EXPIRES_ACCESS,
	NGX_HTTP_EXPIRES_MODIFIED,
	NGX_HTTP_EXPIRES_DAILY,
	NGX_HTTP_EXPIRES_UNSET
} ngx_http_expires_t;

typedef struct {
	ngx_http_expires_t expires;
	time_t expires_time;
	ngx_http_complex_value_t  * expires_value;
	ngx_array_t * headers;
	ngx_array_t * trailers;
} ngx_http_headers_conf_t;

static ngx_int_t ngx_http_set_expires(ngx_http_request_t * r, ngx_http_headers_conf_t * conf);
static ngx_int_t ngx_http_parse_expires(ngx_str_t * value, ngx_http_expires_t * expires, time_t * expires_time, const char ** err);
static ngx_int_t ngx_http_add_cache_control(ngx_http_request_t * r, ngx_http_header_val_t * hv, ngx_str_t * value);
static ngx_int_t ngx_http_add_header(ngx_http_request_t * r, ngx_http_header_val_t * hv, ngx_str_t * value);
static ngx_int_t ngx_http_set_last_modified(ngx_http_request_t * r, ngx_http_header_val_t * hv, ngx_str_t * value);
static ngx_int_t ngx_http_set_response_header(ngx_http_request_t * r, ngx_http_header_val_t * hv, ngx_str_t * value);

static void * ngx_http_headers_create_conf(ngx_conf_t * cf);
static char * ngx_http_headers_merge_conf(ngx_conf_t * cf, void * parent, void * child);
static ngx_int_t ngx_http_headers_filter_init(ngx_conf_t * cf);
static const char * ngx_http_headers_expires(ngx_conf_t * cf, const ngx_command_t * cmd, void * conf); // F_SetHandler
static const char * ngx_http_headers_add(ngx_conf_t * cf, const ngx_command_t * cmd, void * conf); // F_SetHandler

static ngx_http_set_header_t ngx_http_set_headers[] = {
	{ ngx_string("Cache-Control"), 0, ngx_http_add_cache_control },
	{ ngx_string("Last-Modified"), offsetof(ngx_http_headers_out_t, last_modified), ngx_http_set_last_modified },
	{ ngx_string("ETag"), offsetof(ngx_http_headers_out_t, etag), ngx_http_set_response_header },
	{ ngx_null_string, 0, NULL }
};

static ngx_command_t ngx_http_headers_filter_commands[] = {
	{ ngx_string("expires"), NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_HTTP_LIF_CONF|NGX_CONF_TAKE12, ngx_http_headers_expires, NGX_HTTP_LOC_CONF_OFFSET, 0, NULL },
	{ ngx_string("add_header"), NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_HTTP_LIF_CONF|NGX_CONF_TAKE23, ngx_http_headers_add, NGX_HTTP_LOC_CONF_OFFSET, offsetof(ngx_http_headers_conf_t, headers), NULL },
	{ ngx_string("add_trailer"), NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_HTTP_LIF_CONF|NGX_CONF_TAKE23, ngx_http_headers_add, NGX_HTTP_LOC_CONF_OFFSET, offsetof(ngx_http_headers_conf_t, trailers), NULL },
	ngx_null_command
};

static ngx_http_module_t ngx_http_headers_filter_module_ctx = {
	NULL,                              /* preconfiguration */
	ngx_http_headers_filter_init,      /* postconfiguration */
	NULL,                              /* create main configuration */
	NULL,                              /* init main configuration */
	NULL,                              /* create server configuration */
	NULL,                              /* merge server configuration */
	ngx_http_headers_create_conf,      /* create location configuration */
	ngx_http_headers_merge_conf        /* merge location configuration */
};

ngx_module_t ngx_http_headers_filter_module = {
	NGX_MODULE_V1,
	&ngx_http_headers_filter_module_ctx, /* module context */
	ngx_http_headers_filter_commands,  /* module directives */
	NGX_HTTP_MODULE,                   /* module type */
	NULL,                              /* init master */
	NULL,                              /* init module */
	NULL,                              /* init process */
	NULL,                              /* init thread */
	NULL,                              /* exit thread */
	NULL,                              /* exit process */
	NULL,                              /* exit master */
	NGX_MODULE_V1_PADDING
};

static ngx_http_output_header_filter_pt ngx_http_next_header_filter;
static ngx_http_output_body_filter_pt ngx_http_next_body_filter;

static ngx_int_t ngx_http_headers_filter(ngx_http_request_t * pReq)
{
	ngx_int_t result = NGX_OK;
	if(pReq == pReq->main) {
		ngx_http_headers_conf_t * conf = (ngx_http_headers_conf_t *)ngx_http_get_module_loc_conf(pReq, ngx_http_headers_filter_module);
		if(conf->expires != NGX_HTTP_EXPIRES_OFF || conf->headers || conf->trailers) {
			ngx_uint_t safe_status = 0;
			switch(pReq->headers_out.status) {
				case NGX_HTTP_OK:
				case NGX_HTTP_CREATED:
				case NGX_HTTP_NO_CONTENT:
				case NGX_HTTP_PARTIAL_CONTENT:
				case NGX_HTTP_MOVED_PERMANENTLY:
				case NGX_HTTP_MOVED_TEMPORARILY:
				case NGX_HTTP_SEE_OTHER:
				case NGX_HTTP_NOT_MODIFIED:
				case NGX_HTTP_TEMPORARY_REDIRECT:
				case NGX_HTTP_PERMANENT_REDIRECT:
					safe_status = 1;
					break;
			}
			if(conf->expires != NGX_HTTP_EXPIRES_OFF && safe_status) {
				THROW(ngx_http_set_expires(pReq, conf) == NGX_OK);
			}
			if(conf->headers) {
				ngx_http_header_val_t * h = (ngx_http_header_val_t *)conf->headers->elts;
				for(ngx_uint_t i = 0; i < conf->headers->nelts; i++) {
					if(safe_status || h[i].always) {
						ngx_str_t value;
						THROW(ngx_http_complex_value(pReq, &h[i].value, &value) == NGX_OK);
						THROW(h[i].handler(pReq, &h[i], &value) == NGX_OK);
					}
				}
			}
			if(conf->trailers) {
				ngx_http_header_val_t * h = (ngx_http_header_val_t *)conf->trailers->elts;
				for(ngx_uint_t i = 0; i < conf->trailers->nelts; i++) {
					if(safe_status || h[i].always) {
						pReq->expect_trailers = 1;
						break;
					}
				}
			}
		}
	}
	result = ngx_http_next_header_filter(pReq);
	CATCH
		result = NGX_ERROR;
	ENDCATCH
	return result;
}

static ngx_int_t ngx_http_trailers_filter(ngx_http_request_t * r, ngx_chain_t * in)
{
	ngx_str_t value;
	ngx_uint_t i, safe_status;
	ngx_chain_t    * cl;
	ngx_table_elt_t   * t;
	ngx_http_header_val_t  * h;
	ngx_http_headers_conf_t  * conf = (ngx_http_headers_conf_t *)ngx_http_get_module_loc_conf(r, ngx_http_headers_filter_module);
	if(in == NULL || conf->trailers == NULL || !r->expect_trailers || r->header_only) {
		return ngx_http_next_body_filter(r, in);
	}
	for(cl = in; cl; cl = cl->next) {
		if(cl->buf->last_buf) {
			break;
		}
	}
	if(cl == NULL) {
		return ngx_http_next_body_filter(r, in);
	}
	switch(r->headers_out.status) {
		case NGX_HTTP_OK:
		case NGX_HTTP_CREATED:
		case NGX_HTTP_NO_CONTENT:
		case NGX_HTTP_PARTIAL_CONTENT:
		case NGX_HTTP_MOVED_PERMANENTLY:
		case NGX_HTTP_MOVED_TEMPORARILY:
		case NGX_HTTP_SEE_OTHER:
		case NGX_HTTP_NOT_MODIFIED:
		case NGX_HTTP_TEMPORARY_REDIRECT:
		case NGX_HTTP_PERMANENT_REDIRECT:
		    safe_status = 1;
		    break;
		default:
		    safe_status = 0;
		    break;
	}
	h = (ngx_http_header_val_t *)conf->trailers->elts;
	for(i = 0; i < conf->trailers->nelts; i++) {
		if(!safe_status && !h[i].always) {
			continue;
		}
		if(ngx_http_complex_value(r, &h[i].value, &value) != NGX_OK) {
			return NGX_ERROR;
		}
		if(value.len) {
			t = (ngx_table_elt_t*)ngx_list_push(&r->headers_out.trailers);
			if(t == NULL) {
				return NGX_ERROR;
			}
			t->key = h[i].key;
			t->value = value;
			t->hash = 1;
		}
	}
	return ngx_http_next_body_filter(r, in);
}

static ngx_int_t ngx_http_set_expires(ngx_http_request_t * r, ngx_http_headers_conf_t * conf)
{
	const char * err;
	size_t len;
	time_t now, max_age;
	ngx_str_t value;
	ngx_int_t rc;
	ngx_uint_t i;
	ngx_table_elt_t   * e, * cc, ** ccp;
	ngx_http_expires_t expires = conf->expires;
	time_t expires_time = conf->expires_time;
	if(conf->expires_value != NULL) {
		if(ngx_http_complex_value(r, conf->expires_value, &value) != NGX_OK) {
			return NGX_ERROR;
		}
		rc = ngx_http_parse_expires(&value, &expires, &expires_time, &err);
		if(rc != NGX_OK) {
			return NGX_OK;
		}
		if(expires == NGX_HTTP_EXPIRES_OFF) {
			return NGX_OK;
		}
	}
	e = r->headers_out.expires;
	if(!e) {
		e = (ngx_table_elt_t*)ngx_list_push(&r->headers_out.headers);
		if(!e) {
			return NGX_ERROR;
		}
		r->headers_out.expires = e;
		e->hash = 1;
		ngx_str_set(&e->key, "Expires");
	}
	len = sizeof("Mon, 28 Sep 1970 06:00:00 GMT");
	e->value.len = len - 1;
	ccp = (ngx_table_elt_t**)r->headers_out.cache_control.elts;
	if(ccp == NULL) {
		if(ngx_array_init(&r->headers_out.cache_control, r->pool, 1, sizeof(ngx_table_elt_t *)) != NGX_OK) {
			return NGX_ERROR;
		}
		cc = (ngx_table_elt_t*)ngx_list_push(&r->headers_out.headers);
		if(cc == NULL) {
			return NGX_ERROR;
		}
		cc->hash = 1;
		ngx_str_set(&cc->key, "Cache-Control");
		ccp = (ngx_table_elt_t**)ngx_array_push(&r->headers_out.cache_control);
		if(ccp == NULL) {
			return NGX_ERROR;
		}
		*ccp = cc;
	}
	else {
		for(i = 1; i < r->headers_out.cache_control.nelts; i++) {
			ccp[i]->hash = 0;
		}
		cc = ccp[0];
	}
	if(expires == NGX_HTTP_EXPIRES_EPOCH) {
		e->value.data = (u_char *)"Thu, 01 Jan 1970 00:00:01 GMT";
		ngx_str_set(&cc->value, "no-cache");
		return NGX_OK;
	}
	if(expires == NGX_HTTP_EXPIRES_MAX) {
		e->value.data = (u_char *)"Thu, 31 Dec 2037 23:55:55 GMT";
		/* 10 years */
		ngx_str_set(&cc->value, "max-age=315360000");
		return NGX_OK;
	}
	e->value.data = static_cast<u_char *>(ngx_pnalloc(r->pool, len));
	if(e->value.data == NULL) {
		return NGX_ERROR;
	}
	if(expires_time == 0 && expires != NGX_HTTP_EXPIRES_DAILY) {
		memcpy(e->value.data, ngx_cached_http_time.data, ngx_cached_http_time.len + 1);
		ngx_str_set(&cc->value, "max-age=0");
		return NGX_OK;
	}
	now = ngx_time();
	if(expires == NGX_HTTP_EXPIRES_DAILY) {
		expires_time = ngx_next_time(expires_time);
		max_age = expires_time - now;
	}
	else if(expires == NGX_HTTP_EXPIRES_ACCESS || r->headers_out.last_modified_time == -1) {
		max_age = expires_time;
		expires_time += now;
	}
	else {
		expires_time += r->headers_out.last_modified_time;
		max_age = expires_time - now;
	}
	ngx_http_time(e->value.data, expires_time);
	if(conf->expires_time < 0 || max_age < 0) {
		ngx_str_set(&cc->value, "no-cache");
		return NGX_OK;
	}
	cc->value.data = (u_char *)ngx_pnalloc(r->pool, sizeof("max-age=") + NGX_TIME_T_LEN + 1);
	if(cc->value.data == NULL) {
		return NGX_ERROR;
	}
	cc->value.len = ngx_sprintf(cc->value.data, "max-age=%T", max_age) - cc->value.data;
	return NGX_OK;
}

static ngx_int_t ngx_http_parse_expires(ngx_str_t * value, ngx_http_expires_t * expires, time_t * expires_time, const char ** err)
{
	ngx_uint_t minus;
	if(*expires != NGX_HTTP_EXPIRES_MODIFIED) {
		if(value->len == 5 && ngx_strncmp(value->data, "epoch", 5) == 0) {
			*expires = NGX_HTTP_EXPIRES_EPOCH;
			return NGX_OK;
		}
		if(value->len == 3 && ngx_strncmp(value->data, "max", 3) == 0) {
			*expires = NGX_HTTP_EXPIRES_MAX;
			return NGX_OK;
		}
		if(value->len == 3 && ngx_strncmp(value->data, "off", 3) == 0) {
			*expires = NGX_HTTP_EXPIRES_OFF;
			return NGX_OK;
		}
	}
	if(value->len && value->data[0] == '@') {
		value->data++;
		value->len--;
		minus = 0;
		if(*expires == NGX_HTTP_EXPIRES_MODIFIED) {
			*err = "daily time cannot be used with \"modified\" parameter";
			return NGX_ERROR;
		}
		*expires = NGX_HTTP_EXPIRES_DAILY;
	}
	else if(value->len && value->data[0] == '+') {
		value->data++;
		value->len--;
		minus = 0;
	}
	else if(value->len && value->data[0] == '-') {
		value->data++;
		value->len--;
		minus = 1;
	}
	else {
		minus = 0;
	}

	*expires_time = ngx_parse_time(value, 1);

	if(*expires_time == (time_t)NGX_ERROR) {
		*err = "invalid value";
		return NGX_ERROR;
	}
	if(*expires == NGX_HTTP_EXPIRES_DAILY && *expires_time > 24 * 60 * 60) {
		*err = "daily time value must be less than 24 hours";
		return NGX_ERROR;
	}
	if(minus) {
		*expires_time = -*expires_time;
	}
	return NGX_OK;
}

static ngx_int_t ngx_http_add_header(ngx_http_request_t * r, ngx_http_header_val_t * hv, ngx_str_t * value)
{
	ngx_table_elt_t  * h;
	if(value->len) {
		h = (ngx_table_elt_t*)ngx_list_push(&r->headers_out.headers);
		if(h == NULL) {
			return NGX_ERROR;
		}
		h->hash = 1;
		h->key = hv->key;
		h->value = *value;
	}
	return NGX_OK;
}

static ngx_int_t ngx_http_add_cache_control(ngx_http_request_t * r, ngx_http_header_val_t * hv,
    ngx_str_t * value)
{
	ngx_table_elt_t  * cc, ** ccp;

	if(value->len == 0) {
		return NGX_OK;
	}

	ccp = (ngx_table_elt_t**)r->headers_out.cache_control.elts;

	if(ccp == NULL) {
		if(ngx_array_init(&r->headers_out.cache_control, r->pool,
			    1, sizeof(ngx_table_elt_t *))
		    != NGX_OK) {
			return NGX_ERROR;
		}
	}

	cc = (ngx_table_elt_t*)ngx_list_push(&r->headers_out.headers);
	if(cc == NULL) {
		return NGX_ERROR;
	}

	cc->hash = 1;
	ngx_str_set(&cc->key, "Cache-Control");
	cc->value = *value;

	ccp = (ngx_table_elt_t**)ngx_array_push(&r->headers_out.cache_control);
	if(ccp == NULL) {
		return NGX_ERROR;
	}

	*ccp = cc;

	return NGX_OK;
}

static ngx_int_t ngx_http_set_last_modified(ngx_http_request_t * r, ngx_http_header_val_t * hv, ngx_str_t * value)
{
	if(ngx_http_set_response_header(r, hv, value) != NGX_OK) {
		return NGX_ERROR;
	}
	r->headers_out.last_modified_time = (value->len) ? ngx_parse_http_time(value->data, value->len) : -1;
	return NGX_OK;
}

static ngx_int_t ngx_http_set_response_header(ngx_http_request_t * r, ngx_http_header_val_t * hv, ngx_str_t * value)
{
	ngx_table_elt_t * h;
	ngx_table_elt_t ** old = reinterpret_cast<ngx_table_elt_t**>((char *)&r->headers_out + hv->offset);
	if(value->len == 0) {
		if(*old) {
			(*old)->hash = 0;
			*old = NULL;
		}
		return NGX_OK;
	}
	if(*old) {
		h = *old;
	}
	else {
		h = (ngx_table_elt_t*)ngx_list_push(&r->headers_out.headers);
		if(h == NULL) {
			return NGX_ERROR;
		}
		*old = h;
	}
	h->hash = 1;
	h->key = hv->key;
	h->value = *value;
	return NGX_OK;
}

static void * ngx_http_headers_create_conf(ngx_conf_t * cf)
{
	ngx_http_headers_conf_t  * conf = (ngx_http_headers_conf_t *)ngx_pcalloc(cf->pool, sizeof(ngx_http_headers_conf_t));
	if(conf) {
		/*
		 * set by ngx_pcalloc():
		 *
		 *   conf->headers = NULL;
		 *   conf->trailers = NULL;
		 *   conf->expires_time = 0;
		 *   conf->expires_value = NULL;
		 */
		conf->expires = NGX_HTTP_EXPIRES_UNSET;
	}
	return conf;
}

static char * ngx_http_headers_merge_conf(ngx_conf_t * cf, void * parent, void * child)
{
	ngx_http_headers_conf_t * prev = static_cast<ngx_http_headers_conf_t *>(parent);
	ngx_http_headers_conf_t * conf = static_cast<ngx_http_headers_conf_t *>(child);
	if(conf->expires == NGX_HTTP_EXPIRES_UNSET) {
		conf->expires = prev->expires;
		conf->expires_time = prev->expires_time;
		conf->expires_value = prev->expires_value;
		if(conf->expires == NGX_HTTP_EXPIRES_UNSET) {
			conf->expires = NGX_HTTP_EXPIRES_OFF;
		}
	}
	if(conf->headers == NULL) {
		conf->headers = prev->headers;
	}
	if(conf->trailers == NULL) {
		conf->trailers = prev->trailers;
	}
	return NGX_CONF_OK;
}

static ngx_int_t ngx_http_headers_filter_init(ngx_conf_t * cf)
{
	ngx_http_next_header_filter = ngx_http_top_header_filter;
	ngx_http_top_header_filter = ngx_http_headers_filter;
	ngx_http_next_body_filter = ngx_http_top_body_filter;
	ngx_http_top_body_filter = ngx_http_trailers_filter;
	return NGX_OK;
}

static const char * ngx_http_headers_expires(ngx_conf_t * cf, const ngx_command_t * cmd, void * conf) // F_SetHandler
{
	ngx_http_headers_conf_t * hcf = (ngx_http_headers_conf_t *)conf;
	const char * err;
	ngx_str_t  * value;
	ngx_int_t rc;
	ngx_uint_t n;
	ngx_http_complex_value_t cv;
	ngx_http_compile_complex_value_t ccv;
	if(hcf->expires != NGX_HTTP_EXPIRES_UNSET) {
		return "is duplicate";
	}
	value = static_cast<ngx_str_t *>(cf->args->elts);
	if(cf->args->nelts == 2) {
		hcf->expires = NGX_HTTP_EXPIRES_ACCESS;

		n = 1;
	}
	else { /* cf->args->nelts == 3 */
		if(ngx_strcmp(value[1].data, "modified") != 0) {
			return "invalid value";
		}
		hcf->expires = NGX_HTTP_EXPIRES_MODIFIED;
		n = 2;
	}
	memzero(&ccv, sizeof(ngx_http_compile_complex_value_t));
	ccv.cf = cf;
	ccv.value = &value[n];
	ccv.complex_value = &cv;
	if(ngx_http_compile_complex_value(&ccv) != NGX_OK) {
		return NGX_CONF_ERROR;
	}
	if(cv.lengths != NULL) {
		hcf->expires_value = (ngx_http_complex_value_t*)ngx_palloc(cf->pool, sizeof(ngx_http_complex_value_t));
		if(hcf->expires_value == NULL) {
			return NGX_CONF_ERROR;
		}
		*hcf->expires_value = cv;
		return NGX_CONF_OK;
	}
	rc = ngx_http_parse_expires(&value[n], &hcf->expires, &hcf->expires_time, &err);
	if(rc != NGX_OK) {
		return err;
	}
	return NGX_CONF_OK;
}

static const char * ngx_http_headers_add(ngx_conf_t * cf, const ngx_command_t * cmd, void * conf) // F_SetHandler
{
	ngx_http_headers_conf_t * hcf = (ngx_http_headers_conf_t *)conf;
	ngx_uint_t i;
	ngx_http_header_val_t    * hv;
	ngx_http_set_header_t    * set;
	ngx_http_compile_complex_value_t ccv;
	ngx_str_t * value = static_cast<ngx_str_t *>(cf->args->elts);
	ngx_array_t ** headers = (ngx_array_t**)((char *)hcf + cmd->offset);
	if(*headers == NULL) {
		*headers = ngx_array_create(cf->pool, 1, sizeof(ngx_http_header_val_t));
		if(*headers == NULL) {
			return NGX_CONF_ERROR;
		}
	}
	hv = (ngx_http_header_val_t *)ngx_array_push(*headers);
	if(hv == NULL) {
		return NGX_CONF_ERROR;
	}
	hv->key = value[1];
	hv->handler = NULL;
	hv->offset = 0;
	hv->always = 0;
	if(headers == &hcf->headers) {
		hv->handler = ngx_http_add_header;
		set = ngx_http_set_headers;
		for(i = 0; set[i].name.len; i++) {
			if(sstreqi_ascii(value[1].data, set[i].name.data)) {
				hv->offset = set[i].offset;
				hv->handler = set[i].handler;
				break;
			}
		}
	}
	if(value[2].len == 0) {
		memzero(&hv->value, sizeof(ngx_http_complex_value_t));
	}
	else {
		memzero(&ccv, sizeof(ngx_http_compile_complex_value_t));
		ccv.cf = cf;
		ccv.value = &value[2];
		ccv.complex_value = &hv->value;
		if(ngx_http_compile_complex_value(&ccv) != NGX_OK) {
			return NGX_CONF_ERROR;
		}
	}
	if(cf->args->nelts == 3) {
		return NGX_CONF_OK;
	}
	if(ngx_strcmp(value[3].data, "always") != 0) {
		ngx_conf_log_error(NGX_LOG_EMERG, cf, 0, "invalid parameter \"%V\"", &value[3]);
		return NGX_CONF_ERROR;
	}
	hv->always = 1;
	return NGX_CONF_OK;
}
