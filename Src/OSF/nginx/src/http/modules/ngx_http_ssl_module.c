/*
 * Copyright (C) Igor Sysoev
 * Copyright (C) Nginx, Inc.
 */
#include <ngx_config.h>
#include <ngx_core.h>
#pragma hdrstop

typedef ngx_int_t (*ngx_ssl_variable_handler_pt)(ngx_connection_t * c, ngx_pool_t * pool, ngx_str_t * s);

#define NGX_DEFAULT_CIPHERS     "HIGH:!aNULL:!MD5"
#define NGX_DEFAULT_ECDH_CURVE  "auto"

#define NGX_HTTP_NPN_ADVERTISE  "\x08http/1.1"

#ifdef TLSEXT_TYPE_application_layer_protocol_negotiation
static int ngx_http_ssl_alpn_select(ngx_ssl_conn_t * ssl_conn,
    const uchar ** out, uchar * outlen,
    const uchar * in, unsigned int inlen, void * arg);
#endif
#ifdef TLSEXT_TYPE_next_proto_neg
	static int ngx_http_ssl_npn_advertised(ngx_ssl_conn_t * ssl_conn, const uchar ** out, unsigned int * outlen, void * arg);
#endif
static ngx_int_t ngx_http_ssl_static_variable(ngx_http_request_t * r, ngx_http_variable_value_t * v, uintptr_t data);
static ngx_int_t ngx_http_ssl_variable(ngx_http_request_t * r, ngx_http_variable_value_t * v, uintptr_t data);
static ngx_int_t ngx_http_ssl_add_variables(ngx_conf_t * cf);
static void * ngx_http_ssl_create_srv_conf(ngx_conf_t * cf);
static char * ngx_http_ssl_merge_srv_conf(ngx_conf_t * cf, void * parent, void * child);
static const char * ngx_http_ssl_enable(ngx_conf_t * cf, const ngx_command_t * cmd, void * conf); // F_SetHandler
static const char * ngx_http_ssl_password_file(ngx_conf_t * cf, const ngx_command_t * cmd, void * conf); // F_SetHandler
static const char * ngx_http_ssl_session_cache(ngx_conf_t * cf, const ngx_command_t * cmd, void * conf); // F_SetHandler

static ngx_int_t ngx_http_ssl_init(ngx_conf_t * cf);

static ngx_conf_bitmask_t ngx_http_ssl_protocols[] = {
	{ ngx_string("SSLv2"), NGX_SSL_SSLv2 },
	{ ngx_string("SSLv3"), NGX_SSL_SSLv3 },
	{ ngx_string("TLSv1"), NGX_SSL_TLSv1 },
	{ ngx_string("TLSv1.1"), NGX_SSL_TLSv1_1 },
	{ ngx_string("TLSv1.2"), NGX_SSL_TLSv1_2 },
	{ ngx_string("TLSv1.3"), NGX_SSL_TLSv1_3 },
	{ ngx_null_string, 0 }
};

static ngx_conf_enum_t ngx_http_ssl_verify[] = {
	{ ngx_string("off"), 0 },
	{ ngx_string("on"), 1 },
	{ ngx_string("optional"), 2 },
	{ ngx_string("optional_no_ca"), 3 },
	{ ngx_null_string, 0 }
};

