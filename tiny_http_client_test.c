#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "tiny_http_client.h"

#define UPLOAD_TOKEN  "aFBpq-_-nqz48kVVB3y17zI2hJFGF6xWtxYN0PWn:yTHVikrmll7kVcRYO7smlvmDoTo=:eyJzY29wZSI6Imltcy1zdG9yYWdlLXRlc3QiLCJkZWFkbGluZSI6MTYzODg3MDUxN30="
#define UPLOAD_URL    "http://upload-z2.qiniup.com"

static void _http_upload_test(void){

    int ret = -1;
    TinyHttpClient* thiz = tiny_http_client_create(1);

    ret = tiny_http_client_upload_file(thiz, UPLOAD_URL, "file", "./test.jpg", 2, "token", UPLOAD_TOKEN, "key", "upload_test.jpg");
    if(ret != 0){
        printf("tiny http client upload failed\n");
        tiny_http_client_destroy(thiz); 
        return;
    }

    int resp_http_code = tiny_http_client_get_response_http_code(thiz);
    printf("resp http code:%d\n", resp_http_code);

    if(resp_http_code == 200){
        char* response = tiny_http_client_get_response(thiz);

        if(response){
            printf("response:%s\n", response);
        }
    }

    tiny_http_client_destroy(thiz); 

    return;
}

static void _http_get_test(void){
    int ret = -1;
    TinyHttpClient* thiz = tiny_http_client_create(0);

    ret = tiny_http_clinet_get(thiz, "http://www.baidu.com", NULL);
    if(ret != 0){
        printf("tiny http client get failed\n");
        tiny_http_client_destroy(thiz); 
        return;
    }

    int resp_http_code = tiny_http_client_get_response_http_code(thiz);
    printf("resp http code:%d\n", resp_http_code);

    tiny_http_client_destroy(thiz); 

    return;
}


int main(int argc, char* argv[]){

    _http_get_test();
    // _http_upload_test();

    return 0;
}




