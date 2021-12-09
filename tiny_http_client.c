#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <sys/time.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <unistd.h>
#include <linux/tcp.h>
#include <sys/socket.h>
#include <errno.h>
#include <netdb.h>
#include <sys/stat.h>

#include "tiny_http_client.h"

#define TINY_HTTP_BOUNDARY "------tinyhttpclientboundary"

#define HTTP_HEAD   "POST %s HTTP/1.1\r\n"\
                    "Host: %s\r\n"\
                    "Content-Type: multipart/form-data; boundary=%s\r\n"\
                    "Content-Length: %ld\r\n"

#define HTTP_HEAD_END	"Connection: close\r\n\r\n"

#define UPLOAD_REQUEST  "--%s\r\n"\
                        "Content-Disposition: form-data; name=\"file\"; filename=\"%s\"\r\n"\
                        "Content-Type: %s\r\n\r\n"

typedef struct _TinyHttpClientHeader{
	char* key_value;
	struct _TinyHttpClientHeader* next;
}TinyHttpClientHeader;

struct _TinyHttpClient{
	TinyHttpClientHeader* header;

    int resp_http_code;
    char* response;

	unsigned char debug_enable;
};

static char* _get_upload_file_content_type(char* filename){
	if(strstr(filename, ".txt") != NULL){
		return "text/plain";
	}else if(strstr(filename, ".mp4") != NULL){
		return "video/mpeg4";
	}else if(strstr(filename, ".jpg") != NULL || strstr(filename, ".jpeg") != NULL){
		return "image/jpeg";
	}else if(strstr(filename, ".png") != NULL){
		return "image/png";
	}else{
		return "text/html";
	}
}

static int _get_filename(char* file_path, char* filename, size_t max_len){
	if(file_path == NULL || filename == NULL){
		return -1;
	}

    char* p = NULL;

    p = strrchr(file_path, '/');
    if(p == NULL){
        if(strlen(file_path) < max_len){
            strncpy(filename, file_path, max_len);
            return 0;
        }
    }else{
        if(strlen(p+1) < max_len){
            strncpy(filename, p+1, max_len);
            return 0;
        }
    }

    return -1;
}


static int _parse_recv_get_http_code_response(char* data, int* http_code, char* response, size_t max_resp_len){
    if(data == NULL || http_code == NULL || response == NULL){
        return -1;
    }

    typedef enum _RecvDataParseState{
        RECV_DATA_PARSE_STATE_START = 0,
        RECV_DATA_PARSE_STATE_HTTP_PROTOCOL = 1,
        RECV_DATA_PARSE_STATE_HTTP_PROTOCOL_END = 2,
        RECV_DATA_PARSE_STATE_HTTP_CODE = 3,
        RECV_DATA_PARSE_STATE_HTTP_CODE_END = 4,
        RECV_DATA_PARSE_STATE_RESPONSE_DATA = 5,
        RECV_DATA_PARSE_STATE_END,
    }RecvDataParseState;

    RecvDataParseState state = RECV_DATA_PARSE_STATE_START;

    char* http_code_start = 0;
    char* resposne_start = 0;
    char http_code_buff[64] = {0};

    char* p = data;
    while(*p != '\0'){
        if(state == RECV_DATA_PARSE_STATE_START){
            if(strncmp(p, "HTTP/1.1", strlen("HTTP/1.1")) != 0){
                return -1;
            }else{
                state = RECV_DATA_PARSE_STATE_HTTP_PROTOCOL;
                p += strlen("HTTP/1.1");
            }
        }else if(state == RECV_DATA_PARSE_STATE_HTTP_PROTOCOL){

            if(*p == ' '){
                state = RECV_DATA_PARSE_STATE_HTTP_PROTOCOL_END;
            }

            p++;
        }else if(state == RECV_DATA_PARSE_STATE_HTTP_PROTOCOL_END){
            if(isdigit(*p)){
                http_code_start = p;
                state = RECV_DATA_PARSE_STATE_HTTP_CODE;
            }
            ++p;
        }else if(state == RECV_DATA_PARSE_STATE_HTTP_CODE){

            if(*p == ' '){
                strncpy(http_code_buff, http_code_start, p - http_code_start);
                state = RECV_DATA_PARSE_STATE_HTTP_CODE_END;
            }

            p++;
        }else if(state == RECV_DATA_PARSE_STATE_HTTP_CODE_END){

            if(strncmp(p, "\r\n\r\n", strlen("\r\n\r\n")) == 0){
                state = RECV_DATA_PARSE_STATE_RESPONSE_DATA;
                p += strlen("\r\n\r\n");

                resposne_start = p;
            }

            p++;
        }else{
            p++;
        }
    }

    if(state == RECV_DATA_PARSE_STATE_RESPONSE_DATA){

        *http_code = atoi(http_code_buff);
        strncpy(response, resposne_start, max_resp_len);

        return 0;
    }else{

        return -1;
    }

    return 0;
}

