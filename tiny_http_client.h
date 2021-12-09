#ifndef __TINY_HTTP_CLINET_H__
#define __TINY_HTTP_CLINET_H__

#include <stdarg.h>

struct _TinyHttpClient;
typedef struct _TinyHttpClient TinyHttpClient;

/*
    usage: create a tiny http client
    @debug_enable  1:debug mode enable, 0:disable debug mode
    @return NULL:create tiny http client failed
*/
TinyHttpClient* tiny_http_client_create(unsigned char debug_enable);

/*
    usage: destroy tiny http client
*/
void tiny_http_client_destroy(TinyHttpClient *thiz);

/*
    usage: add http header
    @thiz tiny http client
    @key_value custom http header, such as "authToken: xxxxxxxx"
    @return 0:succ, -1:failed
*/
int tiny_http_client_add_header(TinyHttpClient *thiz, char *key_value);

int tiny_http_client_upload_file(TinyHttpClient *thiz,\
		                                char *url,\
		                                char *upload_file_filed,\
		                                char *upload_file_path,\
		                                int param_nums, ...);

int tiny_http_clinet_get(TinyHttpClient* thiz,\
                          char* url,\
                          char* params,\
                          char* response,\
                          size_t max_len);

int tiny_http_clinet_post(TinyHttpClient* thiz,\
                          char* url,\
                          char* params,\
                          char* response,\
                          size_t max_len);

/*
    usage: get the http response body
    @return, not null:response body, null:failed
*/
char* tiny_http_client_get_response(TinyHttpClient* thiz);

/*
    usage: get http response http code
    @return -1:failed, >0:http code
*/
int tiny_http_client_get_response_http_code(TinyHttpClient* thiz);


#endif /*__TINY_HTTP_CLINET_H__*/