static ngx_command_t ngx_http_ssl_commands[] = {
	{ ngx_string("ssl"), NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_CONF_FLAG,
	  ngx_http_ssl_enable, NGX_HTTP_SRV_CONF_OFFSET, offsetof(ngx_http_ssl_srv_conf_t, enable), NULL },
	{ ngx_string("ssl_certificate"), NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_CONF_TAKE1,
	  ngx_conf_set_str_array_slot, NGX_HTTP_SRV_CONF_OFFSET, offsetof(ngx_http_ssl_srv_conf_t, certificates), NULL },
	{ ngx_string("ssl_certificate_key"), NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_CONF_TAKE1,
	  ngx_conf_set_str_array_slot, NGX_HTTP_SRV_CONF_OFFSET, offsetof(ngx_http_ssl_srv_conf_t, certificate_keys), NULL },
	{ ngx_string("ssl_password_file"), NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_CONF_TAKE1,
	  ngx_http_ssl_password_file, NGX_HTTP_SRV_CONF_OFFSET, 0, NULL },
	{ ngx_string("ssl_dhparam"), NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_CONF_TAKE1,
	  ngx_conf_set_str_slot, NGX_HTTP_SRV_CONF_OFFSET, offsetof(ngx_http_ssl_srv_conf_t, dhparam), NULL },
	{ ngx_string("ssl_ecdh_curve"),
	  NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_CONF_TAKE1,
	  ngx_conf_set_str_slot,
	  NGX_HTTP_SRV_CONF_OFFSET,
	  offsetof(ngx_http_ssl_srv_conf_t, ecdh_curve),
	  NULL },

	{ ngx_string("ssl_protocols"),
	  NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_CONF_1MORE,
	  ngx_conf_set_bitmask_slot,
	  NGX_HTTP_SRV_CONF_OFFSET,
	  offsetof(ngx_http_ssl_srv_conf_t, protocols),
	  &ngx_http_ssl_protocols },

	{ ngx_string("ssl_ciphers"),
	  NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_CONF_TAKE1,
	  ngx_conf_set_str_slot,
	  NGX_HTTP_SRV_CONF_OFFSET,
	  offsetof(ngx_http_ssl_srv_conf_t, ciphers),
	  NULL },

	{ ngx_string("ssl_buffer_size"),
	  NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_CONF_TAKE1,
	  ngx_conf_set_size_slot,
	  NGX_HTTP_SRV_CONF_OFFSET,
	  offsetof(ngx_http_ssl_srv_conf_t, buffer_size),
	  NULL },

	{ ngx_string("ssl_verify_client"),
	  NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_CONF_TAKE1,
	  ngx_conf_set_enum_slot,
	  NGX_HTTP_SRV_CONF_OFFSET,
	  offsetof(ngx_http_ssl_srv_conf_t, verify),
	  &ngx_http_ssl_verify },

	{ ngx_string("ssl_verify_depth"),
	  NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_CONF_TAKE1,
	  ngx_conf_set_num_slot,
	  NGX_HTTP_SRV_CONF_OFFSET,
	  offsetof(ngx_http_ssl_srv_conf_t, verify_depth),
	  NULL },

	{ ngx_string("ssl_client_certificate"),
	  NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_CONF_TAKE1,
	  ngx_conf_set_str_slot,
	  NGX_HTTP_SRV_CONF_OFFSET,
	  offsetof(ngx_http_ssl_srv_conf_t, client_certificate),
	  NULL },

	{ ngx_string("ssl_trusted_certificate"),
	  NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_CONF_TAKE1,
	  ngx_conf_set_str_slot,
	  NGX_HTTP_SRV_CONF_OFFSET,
	  offsetof(ngx_http_ssl_srv_conf_t, trusted_certificate),
	  NULL },

	{ ngx_string("ssl_prefer_server_ciphers"), NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_CONF_FLAG,
	  ngx_conf_set_flag_slot, NGX_HTTP_SRV_CONF_OFFSET, offsetof(ngx_http_ssl_srv_conf_t, prefer_server_ciphers), NULL },
	{ ngx_string("ssl_session_cache"), NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_CONF_TAKE12,
	  ngx_http_ssl_session_cache, NGX_HTTP_SRV_CONF_OFFSET, 0, NULL },
	{ ngx_string("ssl_session_tickets"), NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_CONF_FLAG,
	  ngx_conf_set_flag_slot, NGX_HTTP_SRV_CONF_OFFSET, offsetof(ngx_http_ssl_srv_conf_t, session_tickets), NULL },

	{ ngx_string("ssl_session_ticket_key"),
	  NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_CONF_TAKE1,
	  ngx_conf_set_str_array_slot,
	  NGX_HTTP_SRV_CONF_OFFSET,
	  offsetof(ngx_http_ssl_srv_conf_t, session_ticket_keys),
	  NULL },

	{ ngx_string("ssl_session_timeout"),
	  NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_CONF_TAKE1,
	  ngx_conf_set_sec_slot,
	  NGX_HTTP_SRV_CONF_OFFSET,
	  offsetof(ngx_http_ssl_srv_conf_t, session_timeout),
	  NULL },

	{ ngx_string("ssl_crl"),
	  NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_CONF_TAKE1,
	  ngx_conf_set_str_slot,
	  NGX_HTTP_SRV_CONF_OFFSET,
	  offsetof(ngx_http_ssl_srv_conf_t, crl),
	  NULL },

	{ ngx_string("ssl_stapling"),
	  NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_CONF_FLAG,
	  ngx_conf_set_flag_slot,
	  NGX_HTTP_SRV_CONF_OFFSET,
	  offsetof(ngx_http_ssl_srv_conf_t, stapling),
	  NULL },

	{ ngx_string("ssl_stapling_file"), NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_CONF_TAKE1,
	  ngx_conf_set_str_slot, NGX_HTTP_SRV_CONF_OFFSET, offsetof(ngx_http_ssl_srv_conf_t, stapling_file), NULL },
	{ ngx_string("ssl_stapling_responder"), NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_CONF_TAKE1,
	  ngx_conf_set_str_slot, NGX_HTTP_SRV_CONF_OFFSET, offsetof(ngx_http_ssl_srv_conf_t, stapling_responder), NULL },
	{ ngx_string("ssl_stapling_verify"), NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_CONF_FLAG,
	  ngx_conf_set_flag_slot, NGX_HTTP_SRV_CONF_OFFSET, offsetof(ngx_http_ssl_srv_conf_t, stapling_verify), NULL },
	ngx_null_command
};