static int _get_file_size(const char *path){  
    unsigned long filesize = -1;      
    struct stat statbuff;  
    if(stat(path, &statbuff) < 0){  
        return -1;  
    }else{  
        filesize = statbuff.st_size;  
    }  

    return filesize;  
}

static int _send_data(int sock, char *buff,int len){
    if(sock < 0 || buff == NULL || !(len > 0)){
        return -1;
    }   

    int ret = 0;
    int send_total = 0;
    while(len){

        ret = send(sock, buff + send_total, len, 0);
        len -= ret;
        send_total += ret;
        if(ret < 0){
            printf("[%s][%d] create sock failed, error(%s)\n", __func__, __LINE__, strerror(errno));
            return -1;
        }
    }

    return 0;
}


static int _create_sock(void){
    int sock = -1;
    int         nReuseAddr = 1;
    int     nFlag = 1;

    sock = socket( PF_INET, SOCK_STREAM, IPPROTO_TCP);
    if(sock == -1){
        printf("[%s][%d]create sock failed, error(%s)\n", __func__, __LINE__, strerror(errno));
        return -1;
    }

    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, ( void* )&nReuseAddr, sizeof( int ) );
    setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, ( char* )&nFlag, sizeof( int ) );
   

    struct timeval timeout;

    nFlag = 1;
    setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, ( void* )&nFlag, sizeof( int ) );
    timeout.tv_sec = 5;
    timeout.tv_usec = 0;
			
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, ( char* )&timeout, sizeof( timeout ) );
    timeout.tv_sec = 5;
    timeout.tv_usec = 0;
    setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, ( char* )&timeout, sizeof( timeout ) ); 
    return sock;
}


static int _close_sock(int sock){
    shutdown(sock, 2);
    close(sock);

    return 0;
}

static int _parse_get_server_port(char* url, char server[64], unsigned short* port, char path[64]){
    if(url == NULL || server == NULL || path == NULL){
        return -1;
    }

    typedef enum _UrlParseState{
        URL_PARSE_STATE_START = 0,
        URL_PARSE_STATE_HTTP_FLAG = 1,
        URL_PARSE_STATE_HTTP_SLASH_SERVER,
        URL_PARSE_STATE_SERVER,
        URL_PARSE_STATE_SERVER_COLON_PORT,
        URL_PARSE_STATE_PORT,
        URL_PARSE_STATE_PATH,
        URL_PARSE_STATE_END,
    }UrlParseState;

    UrlParseState state = URL_PARSE_STATE_START;

    char* server_start = 0;
    char* port_start = 0;
    char* path_start = 0;

    char* p = url;
    while(*p != '\0'){
        if(state == URL_PARSE_STATE_START){
            if(strncmp(p, "http:", 5) != 0){
                return -1;
            }else{
                state = URL_PARSE_STATE_HTTP_FLAG;
                p += strlen("http:");
            }
        }else if(state == URL_PARSE_STATE_HTTP_FLAG){
            if(*p == '/'){
                state = URL_PARSE_STATE_HTTP_SLASH_SERVER;
            }

            p++;
        }else if(state == URL_PARSE_STATE_HTTP_SLASH_SERVER){
            if(islower(*p) || isdigit(*p)){
                server_start = p;
                state = URL_PARSE_STATE_SERVER;
            }
            ++p;
        }else if(state == URL_PARSE_STATE_SERVER){
            if(*p == ':'){
                strncpy(server, server_start, p - server_start);
                state = URL_PARSE_STATE_SERVER_COLON_PORT;
            }
            p++;
        }else if(state == URL_PARSE_STATE_SERVER_COLON_PORT){
            if(isdigit(*p)){
                port_start = p;
                state = URL_PARSE_STATE_PORT;
            }
            p++;
        }else if(state == URL_PARSE_STATE_PORT){
            if(*p == '/'){  
                char buff[32] = {0};
                strncpy(buff, port_start, p - port_start);

                *port = atoi(buff);
                path_start = p;
                state = URL_PARSE_STATE_PATH;
            }
            p++;
        }else if(state == URL_PARSE_STATE_PATH){
            p++;
        }
    }


    if(state == URL_PARSE_STATE_SERVER){
        strncpy(server, server_start, p - server_start);
        *port = 80;
        strcpy(path, "/");
        return 0;
    }else if(state == URL_PARSE_STATE_PATH){
        strncpy(path, path_start, p - port_start);
        return 0;
    }else{

        return -1;
    }
}

