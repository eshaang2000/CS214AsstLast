#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <stdio.h> 
#include <sys/types.h> 
#include <unistd.h> 
#include <string.h>
int main(int argc, char* argv[]){
    // system("mkdir client && mkdir server");
    // int n=atoi(argv[1]);
    int flag=0;
    if(argc!=1){
        flag=1;
    }
    mkdir("server", 0777);
    system("make");
    system("cp WTFserver server");
    char* comm=malloc(1000*sizeof(char));
    strcpy(comm, "cd server && ./WTFserver ");
    // strcat(comm)
    if(flag==1){
        strcat(comm,argv[2]);
        strcat(comm,"&");
        system(comm);
        // bzero(comm);
        free(comm);

    char* comm=malloc(1000*sizeof(char));
    strcpy(comm, "./WTF configure ");
    strcat(comm, argv[1]);
    strcat(comm," ");
    strcat(comm, argv[2]);
    system(comm);
    free(comm);




    }
    else{
    // int n=fork();
    // if(n==0){
    system("cd server && ./WTFserver 8080&");
    system("./WTF configure localhost 8080");
    }
    system("./WTF create proj1");
    // printf("\n\nThis is the create command");
    system("touch proj1/file1.txt && touch proj1/file2.txt");
    system("touch proj1/file3.txt");
    system("./WTF add proj1 file1.txt");
    system("./WTF add proj1 file2.txt");
    system("./WTF add proj1 file3.txt");
    system("./WTF remove proj1 file2.txt");
    system("./WTF commit proj1");
    system("./WTF push proj1");
    // system("./WTF history proj1");
    system("rm -f proj1/.Manifest");
    system("touch proj1/.Manifest");
    system("chmod 777 proj1/.Manifest");
    system("echo 0 > proj/Manifest.txt");
    system("./WTF update proj1");
    system("./WTF upgrade proj1");
    printf("\nWe now create another project just to destroy\n");
    system("./WTF create proj_destroy");
    system("./WTF destroy proj_destroy");
    printf("The changes are now all visible in the directories\n");



    // }
    // else{
    // system("ls");
    
    // system("");
    

}