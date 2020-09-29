all:
	gcc -w WTFServer.c -o WTFserver -lssl -lcrypto
	gcc -w WTF.c -o WTF -lssl -lcrypto
clean: 
	rm -r -f WTF WTFserver WTFtest server proj1 proj_destroy
test: WTFtest.c
	gcc WTFtest.c -o WTFtest
