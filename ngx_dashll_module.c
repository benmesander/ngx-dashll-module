#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>

static ngx_int_t ngx_dashll_init(ngx_conf_t *cf);

static ngx_http_module_t ngx_dashll_module_ctx = {
	NULL,            /* preconfiguration */
	ngx_dashll_init, /* postconfiguration */
	NULL,            /* create main configuration */
	NULL,            /* init main configuration */
	NULL,            /* create server configuration */
	NULL,            /* merge server configuration */
	NULL,            /* create location configuration */
	NULL             /* merge location configuration */
};


ngx_module_t ngx_dashll_module = {
	NGX_MODULE_V1,
	&ngx_dashll_module_ctx, /* module context */
	NULL,                   /* module directives */
	NGX_HTTP_MODULE,        /* module type */
	NULL,                   /* init master */
	NULL,                   /* init module */
	NULL,                   /* init process */
	NULL,                   /* init thread */
	NULL,                   /* exit thread */
	NULL,                   /* exit process */
	NULL,                   /* exit master */
	NGX_MODULE_V1_PADDING
};

/* DASHLL request context */
typedef struct {
	unsigned          done:1;              /* entire body posted */
	unsigned          waiting_more_body:1; /* waiting for more body to come */
} ngx_dashll_ctx_t;

static ngx_int_t ngx_dashll_access_handler(ngx_http_request_t *r);
static void ngx_dashll_payload_handler(ngx_http_request_t *r);

/* handle body */
static void ngx_dashll_payload_handler(ngx_http_request_t *r)
{
	ngx_dashll_ctx_t     *ctx; /* request scope */
    
	ngx_log_error(NGX_LOG_EMERG, r->connection->log, 0, "ngx_dashll_payload_handler called");

	ctx = ngx_http_get_module_ctx(r, ngx_dashll_module);
	ctx->done = 1;

	ngx_log_error(NGX_LOG_EMERG, r->connection->log, 0, "waiting more body: %d", ctx->waiting_more_body);

	if (ctx->waiting_more_body) {
		ctx->waiting_more_body = 0;

		ngx_http_core_run_phases(r);
	}
}

/* access phase handler - reject requests that don't meet spec */
static ngx_int_t ngx_dashll_access_handler(ngx_http_request_t *r)
{
	ngx_table_elt_t *cl;
	ngx_table_elt_t *ct;

	/* must be a HTTP POST */ 
	if (r->method != NGX_HTTP_POST) {
		return NGX_DECLINED;
	}

	/* must have a content type of application/json */
	ct = r->headers_in.content_type;
	if (NULL == ct) {
		return NGX_HTTP_FORBIDDEN;
	}
	if (ngx_strcmp(ct->value.data, "application/json")) {
		return NGX_HTTP_FORBIDDEN;
	}
  
	/* must have a content length */
	cl = r->headers_in.content_length;
	if (NULL == cl) {
		return NGX_HTTP_FORBIDDEN;
	}

	return NGX_DECLINED;
}


/* content handler */
ngx_int_t ngx_dashll_http_content_handler(ngx_http_request_t *r) {
	ngx_int_t rc;

	ngx_dashll_ctx_t     *ctx; /* request scope */
	ngx_log_error(NGX_LOG_EMERG, r->connection->log, 0, "ngx_dashll_access_handler create request-scope ctx");
	ctx = ngx_pcalloc(r->pool, sizeof(ngx_dashll_ctx_t));
	if (NULL == ctx) {
		return NGX_ERROR;
	}
	/* set by ngx_pcalloc:
	 *      ctx->done = 0;
	 *      ctx->waiting_more_body = 0;
	 */
	ngx_http_set_ctx(r, ctx, ngx_dashll_module);

	ngx_log_error(NGX_LOG_EMERG, r->connection->log, 0, "ngx_dashll_access_handler start to read request body");

	/* xxx: should only get called once? is this where unbuffered should be called? */
	rc = ngx_http_read_client_request_body(r, ngx_dashll_payload_handler);
	if (rc >= NGX_HTTP_SPECIAL_RESPONSE) {
		/* error */
		return rc;
	}

	if (rc == NGX_AGAIN) {
		ctx->waiting_more_body = 1;

		return NGX_DONE;
	}

	ngx_log_error(NGX_LOG_EMERG, r->connection->log, 0, "ngx_dashll_access_handler read request body in one request");

	return NGX_DONE;
}


/* see ngx_http_foo_init in https://nginx.org/en/docs/dev/development_guide.html#Modules */
static ngx_int_t ngx_dashll_init(ngx_conf_t *cf)
{
	ngx_http_handler_pt               *h;
	ngx_http_core_loc_conf_t          *clcf;
	ngx_http_core_main_conf_t         *cmcf;

	ngx_conf_log_error(NGX_LOG_EMERG, cf, 0, "ngx_dashll_init called");

	cmcf = ngx_http_conf_get_module_main_conf(cf, ngx_http_core_module);

	h = ngx_array_push(&cmcf->phases[NGX_HTTP_ACCESS_PHASE].handlers);
	if (NULL == h) {
		return NGX_ERROR;
	}
	*h = ngx_dashll_access_handler;

	/* xxx: is this how I add a content handler ? */
	clcf = ngx_http_conf_get_module_loc_conf(cf, ngx_http_core_module);
	clcf->handler = ngx_dashll_http_content_handler;

	return NGX_OK;
}