static int _read_send_file_content(char* file_path, size_t filesize, int sock){
    if(file_path == NULL || sock < 0){
        return -1;
    }

    int remain_size = filesize;
    int fd = -1;

    fd = open(file_path, O_RDONLY);
    if(fd < 0){
        printf("open file failed, error:%s\n", strerror(errno));
        return -1;
    }

    int mtu_size = 1024;
    int read_size = 0;
    char buff[2048] = {0};
    int ret;

    while(remain_size > 0){
        if(remain_size > mtu_size){
            read_size = mtu_size;
        }else{
            read_size = remain_size;
        }

        ret = read(fd, buff, read_size);
        if(ret < 0){
            printf("[%s][%d] read data failed, error:%s\n", __func__, __LINE__, strerror(errno));
            close(fd);
            return -1;
        }

        ret = _send_data(sock, buff, read_size);
        if(ret != 0){
            printf("[%s][%d] send data failed.\n", __func__, __LINE__);
            close(fd);
            return -1;
        }

        remain_size -= read_size;
    }

    close(fd);

    return 0;
}

TinyHttpClient* tiny_http_client_create(unsigned char debug_enable){
    TinyHttpClient* thiz = NULL;

    thiz = (TinyHttpClient*)malloc(sizeof(TinyHttpClient));

	thiz->header = NULL;

    thiz->resp_http_code = 0;
    thiz->response = NULL;

	thiz->debug_enable = debug_enable;

    return thiz;
}

static void _tiny_http_client_header_destroy(TinyHttpClientHeader *header){
	if(header == NULL){
		return;
	}	

	TinyHttpClientHeader *p = header; 

	while(p){
		TinyHttpClientHeader *next = p->next;

		if(p->key_value){
			free(p->key_value);
		}
		free(p);

		p = next;
	}

	return;
}

void tiny_http_client_destroy(TinyHttpClient* thiz){
    if(thiz){	

		_tiny_http_client_header_destroy(thiz->header);
	
		if(thiz->response){
			free(thiz->response);
			thiz->response = NULL;
		}

		free(thiz);
		thiz = NULL;
    }

    return;
}