static ngx_http_module_t ngx_http_ssl_module_ctx = {
	ngx_http_ssl_add_variables,        /* preconfiguration */
	ngx_http_ssl_init,                 /* postconfiguration */

	NULL,                              /* create main configuration */
	NULL,                              /* init main configuration */

	ngx_http_ssl_create_srv_conf,      /* create server configuration */
	ngx_http_ssl_merge_srv_conf,       /* merge server configuration */

	NULL,                              /* create location configuration */
	NULL                               /* merge location configuration */
};

ngx_module_t ngx_http_ssl_module = {
	NGX_MODULE_V1,
	&ngx_http_ssl_module_ctx,          /* module context */
	ngx_http_ssl_commands,             /* module directives */
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

static ngx_http_variable_t ngx_http_ssl_vars[] = {
	{ ngx_string("ssl_protocol"), NULL, ngx_http_ssl_static_variable, (uintptr_t)ngx_ssl_get_protocol, NGX_HTTP_VAR_CHANGEABLE, 0 },
	{ ngx_string("ssl_cipher"), NULL, ngx_http_ssl_static_variable, (uintptr_t)ngx_ssl_get_cipher_name, NGX_HTTP_VAR_CHANGEABLE, 0 },
	{ ngx_string("ssl_ciphers"), NULL, ngx_http_ssl_variable, (uintptr_t)ngx_ssl_get_ciphers, NGX_HTTP_VAR_CHANGEABLE, 0 },
	{ ngx_string("ssl_curves"), NULL, ngx_http_ssl_variable, (uintptr_t)ngx_ssl_get_curves, NGX_HTTP_VAR_CHANGEABLE, 0 },
	{ ngx_string("ssl_session_id"), NULL, ngx_http_ssl_variable, (uintptr_t)ngx_ssl_get_session_id, NGX_HTTP_VAR_CHANGEABLE, 0 },
	{ ngx_string("ssl_session_reused"), NULL, ngx_http_ssl_variable, (uintptr_t)ngx_ssl_get_session_reused, NGX_HTTP_VAR_CHANGEABLE, 0 },
	{ ngx_string("ssl_server_name"), NULL, ngx_http_ssl_variable, (uintptr_t)ngx_ssl_get_server_name, NGX_HTTP_VAR_CHANGEABLE, 0 },
	{ ngx_string("ssl_client_cert"), NULL, ngx_http_ssl_variable, (uintptr_t)ngx_ssl_get_certificate, NGX_HTTP_VAR_CHANGEABLE, 0 },
	{ ngx_string("ssl_client_raw_cert"), NULL, ngx_http_ssl_variable, (uintptr_t)ngx_ssl_get_raw_certificate, NGX_HTTP_VAR_CHANGEABLE, 0 },
	{ ngx_string("ssl_client_s_dn"), NULL, ngx_http_ssl_variable, (uintptr_t)ngx_ssl_get_subject_dn, NGX_HTTP_VAR_CHANGEABLE, 0 },
	{ ngx_string("ssl_client_i_dn"), NULL, ngx_http_ssl_variable, (uintptr_t)ngx_ssl_get_issuer_dn, NGX_HTTP_VAR_CHANGEABLE, 0 },
	{ ngx_string("ssl_client_s_dn_legacy"), NULL, ngx_http_ssl_variable, (uintptr_t)ngx_ssl_get_subject_dn_legacy, NGX_HTTP_VAR_CHANGEABLE, 0 },
	{ ngx_string("ssl_client_i_dn_legacy"), NULL, ngx_http_ssl_variable, (uintptr_t)ngx_ssl_get_issuer_dn_legacy, NGX_HTTP_VAR_CHANGEABLE, 0 },
	{ ngx_string("ssl_client_serial"), NULL, ngx_http_ssl_variable, (uintptr_t)ngx_ssl_get_serial_number, NGX_HTTP_VAR_CHANGEABLE, 0 },
	{ ngx_string("ssl_client_fingerprint"), NULL, ngx_http_ssl_variable, (uintptr_t)ngx_ssl_get_fingerprint, NGX_HTTP_VAR_CHANGEABLE, 0 },
	{ ngx_string("ssl_client_verify"), NULL, ngx_http_ssl_variable, (uintptr_t)ngx_ssl_get_client_verify, NGX_HTTP_VAR_CHANGEABLE, 0 },
	{ ngx_string("ssl_client_v_start"), NULL, ngx_http_ssl_variable, (uintptr_t)ngx_ssl_get_client_v_start, NGX_HTTP_VAR_CHANGEABLE, 0 },
	{ ngx_string("ssl_client_v_end"), NULL, ngx_http_ssl_variable, (uintptr_t)ngx_ssl_get_client_v_end, NGX_HTTP_VAR_CHANGEABLE, 0 },
	{ ngx_string("ssl_client_v_remain"), NULL, ngx_http_ssl_variable, (uintptr_t)ngx_ssl_get_client_v_remain, NGX_HTTP_VAR_CHANGEABLE, 0 },
	{ ngx_null_string, NULL, NULL, 0, 0, 0 }
};

static ngx_str_t ngx_http_ssl_sess_id_ctx = ngx_string("HTTP");

#ifdef TLSEXT_TYPE_application_layer_protocol_negotiation

static int ngx_http_ssl_alpn_select(ngx_ssl_conn_t * ssl_conn, const uchar ** out,
    uchar * outlen, const uchar * in, unsigned int inlen,
    void * arg)
{
	unsigned int srvlen;
	uchar   * srv;
#if (NGX_DEBUG)
	unsigned int i;
#endif
#if (NGX_HTTP_V2)
	ngx_http_connection_t  * hc;
#endif
#if (NGX_HTTP_V2 || NGX_DEBUG)
	ngx_connection_t  * c;

	c = (ngx_connection_t *)ngx_ssl_get_connection(ssl_conn);
#endif

#if (NGX_DEBUG)
	for(i = 0; i < inlen; i += in[i] + 1) {
		ngx_log_debug2(NGX_LOG_DEBUG_HTTP, c->log, 0, "SSL ALPN supported by client: %*s", (size_t)in[i], &in[i+1]);
	}
#endif
#if (NGX_HTTP_V2)
	hc = (ngx_http_connection_t *)c->data;
	if(hc->addr_conf->http2) {
		srv = (uchar *)NGX_HTTP_V2_ALPN_ADVERTISE NGX_HTTP_NPN_ADVERTISE;
		srvlen = sizeof(NGX_HTTP_V2_ALPN_ADVERTISE NGX_HTTP_NPN_ADVERTISE) - 1;
	}
	else
#endif
	{
		srv = (uchar *)NGX_HTTP_NPN_ADVERTISE;
		srvlen = sizeof(NGX_HTTP_NPN_ADVERTISE) - 1;
	}
	if(SSL_select_next_proto((uchar**)out, outlen, srv, srvlen, in, inlen) != OPENSSL_NPN_NEGOTIATED) {
		return SSL_TLSEXT_ERR_NOACK;
	}
	ngx_log_debug2(NGX_LOG_DEBUG_HTTP, c->log, 0, "SSL ALPN selected: %*s", (size_t)*outlen, *out);
	return SSL_TLSEXT_ERR_OK;
}

#endif

#ifdef TLSEXT_TYPE_next_proto_neg

static int ngx_http_ssl_npn_advertised(ngx_ssl_conn_t * ssl_conn,
    const uchar ** out, unsigned int * outlen, void * arg)
{
#if (NGX_HTTP_V2 || NGX_DEBUG)
	ngx_connection_t  * c;

	c = (ngx_connection_t *)ngx_ssl_get_connection(ssl_conn);
	ngx_log_debug0(NGX_LOG_DEBUG_HTTP, c->log, 0, "SSL NPN advertised");
#endif

#if (NGX_HTTP_V2)
	{
		ngx_http_connection_t  * hc;

		hc = (ngx_http_connection_t *)c->data;

		if(hc->addr_conf->http2) {
			*out =
			    (uchar *)NGX_HTTP_V2_NPN_ADVERTISE NGX_HTTP_NPN_ADVERTISE;
			*outlen = sizeof(NGX_HTTP_V2_NPN_ADVERTISE NGX_HTTP_NPN_ADVERTISE) - 1;

			return SSL_TLSEXT_ERR_OK;
		}
	}
#endif

	*out = (uchar *)NGX_HTTP_NPN_ADVERTISE;
	*outlen = sizeof(NGX_HTTP_NPN_ADVERTISE) - 1;

	return SSL_TLSEXT_ERR_OK;
}

#endif

static ngx_int_t ngx_http_ssl_static_variable(ngx_http_request_t * r, ngx_http_variable_value_t * v, uintptr_t data)
{
	ngx_ssl_variable_handler_pt handler = (ngx_ssl_variable_handler_pt)data;
	size_t len;
	ngx_str_t s;
	if(r->connection->ssl) {
		(void)handler(r->connection, NULL, &s);
		v->data = s.data;
		for(len = 0; v->data[len]; len++) { /* void */
		}
		v->len = len;
		v->valid = 1;
		v->no_cacheable = 0;
		v->not_found = 0;
		return NGX_OK;
	}
	v->not_found = 1;
	return NGX_OK;
}

static ngx_int_t ngx_http_ssl_variable(ngx_http_request_t * r, ngx_http_variable_value_t * v, uintptr_t data)
{
	ngx_ssl_variable_handler_pt handler = (ngx_ssl_variable_handler_pt)data;
	ngx_str_t s;
	if(r->connection->ssl) {
		if(handler(r->connection, r->pool, &s) != NGX_OK) {
			return NGX_ERROR;
		}
		v->len = s.len;
		v->data = s.data;
		if(v->len) {
			v->valid = 1;
			v->no_cacheable = 0;
			v->not_found = 0;
			return NGX_OK;
		}
	}
	v->not_found = 1;
	return NGX_OK;
}

static ngx_int_t ngx_http_ssl_add_variables(ngx_conf_t * cf)
{
	for(ngx_http_variable_t * v = ngx_http_ssl_vars; v->name.len; v++) {
		ngx_http_variable_t * var = ngx_http_add_variable(cf, &v->name, v->flags);
		if(var == NULL) {
			return NGX_ERROR;
		}
		var->get_handler = v->get_handler;
		var->data = v->data;
	}
	return NGX_OK;
}

static void * ngx_http_ssl_create_srv_conf(ngx_conf_t * cf)
{
	ngx_http_ssl_srv_conf_t * sscf = (ngx_http_ssl_srv_conf_t *)ngx_pcalloc(cf->pool, sizeof(ngx_http_ssl_srv_conf_t));
	if(sscf) {
		/*
		 * set by ngx_pcalloc():
		 *
		 *   sscf->protocols = 0;
		 *   sscf->dhparam = { 0, NULL };
		 *   sscf->ecdh_curve = { 0, NULL };
		 *   sscf->client_certificate = { 0, NULL };
		 *   sscf->trusted_certificate = { 0, NULL };
		 *   sscf->crl = { 0, NULL };
		 *   sscf->ciphers = { 0, NULL };
		 *   sscf->shm_zone = NULL;
		 *   sscf->stapling_file = { 0, NULL };
		 *   sscf->stapling_responder = { 0, NULL };
		 */
		sscf->enable = NGX_CONF_UNSET;
		sscf->prefer_server_ciphers = NGX_CONF_UNSET;
		sscf->buffer_size = NGX_CONF_UNSET_SIZE;
		sscf->verify = NGX_CONF_UNSET_UINT;
		sscf->verify_depth = NGX_CONF_UNSET_UINT;
		sscf->certificates = (ngx_array_t*)NGX_CONF_UNSET_PTR;
		sscf->certificate_keys = (ngx_array_t*)NGX_CONF_UNSET_PTR;
		sscf->passwords = (ngx_array_t*)NGX_CONF_UNSET_PTR;
		sscf->builtin_session_cache = NGX_CONF_UNSET;
		sscf->session_timeout = NGX_CONF_UNSET;
		sscf->session_tickets = NGX_CONF_UNSET;
		sscf->session_ticket_keys = (ngx_array_t*)NGX_CONF_UNSET_PTR;
		sscf->stapling = NGX_CONF_UNSET;
		sscf->stapling_verify = NGX_CONF_UNSET;
	}
	return sscf;
}

static char * ngx_http_ssl_merge_srv_conf(ngx_conf_t * cf, void * parent, void * child)
{
	ngx_http_ssl_srv_conf_t * prev = (ngx_http_ssl_srv_conf_t *)parent;
	ngx_http_ssl_srv_conf_t * conf = (ngx_http_ssl_srv_conf_t *)child;
	ngx_pool_cleanup_t  * cln;
	if(conf->enable == NGX_CONF_UNSET) {
		if(prev->enable == NGX_CONF_UNSET) {
			conf->enable = 0;
		}
		else {
			conf->enable = prev->enable;
			conf->file = prev->file;
			conf->line = prev->line;
		}
	}
	ngx_conf_merge_value(conf->session_timeout, prev->session_timeout, 300);
	ngx_conf_merge_value(conf->prefer_server_ciphers, prev->prefer_server_ciphers, 0);
	ngx_conf_merge_bitmask_value(conf->protocols, prev->protocols, (NGX_CONF_BITMASK_SET|NGX_SSL_TLSv1|NGX_SSL_TLSv1_1|NGX_SSL_TLSv1_2));
	ngx_conf_merge_size_value(conf->buffer_size, prev->buffer_size, NGX_SSL_BUFSIZE);
	ngx_conf_merge_uint_value(conf->verify, prev->verify, 0);
	ngx_conf_merge_uint_value(conf->verify_depth, prev->verify_depth, 1);
	ngx_conf_merge_ptr_value(conf->certificates, prev->certificates, NULL);
	ngx_conf_merge_ptr_value(conf->certificate_keys, prev->certificate_keys, NULL);
	ngx_conf_merge_ptr_value(conf->passwords, prev->passwords, NULL);
	ngx_conf_merge_str_value(conf->dhparam, prev->dhparam, "");
	ngx_conf_merge_str_value(conf->client_certificate, prev->client_certificate, "");
	ngx_conf_merge_str_value(conf->trusted_certificate, prev->trusted_certificate, "");
	ngx_conf_merge_str_value(conf->crl, prev->crl, "");
	ngx_conf_merge_str_value(conf->ecdh_curve, prev->ecdh_curve, NGX_DEFAULT_ECDH_CURVE);
	ngx_conf_merge_str_value(conf->ciphers, prev->ciphers, NGX_DEFAULT_CIPHERS);
	ngx_conf_merge_value(conf->stapling, prev->stapling, 0);
	ngx_conf_merge_value(conf->stapling_verify, prev->stapling_verify, 0);
	ngx_conf_merge_str_value(conf->stapling_file, prev->stapling_file, "");
	ngx_conf_merge_str_value(conf->stapling_responder, prev->stapling_responder, "");
	conf->ssl.log = cf->log;
	if(conf->enable) {
		if(conf->certificates == NULL) {
			ngx_log_error(NGX_LOG_EMERG, cf->log, 0, "no \"ssl_certificate\" is defined for the \"ssl\" directive in %s:%ui", conf->file, conf->line);
			return NGX_CONF_ERROR;
		}
		if(conf->certificate_keys == NULL) {
			ngx_log_error(NGX_LOG_EMERG, cf->log, 0, "no \"ssl_certificate_key\" is defined for the \"ssl\" directive in %s:%ui", conf->file, conf->line);
			return NGX_CONF_ERROR;
		}
		if(conf->certificate_keys->nelts < conf->certificates->nelts) {
			ngx_log_error(NGX_LOG_EMERG, cf->log, 0, "no \"ssl_certificate_key\" is defined for certificate \"%V\" and the \"ssl\" directive in %s:%ui",
			    ((ngx_str_t *)conf->certificates->elts) + conf->certificates->nelts - 1, conf->file, conf->line);
			return NGX_CONF_ERROR;
		}
	}
	else {
		if(conf->certificates == NULL) {
			return NGX_CONF_OK;
		}
		if(conf->certificate_keys == NULL || conf->certificate_keys->nelts < conf->certificates->nelts) {
			ngx_log_error(NGX_LOG_EMERG, cf->log, 0, "no \"ssl_certificate_key\" is defined for certificate \"%V\"", ((ngx_str_t *)conf->certificates->elts) + conf->certificates->nelts - 1);
			return NGX_CONF_ERROR;
		}
	}
	if(ngx_ssl_create(&conf->ssl, conf->protocols, conf) != NGX_OK) {
		return NGX_CONF_ERROR;
	}
#ifdef SSL_CTRL_SET_TLSEXT_HOSTNAME
	if(SSL_CTX_set_tlsext_servername_callback(conf->ssl.ctx, ngx_http_ssl_servername) == 0) {
		ngx_log_error(NGX_LOG_WARN, cf->log, 0, "nginx was built with SNI support, however, now it is linked dynamically to an OpenSSL library which has no tlsext support, therefore SNI is not available");
	}
#endif
#ifdef TLSEXT_TYPE_application_layer_protocol_negotiation
	SSL_CTX_set_alpn_select_cb(conf->ssl.ctx, ngx_http_ssl_alpn_select, NULL);
#endif
#ifdef TLSEXT_TYPE_next_proto_neg
	SSL_CTX_set_next_protos_advertised_cb(conf->ssl.ctx, ngx_http_ssl_npn_advertised, NULL);
#endif
	cln = ngx_pool_cleanup_add(cf->pool, 0);
	if(cln == NULL) {
		return NGX_CONF_ERROR;
	}
	cln->handler = ngx_ssl_cleanup_ctx;
	cln->data = &conf->ssl;
	if(ngx_ssl_certificates(cf, &conf->ssl, conf->certificates, conf->certificate_keys, conf->passwords) != NGX_OK) {
		return NGX_CONF_ERROR;
	}
	if(ngx_ssl_ciphers(cf, &conf->ssl, &conf->ciphers, conf->prefer_server_ciphers) != NGX_OK) {
		return NGX_CONF_ERROR;
	}
	conf->ssl.buffer_size = conf->buffer_size;
	if(conf->verify) {
		if(conf->client_certificate.len == 0 && conf->verify != 3) {
			ngx_log_error(NGX_LOG_EMERG, cf->log, 0, "no ssl_client_certificate for ssl_client_verify");
			return NGX_CONF_ERROR;
		}
		if(ngx_ssl_client_certificate(cf, &conf->ssl, &conf->client_certificate, conf->verify_depth) != NGX_OK) {
			return NGX_CONF_ERROR;
		}
	}
	if(ngx_ssl_trusted_certificate(cf, &conf->ssl, &conf->trusted_certificate, conf->verify_depth) != NGX_OK) {
		return NGX_CONF_ERROR;
	}
	if(ngx_ssl_crl(cf, &conf->ssl, &conf->crl) != NGX_OK) {
		return NGX_CONF_ERROR;
	}
	if(ngx_ssl_dhparam(cf, &conf->ssl, &conf->dhparam) != NGX_OK) {
		return NGX_CONF_ERROR;
	}
	if(ngx_ssl_ecdh_curve(cf, &conf->ssl, &conf->ecdh_curve) != NGX_OK) {
		return NGX_CONF_ERROR;
	}
	ngx_conf_merge_value(conf->builtin_session_cache, prev->builtin_session_cache, NGX_SSL_NONE_SCACHE);
	SETIFZ(conf->shm_zone, prev->shm_zone);
	if(ngx_ssl_session_cache(&conf->ssl, &ngx_http_ssl_sess_id_ctx, conf->builtin_session_cache, conf->shm_zone, conf->session_timeout) != NGX_OK) {
		return NGX_CONF_ERROR;
	}
	ngx_conf_merge_value(conf->session_tickets, prev->session_tickets, 1);
#ifdef SSL_OP_NO_TICKET
	if(!conf->session_tickets) {
		SSL_CTX_set_options(conf->ssl.ctx, SSL_OP_NO_TICKET);
	}
#endif
	ngx_conf_merge_ptr_value(conf->session_ticket_keys, prev->session_ticket_keys, NULL);
	if(ngx_ssl_session_ticket_keys(cf, &conf->ssl, conf->session_ticket_keys) != NGX_OK) {
		return NGX_CONF_ERROR;
	}
	if(conf->stapling) {
		if(ngx_ssl_stapling(cf, &conf->ssl, &conf->stapling_file, &conf->stapling_responder, conf->stapling_verify) != NGX_OK) {
			return NGX_CONF_ERROR;
		}
	}
	return NGX_CONF_OK;
}

static const char * ngx_http_ssl_enable(ngx_conf_t * cf, const ngx_command_t * cmd, void * conf) // F_SetHandler
{
	ngx_http_ssl_srv_conf_t * sscf = (ngx_http_ssl_srv_conf_t *)conf;
	const char * rv = ngx_conf_set_flag_slot(cf, cmd, conf);
	if(rv == NGX_CONF_OK) {
		sscf->file = cf->conf_file->file.name.data;
		sscf->line = cf->conf_file->line;
	}
	return rv;
}

static const char * ngx_http_ssl_password_file(ngx_conf_t * cf, const ngx_command_t * cmd, void * conf) // F_SetHandler
{
	ngx_http_ssl_srv_conf_t * sscf = (ngx_http_ssl_srv_conf_t *)conf;
	if(sscf->passwords != NGX_CONF_UNSET_PTR) {
		return "is duplicate";
	}
	else {
		ngx_str_t * value = static_cast<ngx_str_t *>(cf->args->elts);
		sscf->passwords = ngx_ssl_read_password_file(cf, &value[1]);
		if(sscf->passwords == NULL) {
			return NGX_CONF_ERROR;
		}
		return NGX_CONF_OK;
	}
}

static const char * ngx_http_ssl_session_cache(ngx_conf_t * cf, const ngx_command_t * cmd, void * conf) // F_SetHandler
{
	ngx_http_ssl_srv_conf_t * sscf = (ngx_http_ssl_srv_conf_t *)conf;
	size_t len;
	ngx_str_t name, size;
	ngx_int_t n;
	ngx_uint_t i, j;
	ngx_str_t * value = static_cast<ngx_str_t *>(cf->args->elts);
	for(i = 1; i < cf->args->nelts; i++) {
		if(sstreq(value[i].data, "off")) {
			sscf->builtin_session_cache = NGX_SSL_NO_SCACHE;
		}
		else if(sstreq(value[i].data, "none")) {
			sscf->builtin_session_cache = NGX_SSL_NONE_SCACHE;
		}
		else if(sstreq(value[i].data, "builtin")) {
			sscf->builtin_session_cache = NGX_SSL_DFLT_BUILTIN_SCACHE;
		}
		else if(value[i].len > sizeof("builtin:")-1 && ngx_strncmp(value[i].data, "builtin:", sizeof("builtin:") - 1) == 0) {
			n = ngx_atoi(value[i].data + sizeof("builtin:")-1, value[i].len - (sizeof("builtin:") - 1));
			if(n == NGX_ERROR) {
				goto invalid;
			}
			sscf->builtin_session_cache = n;
		}
		else if(value[i].len > sizeof("shared:")-1 && ngx_strncmp(value[i].data, "shared:", sizeof("shared:") - 1) == 0) {
			len = 0;
			for(j = sizeof("shared:")-1; j < value[i].len; j++) {
				if(value[i].data[j] == ':') {
					break;
				}
				len++;
			}
			if(len == 0) {
				goto invalid;
			}
			name.len = len;
			name.data = value[i].data + sizeof("shared:") - 1;
			size.len = value[i].len - j - 1;
			size.data = name.data + len + 1;
			n = ngx_parse_size(&size);
			if(n == NGX_ERROR) {
				goto invalid;
			}
			if(n < (ngx_int_t)(8 * ngx_pagesize)) {
				ngx_conf_log_error(NGX_LOG_EMERG, cf, 0, "session cache \"%V\" is too small", &value[i]);
				return NGX_CONF_ERROR;
			}
			sscf->shm_zone = ngx_shared_memory_add(cf, &name, n, &ngx_http_ssl_module);
			if(sscf->shm_zone == NULL) {
				return NGX_CONF_ERROR;
			}
			sscf->shm_zone->F_Init = ngx_ssl_session_cache_init;
		}
		else
			goto invalid;
	}
	if(sscf->shm_zone && sscf->builtin_session_cache == NGX_CONF_UNSET) {
		sscf->builtin_session_cache = NGX_SSL_NO_BUILTIN_SCACHE;
	}
	return NGX_CONF_OK;
invalid:
	ngx_conf_log_error(NGX_LOG_EMERG, cf, 0, "invalid session cache \"%V\"", &value[i]);
	return NGX_CONF_ERROR;
}

static ngx_int_t ngx_http_ssl_init(ngx_conf_t * cf)
{
	ngx_http_core_loc_conf_t  * clcf;
	ngx_http_core_main_conf_t * cmcf = (ngx_http_core_main_conf_t*)ngx_http_conf_get_module_main_conf(cf, ngx_http_core_module);
	ngx_http_core_srv_conf_t ** cscfp = (ngx_http_core_srv_conf_t **)cmcf->servers.elts;
	for(ngx_uint_t s = 0; s < cmcf->servers.nelts; s++) {
		ngx_http_ssl_srv_conf_t * sscf = (ngx_http_ssl_srv_conf_t *)cscfp[s]->ctx->srv_conf[ngx_http_ssl_module.ctx_index];
		if(sscf->ssl.ctx == NULL || !sscf->stapling) {
			continue;
		}
		clcf = (ngx_http_core_loc_conf_t*)cscfp[s]->ctx->loc_conf[ngx_http_core_module.ctx_index];
		if(ngx_ssl_stapling_resolver(cf, &sscf->ssl, clcf->resolver, clcf->resolver_timeout) != NGX_OK) {
			return NGX_ERROR;
		}
	}
	return NGX_OK;
}
