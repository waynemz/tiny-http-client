SRC := tiny_http_client.c

all:
	gcc -o tiny_http_client_test $(SRC) tiny_http_client_test.c 
clean:
	rm -f tiny_http_client_test *.o
