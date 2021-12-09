#ifndef __TINY_HTTP_CLINET_H__
#define __TINY_HTTP_CLINET_H__

#include <stdarg.h>

struct _TinyHttpClient;
typedef struct _TinyHttpClient TinyHttpClient;

TinyHttpClient* tiny_http_client_create(unsigned char debug_enable);
void tiny_http_client_destroy(TinyHttpClient* thiz);

int tiny_http_client_add_header(TinyHttpClient* thiz, char* key_value);

int tiny_http_client_upload_file(TinyHttpClient* thiz,\
		                                char* url,\
		                                char* upload_file_filed,\
		                                char* upload_file_path,\
		                                int param_nums,...);

int tiny_http_clinet_get(TinyHttpClient* thiz,\
                          char* url,\
                          char* header,\
                          char* params,\
                          char* response,\
                          size_t max_len);

int tiny_http_clinet_post(TinyHttpClient* thiz,\
                          char* url,\
                          char* header,\
                          char* params,\
                          char* response,\
                          size_t max_len);

char* tiny_http_client_get_response(TinyHttpClient* thiz);

int tiny_http_client_get_response_http_code(TinyHttpClient* thiz);


#endif /*__TINY_HTTP_CLINET_H__*/