int tiny_http_client_upload_file(TinyHttpClient* thiz,\
                                char* url,\
                                char* upload_file_filed,\
                                char* upload_file_path,\
                                int param_nums,...){
    if(thiz == NULL || url == NULL || upload_file_filed == NULL || upload_file_path == NULL){
        return -1;
    }

    int ret = 0;

    int filesize = _get_file_size(upload_file_path);
    if(filesize == -1){
        printf("get file size failed, %s \n", upload_file_path);
        return -1;
    }
			
	char filename[64] = {0};
	ret = _get_filename(upload_file_path, filename, 64);
	if(ret != 0){
		printf("[%s][%d] get filename failed, path(%s)\n", __func__, __LINE__, upload_file_path);
		return -1;
	}

    char server[64] = {0};
    unsigned short port = 0;
    char path[64] = {0};

    ret = _parse_get_server_port(url, server, &port, path);
    if(ret != 0){
        printf("[%s][%d] parse get server port failed.\n", __func__, __LINE__);
        return -1;
    }

    int sock = -1;

    sock = _create_sock();

    struct hostent* host;

    host = gethostbyname(server);
    if(host == NULL){
        printf("gethostbyname failed.\n");
        _close_sock(sock);
        return -1;
    }

    struct sockaddr_in daddr;
    bzero(&daddr, sizeof( struct sockaddr_in ) );
    daddr.sin_family        = AF_INET;
    daddr.sin_port          = htons(port);
    daddr.sin_addr = *((struct in_addr *)host->h_addr);

    ret = connect(sock, ( struct sockaddr* )&daddr, sizeof( struct sockaddr ));
    if(ret == -1){ 
        perror("connect failed!\n");
        _close_sock(sock);
        return -1;
    }

    char http_head[1024] = {0};
    char http_form_data[512] = {0};
    char upload_request[1024] = {0};
    char end[256] = {0};
    size_t end_len = 0;
    size_t total_http_content_size = 0;

    sprintf(end, "\r\n--%s--\r\n", TINY_HTTP_BOUNDARY);
    end_len = strlen(end);


    size_t form_data_len = 0;

    va_list argp;              
    va_start(argp, param_nums);

    int i = 0;
    for(i = 0; i < param_nums; i++){
        char* param_key = va_arg(argp, char*);
        char* param_value = va_arg(argp, char*);

        char tmp_buff[512] = {0};

        sprintf(tmp_buff,
                "--%s\r\n"
                "Content-Disposition: form-data; name=\"%s\"\r\n\r\n"
                "%s\r\n",
                TINY_HTTP_BOUNDARY, param_key, param_value);

        strcat(http_form_data, tmp_buff);
    }
    va_end(argp); 

    form_data_len = strlen(http_form_data);

    unsigned long upload_request_len = snprintf(upload_request, 1024, UPLOAD_REQUEST, TINY_HTTP_BOUNDARY, upload_file_path, _get_upload_file_content_type(filename)); 

    total_http_content_size = form_data_len + upload_request_len + filesize + end_len;

	snprintf(http_head, 1024, HTTP_HEAD, path, server, TINY_HTTP_BOUNDARY, total_http_content_size); 

	TinyHttpClientHeader* header = thiz->header;
	while(header != NULL){
		char buff[256] = {0};
		snprintf(buff, 256, "%s\r\n", header->key_value);
		strcat(http_head, buff);
		header = header->next;
	}
	
	strcat(http_head, HTTP_HEAD_END);

	unsigned int head_len = strlen(http_head);

    size_t send_size = head_len + form_data_len + upload_request_len;

    char* send_buff = NULL;
    size_t copy_len = 0;

    send_buff = (char*)malloc(send_size + 1);
    if(send_buff == NULL){
        printf("[%s][%d] malloc memory failed.\n", __func__, __LINE__);
        _close_sock(sock);
        return -1;
    }

    memset(send_buff, 0, send_size + 1);

    memcpy(send_buff, http_head, head_len);
    copy_len += head_len;

    memcpy(send_buff + copy_len, http_form_data, form_data_len);
    copy_len += form_data_len;

    memcpy(send_buff + copy_len, upload_request, upload_request_len);
    copy_len += upload_request_len;

	if(thiz->debug_enable){
		printf("[%s][%d] >>>>>>>>>>>>>>>>>>>>> \n %s\n", __func__, __LINE__, send_buff);
	}

    ret = _send_data(sock, send_buff, copy_len);
    if(ret < 0){
        printf("[%s][%d] read send file content failed.\n", __func__, __LINE__);
        free(send_buff);
        _close_sock(sock);
        return -1;
    }

    ret = _read_send_file_content(upload_file_path, filesize, sock);
    if(ret < 0){
        printf("[%s][%d] read send file content failed.\n", __func__, __LINE__);
        free(send_buff);            
        _close_sock(sock);
        return -1;
    }

    ret = _send_data(sock, end, end_len);
    if(ret < 0){
        printf("[%s][%d] read send file content failed.\n", __func__, __LINE__);
        free(send_buff);            
        _close_sock(sock);
        return -1;
    }

    char recv_buff[1024] = {0};
    ret = recv(sock, recv_buff, 1024, 0);
    if(ret < 0){
        printf("[%s][%d] recv failed, error:%s.\n", __func__, __LINE__, strerror(errno));

        free(send_buff);

        _close_sock(sock);
        return -1;
    }else{
		_close_sock(sock);

		if(thiz->debug_enable){
			printf("[%s][%d] <<<<<<<<<<<<<<<<<<<< \n %s\n", __func__, __LINE__, recv_buff);
		}

        int http_code = 0;
        char response[512] = {0};

        ret = _parse_recv_get_http_code_response(recv_buff, &http_code, response, 512);
        if(ret < 0){
            free(send_buff);            
            return -1;
        }

        thiz->resp_http_code = http_code;
        if(thiz->response != NULL){
            free(thiz->response);
			thiz->response = NULL;
        }
        thiz->response = strdup(response);

		if(send_buff){
			free(send_buff);		
			send_buff = NULL;
		}

        return 0;
    }
}


char* tiny_http_client_get_response(TinyHttpClient* thiz){
    if(thiz == NULL){
        return NULL;
    }

    return thiz->response;
}

int tiny_http_client_get_response_http_code(TinyHttpClient* thiz){
    if(thiz == NULL){
        return -1;
    }

    return thiz->resp_http_code;
}	

int tiny_http_client_add_header(TinyHttpClient* thiz, char* key_value){
	if(thiz == NULL || key_value == NULL){
		return -1;
	}

	TinyHttpClientHeader* header_node = (TinyHttpClientHeader*)malloc(sizeof(TinyHttpClientHeader));
	header_node->next = NULL;
	header_node->key_value = strdup(key_value);

	if(thiz->header == NULL){
		thiz->header = header_node;
	}else{
		TinyHttpClientHeader* p = thiz->header;

		while(p->next != NULL){
			p = p->next;
		}

		p->next = header_node;
	}

	return 0;
}

int tiny_http_clinet_get(TinyHttpClient* thiz,\
                          char* url,\
                          char* params,\
                          char* response,\
                          size_t max_len){

    //To Do
    return 0;
}


int tiny_http_clinet_post(TinyHttpClient* thiz,\
                          char* url,\
                          char* params,\
                          char* response,\
                          size_t max_len){
    //To Do

    return 0;
}

