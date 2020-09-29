//#include <winsock2.h>
//#include<winsock.h>
//#include <windows.h>
#include <netdb.h> 
#include <stdio.h> 
#include <stdlib.h> 
#include <string.h> 
#include <sys/socket.h> 
#include <dirent.h>
#include<fcntl.h> 
#include<errno.h> 
#include<sys/stat.h>
#include<sys/sendfile.h>
#include <openssl/sha.h>
#include <unistd.h>


#define MAX 2000 
#define SA struct sockaddr 
#define TRUE 1
#define FALSE -1
#define delimiter "\n"

int n;
int config();
void writeFileContents(int socket_fd, int file_fd);
int samechar(const char *str, char c);
char* readUntilDelim(int fd, char delim);
char* getFileContents(int fd);
char* getFileHash(int file_fd);
void updateHashes(int manifest_fd);
void sendFileContents(int socket_fd, char* file);

struct manifestEntry{
    char* path;
    char* hash;
    int vers;
};

struct manifestEntry* man[1000];


// struct manifest* put(int fd);//takes the file descriptor and puts it into the manifest struct
void put(int fd);


/*
int main(int argc, char* argv[]){
    int socket_fd, conn_fd;
    //initialize socket
    socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if(socket_fd == -1){
        printf("socket creation error\n");
        exit(0);
    } else 
        printf("socket creation successful\n");

    //create object representing endpoint (server) to connect to
    struct hostent* host_name = gethostbyname(argv[1]); //hostname obj
    struct sockaddr_in server_address; //object representing endpoint connecting to
    bzero(&server_address, sizeof(server_address));
    server_address.sin_family = AF_INET;
    int port_num = atoi(argv[2]); //argv[2] == port # of connection
    server_address.sin_port = htons(port_num);
    bcopy((char*)host_name->h_addr, (char*)&server_address.sin_addr.s_addr, host_name->h_length);

    //connect to server
    if (connect(socket_fd, (SA*)&server_address, sizeof(server_address)) != 0){
        printf("connection failed\n");
        exit(0);
    } else {
        printf("connected to server\n");
    }

    //writing message to server
    char msg[MAX];
    bzero(msg, sizeof(msg));
    strcpy(msg, "yeet\n");
    write(socket_fd, msg, sizeof(msg));

    //receiving message from server
    bzero(msg, sizeof(msg));
    read(socket_fd, msg, sizeof(msg));
    printf("From server: %s\n", msg);
    printf("client exiting\n");

    close(socket_fd);


}
*/
int main(int argc, char* argv[]){
    char* cmd = argv[1];
    if (strcmp(cmd, "configure") ==  0){
        //write host & port # to configure file
        int config_fd = open(".configure", O_RDWR | O_CREAT, 0666);
        if (config_fd < 0){ perror("configure error"); exit(0); }
        fchmod(config_fd, S_IRWXU);
        char* config_str = (char*) malloc( ( strlen(argv[2]) + strlen(argv[3])  + 2 ) * sizeof(char));
        strcpy(config_str, argv[2]);
        strcat(config_str, delimiter);
        strcat(config_str, argv[3]);
        strcat(config_str, delimiter);
        write(config_fd, config_str, strlen(config_str));
        free(config_str);
    } else if (strcmp(cmd, "checkout") ==  0){
        int socket_fd = config(); //creates connection to server
        if (socket_fd == -1){ printf("config error\n"); exit(0); }
        else {
            //send message to server
            char msg[MAX];
            bzero(msg, sizeof(msg));
            strcpy(msg, cmd);
            strcat(msg, delimiter);
            strcat(msg, argv[2]);
            strcat(msg, delimiter);
            //printf("message sending to server: %s\n", msg);
            write(socket_fd, msg, sizeof(msg));

            //receiving message from server
            char c;
            int bytes;
            //printf("now receiving data from server. message:\n");

            //reads incoming message from server & creates appropriate files/dirs on client side
            //message format: "D:<dir_name>:F:<filename>:<file contents>:...."
            while(1){
                bytes = read(socket_fd, &c, 1);
                //printf("%c", c);
                if (bytes == 0) { break; }
                else if (bytes < 0) { perror("error reading socket\n"); exit(1); }
                if (samechar(delimiter, c) == 1)
                    continue;
                else if (c == 'D'){ //incoming directory
                    //printf("\nnow creating directory\n");
                    bytes = read(socket_fd, &c, 1);
                    //printf("%c", c);
                    if (samechar(delimiter, c) == 1){
                        //OK
                    }
                    char dir_name[500];
                    int n = 0;
                    while (n < 499){
                        bytes = read(socket_fd, &c, 1);
                        //printf("%c", c);
                        if (samechar(delimiter, c) == 1){ break; }
                        if (c < 32 || c > 126){ continue; }
                        dir_name[n++] = c;
                    }
                    dir_name[n] = '\0';
                    //printf("client now creating directory: %s\n", dir_name);
                    int dir = mkdir(dir_name, 0777);
                    if (dir < 0){ printf("unable to make directory %s\n", dir_name); exit(1); }
                    else { continue; }
                } else if (c == 'F'){ //incoming file
                    //printf("\nnow creating file\n");
                    bytes = read(socket_fd, &c, 1);
                    //printf("%c", c);
                    if (samechar(delimiter, c) == 1){
                        //OK
                    }
                    char f_name[500];
                    int n = 0;
                    while (n < 499){
                        bytes = read(socket_fd, &c, 1);
                        //printf("%c", c);
                        if (samechar(delimiter, c) == 1){ break; }
                        f_name[n++] = c;
                    }
                    f_name[n] = '\0';
                    //printf("client now creating file: %s\n", f_name);
                    int file_fd = open(f_name, O_RDWR | O_CREAT);
                    chmod(f_name, S_IRWXU);
                    if (file_fd < 0){ printf("error opening file %s\n", f_name); exit(1); }
                    else {
                        writeFileContents(socket_fd, file_fd);
                        continue;
                    }
                }

            }
        }

        write(socket_fd, "OK", strlen("OK"));
    } else if (strcmp(cmd, "update") ==  0){
        int socket_fd = config(); //creates connection to server
        if (socket_fd < 0){ printf("config error\n"); exit(0); }

        //sending message to server
        char msg[MAX];
        bzero(msg, sizeof(msg));
        strcpy(msg, cmd);
        strcat(msg, delimiter);
        strcat(msg, argv[2]);
        strcat(msg, delimiter);
        //printf("message sending to server: %s\n", msg);
        write(socket_fd, msg, sizeof(msg));

        //receiving ack
        bzero(msg, sizeof(msg));
        read(socket_fd, msg, sizeof(msg));
        if (strcmp(msg, "OK") != 0){
            perror("socket error\n"); exit(1);
        }

        //sending client Manifest
        char* manifest_path = (char*) malloc( (strlen(argv[2]) + strlen("/.Manifest\0")) * sizeof(char) );
        strcpy(manifest_path, argv[2]);
        strcat(manifest_path, "/.Manifest\0");
        sendFileContents(socket_fd, manifest_path);

        //receiving update file
        char* update_path = (char*) malloc( (strlen(argv[2]) + strlen("/.Update\0")) * sizeof(char) );
        strcpy(update_path, argv[2]);
        strcat(update_path, "/.Update\0");
        remove(update_path);
        int update_fd = open(update_path, O_RDWR | O_APPEND | O_CREAT, 0777);
        if (update_fd < 0){ perror("error opening update file"); exit(1); }
        chmod(update_path, S_IRWXU);
        writeFileContents(socket_fd, update_fd);
        
        free(manifest_path);
        free(update_path);

    } else if (strcmp(cmd, "upgrade") ==  0){
        int socket_fd = config(); //creates connection to server
        if (socket_fd < 0){ printf("config error\n"); exit(0); }

        //sending message to server
        char msg[MAX];
        bzero(msg, sizeof(msg));
        strcpy(msg, cmd);
        strcat(msg, delimiter);
        strcat(msg, argv[2]);
        strcat(msg, delimiter);
        //printf("message sending to server: %s\n", msg);
        write(socket_fd, msg, sizeof(msg));

        //receiving message from server
        char c;
        int bytes;
        int terminate = FALSE;

        //reads incoming message from server & creates appropriate files/dirs on client side
        //message format: "D:<dir_name>:F:<filename>:<file contents>:...."
        while(1){
            bytes = read(socket_fd, &c, 1);
            printf("%c", c);
            if (bytes == 0) { break; }
            else if (bytes < 0) { perror("error reading from socket\n"); exit(1); }
            if (samechar(delimiter, c) == 1)
                continue;
            else if (c == 'D'){ //incoming directory
                //printf("\nnow creating directory\n");
                bytes = read(socket_fd, &c, 1);
                printf("%c", c);
                if (samechar(delimiter, c) == 1){
                    //OK
                }
                char dir_name[500];
                int n = 0;
                while (n < 499){
                    bytes = read(socket_fd, &c, 1);
                    printf("%c", c);
                    if (samechar(delimiter, c) == 1){ break; }
                    if (c < 32 || c > 126){ continue; }
                    dir_name[n++] = c;
                }
                dir_name[n] = '\0';
                //printf("client now creating directory: %s\n", dir_name);
                remove(dir_name);
                int dir = mkdir(dir_name, 0777);
                if (dir < 0){ printf("unable to make directory %s\n", dir_name); exit(1); }
                else { continue; }
            } else if (c == 'F' || c == 'M'){ //incoming file
                if (c == 'M'){
                    terminate = TRUE;
                }
                //printf("\nnow creating file\n");
                //bzero(c, sizeof(c));
                //bytes = read(socket_fd, &c, 1);
                //lseek(socket_fd, 1, SEEK_CUR);
                read(socket_fd, &c, 1);
                //printf("%c", c);
                while (c == 'M'){
                    //printf("curr byte: %c\n", c);
                    bytes = read(socket_fd, &c, 1);
                }
                char f_name[500];
                int n = 0;
                lseek(socket_fd, 1, SEEK_CUR);
                while (n < 499){
                    bytes = read(socket_fd, &c, 1);
                    //printf("%c", c);
                    if (samechar(delimiter, c) == 1){ break; }
                    f_name[n++] = c;
                }
                f_name[n] = '\0';
                //printf("client now creating file: %s\n", f_name);
                remove(f_name);
                int file_fd = open(f_name, O_RDWR | O_CREAT, 0777);
                chmod(f_name, S_IRWXU);
                if (file_fd < 0){ printf("error opening file %s\n", f_name); exit(1); }
                else {
                    //printf("writing file contents of %s\n", f_name);
                    writeFileContents(socket_fd, file_fd);
                    //printf("writing done\n");
                    if (terminate == TRUE){
                        break;
                    }
                    continue;
                }
            }
        }

        //open .commit
        char* update_path = (char*) malloc( (strlen(argv[2]) + strlen("/.Update\0")) * sizeof(char) );
        strcpy(update_path, argv[2]);
        strcat(update_path, "/.Update\0");
        int update_fd = open(update_path, O_RDWR | O_APPEND);
        if (update_fd < 0){ perror("error opening commit file"); exit(0); }
        
        //find files for deletion in .commit
        while(1){
            char* line = readUntilDelim(update_fd, '\n');
            if (line == NULL || strcmp(line, "\0") == 0){
                break;
            }
            
            char* token = strtok(line, "\t");
            char* flag = token;

            token = strtok(NULL, "\t");
            char* path = token;

            token = strtok(NULL, "\t");
            char* hash = token;

            token = strtok(NULL, "\t");
            char* ver_num = token;

            if (strcmp(flag, "D") == 0){
                remove(path);
            }
        }
        remove(update_path);
        free(update_path);

    } else if (strcmp(cmd, "commit") ==  0){
        int socket_fd = config(); //creates connection to server
        if (socket_fd < 0){ printf("config error\n"); exit(0); }

        //sending message to server
        char msg[MAX];
        bzero(msg, sizeof(msg));
        strcpy(msg, cmd);
        strcat(msg, delimiter);
        strcat(msg, argv[2]);
        strcat(msg, delimiter);
        //printf("message sending to server: %s\n", msg);
        write(socket_fd, msg, sizeof(msg));

        //receiving message from server
        bzero(msg, sizeof(msg));
        read(socket_fd, msg, sizeof(msg));

        //open client manifest
        char* manifest_path = (char*) malloc( (strlen(argv[2]) + strlen("/.Manifest\0")) * sizeof(char) );
        strcpy(manifest_path, argv[2]);
        strcat(manifest_path, "/.Manifest\0");
        int manifest_fd = open(manifest_path, O_RDWR | O_CREAT);
        if (manifest_fd < 0){ perror("error opening manifest file"); exit(1); }
        free(manifest_path);

        //compare version #s
        char cli_man_num;
        read(manifest_fd, &cli_man_num, 1);
        if (msg[0] > cli_man_num){
            printf("client does not have latest version - please update\n");
            exit(1);
        }
        
        //update the client manifest
        lseek(manifest_fd, 0, SEEK_SET);
        updateHashes(manifest_fd);
        
        //create .commit
        char* commit_path = (char*) malloc( (strlen(argv[2]) + strlen("/Commit.txt\0")) * sizeof(char) );
        strcpy(commit_path, argv[2]);
        strcat(commit_path, "/Commit.txt\0");
        int commit_fd = open(commit_path, O_RDWR | O_APPEND | O_CREAT);
        if (commit_fd < 0){ perror("error opening commit file"); exit(0); }
        chmod(commit_path, S_IRWXU);

        //searching client manifest for changes
        lseek(manifest_fd, 2, SEEK_SET);
        while(1){
            char* line = readUntilDelim(manifest_fd, '\n');
            if (line == NULL || strcmp(line, "\0") == 0){
                break;
            }
            char* token = strtok(line, "\t");
            char* path = token;

            token = strtok(NULL, "\t");
            char* hash = token;

            token = strtok(NULL, "\t");
            char* ver_num = token;

            if (strstr(msg, path) == NULL){ //add case
                //write add to commit
                write(commit_fd, "A\t", strlen("A\t"));
                write(commit_fd, path, strlen(path));
                write(commit_fd, "\t", strlen("\t"));
                write(commit_fd, hash, strlen(hash));
                write(commit_fd, "\t", strlen("\t"));
                write(commit_fd, ver_num, strlen(ver_num));
                write(commit_fd, "\n", strlen("\n"));
                printf("A %s\n", path);

            } else {
                char* temp = strstr(msg, path);
                char server_hash[SHA_DIGEST_LENGTH*2+1];
                memcpy(server_hash, &temp[1], SHA_DIGEST_LENGTH*2);
                server_hash[SHA_DIGEST_LENGTH*2] = '\0';
                if (strcmp(hash, server_hash) != 0){ //modify case
                    write(commit_fd, "M\t", strlen("A\t"));
                    write(commit_fd, path, strlen(path));
                    write(commit_fd, "\t", strlen("\t"));
                    write(commit_fd, hash, strlen(hash));
                    write(commit_fd, "\t", strlen("\t"));
                    write(commit_fd, ver_num, strlen(ver_num));
                    write(commit_fd, "\n", strlen("\n"));
                    printf("M %s\n", path);
                }
            }
        }

        int i;
        char* line = NULL;
        int n = 0;
        lseek(manifest_fd, 0, SEEK_SET);
        char* cli_manifest_contents = getFileContents(manifest_fd);
        
        //searching server manifest for changes
        for (i = 2; i < strlen(msg); i++){
            char c = msg[i];
            line = (char*) realloc(line, ++n*sizeof(char));
            if (c == '\n'){
                line[n-1] = '\0';
                char* token = strtok(line, "\t");
                char* path = token;

                token = strtok(NULL, "\t");
                char* hash = token;

                token = strtok(NULL, "\t");
                char* ver_num = token;

                if (strstr(cli_manifest_contents, path) == NULL){ //delete case
                    //write delete to commit
                    write(commit_fd, "D\t", strlen("A\t"));
                    write(commit_fd, path, strlen(path));
                    write(commit_fd, "\t", strlen("\t"));
                    write(commit_fd, hash, strlen(hash));
                    write(commit_fd, "\t", strlen("\t"));
                    write(commit_fd, ver_num, strlen(ver_num));
                    write(commit_fd, "\n", strlen("\n"));
                    printf("D %s\n", path);
                }
                line = NULL;
                n = 0;

            } else {
                line[n-1] = c;
            }
        }
        free(cli_manifest_contents);
        free(line);
        

        close(commit_fd);
        sendFileContents(socket_fd, commit_path);
        close(socket_fd);
        free(commit_path);


    } else if (strcmp(cmd, "push") ==  0){

        int socket_fd = config(); //creates connection to server
        if (socket_fd < 0){ printf("config error\n"); exit(0); }

        //sending message to server
        char msg[MAX];
        bzero(msg, sizeof(msg));
        strcpy(msg, cmd);
        strcat(msg, delimiter);
        strcat(msg, argv[2]);
        strcat(msg, delimiter);
        //printf("message sending to server: %s\n", msg);
        write(socket_fd, msg, sizeof(msg));

        //receiving message from server
        bzero(msg, sizeof(msg));
        read(socket_fd, msg, sizeof(msg));
        if (strcmp(msg, "OK") != 0){
            perror("socket error\n"); exit(1);
        }

        //open .commit
        char* commit_path = (char*) malloc( (strlen(argv[2]) + strlen("/Commit.txt\0")) * sizeof(char) );
        strcpy(commit_path, argv[2]);
        strcat(commit_path, "/Commit.txt\0");
        int commit_fd = open(commit_path, O_RDWR | O_APPEND);
        if (commit_fd < 0){ perror("error opening commit file"); exit(0); }

        //send indicated files (modify, add cases)
        while(1){
            char* line = readUntilDelim(commit_fd, '\n');
            if (line == NULL || strcmp(line, "\0") == 0){
                break;
            }
            
            char* token = strtok(line, "\t");
            char* flag = token;

            token = strtok(NULL, "\t");
            char* path = token;

            token = strtok(NULL, "\t");
            char* hash = token;

            token = strtok(NULL, "\t");
            char* ver_num = token;

            if (strcmp(flag, "M") == 0 || strcmp(flag, "A") == 0){
                write(socket_fd, "F", strlen("F"));
                write(socket_fd, delimiter, strlen(delimiter));
                write(socket_fd, path, strlen(path));
                write(socket_fd, delimiter, strlen(delimiter));
                sendFileContents(socket_fd, path);
                write(socket_fd, "\e", strlen("\e"));
                write(socket_fd, delimiter, strlen(delimiter));
            }
        }

        //sending manifest
        char* manifest_path = (char*) malloc( (strlen(argv[2]) + strlen("/.Manifest\0") + 2) * sizeof(char) );
        strcpy(manifest_path, "./");
        strcat(manifest_path, argv[2]);
        strcat(manifest_path, "/.Manifest\0");
        write(socket_fd, "M", strlen("M"));
        write(socket_fd, delimiter, strlen(delimiter));
        write(socket_fd, manifest_path, strlen(manifest_path));
        write(socket_fd, delimiter, strlen(delimiter));
        sendFileContents(socket_fd, manifest_path);
        write(socket_fd, "\e", strlen("\e"));
        write(socket_fd, delimiter, strlen(delimiter));

        //receive success message from server
        bzero(msg, sizeof(msg));
        read(socket_fd, msg, sizeof(msg));
        printf("message from server: %s\n", msg);

        //delete .commit & free everything
        remove(commit_path);
        free(commit_path);
        free(manifest_path);
        close(socket_fd);


    } else if (strcmp(cmd, "create") ==  0){
        int socket_fd = config(); //creates connection to server
        if (socket_fd == -1){ printf("config error\n"); exit(0); }
        else {
            //send message to server
            char msg[MAX];
            bzero(msg, sizeof(msg));
            strcpy(msg, cmd);
            strcat(msg, delimiter);
            strcat(msg, argv[2]);
            strcat(msg, delimiter);
            //printf("message sending to server: %s\n", msg);
            write(socket_fd, msg, sizeof(msg));

            //receiving message from server
            bzero(msg, sizeof(msg));
            read(socket_fd, msg, sizeof(msg));
            //printf("From server: %s\n", msg);

            //initialize folder & Manifest file on client side

            //directory
            int dir = mkdir(argv[2], 0777);
            if (dir < 0){ printf("unable to create directory\n"); exit(1); }

            //Manifest file
            char* path = (char*) malloc( (strlen(argv[2]) + strlen("/.Manifest\0")) * sizeof(char) );
            strcpy(path, argv[2]);
            strcat(path, "/.Manifest\0");
            int manifest_fd = open(path, O_RDWR | O_APPEND | O_CREAT);
            fchmod(manifest_fd, S_IRWXU);
            if (manifest_fd < 0){ printf("manifest creation error\n"); exit(1); }
            write(manifest_fd, "0\n", strlen("0\n"));
            free(path);
        }
        close(socket_fd);
    } else if (strcmp(cmd, "destroy") ==  0){
        int socket_fd = config(); //creates connection to server
        if (socket_fd == -1){ printf("config error\n"); exit(0); }
        else {
            //send message to server
            char msg[MAX];
            bzero(msg, sizeof(msg));
            strcpy(msg, cmd);
            strcat(msg, delimiter);
            strcat(msg, argv[2]);
            strcat(msg, delimiter);
            printf("message sending to server: %s\n", msg);
            // write(socket_fd, "Eshaan", sizeof("Eshaan"));
            write(socket_fd, msg, sizeof(msg));

            //sending my own message to the server to see how it works
            

            //receiving message from server
            bzero(msg, sizeof(msg));
            read(socket_fd, msg, sizeof(msg));
            printf("From server: %s\n", msg);
        }
        //destroy
    } else if (strcmp(cmd, "add") ==  0){
        //open manifest file in specified project
        // printf("Fuck\n");
        char* manifest_path = (char*) malloc( (strlen(argv[2]) + strlen("/.Manifest\0")) * sizeof(char) );
        strcpy(manifest_path, argv[2]);
        strcat(manifest_path, "/.Manifest\0");
        printf("%s\n", manifest_path);
        int manifest_fd = open(manifest_path, O_RDWR | O_CREAT);
        if (manifest_fd < 0){ perror("error opening manifest file"); exit(1); }


        //create path
        char* path = (char*) malloc( (strlen(argv[2])+strlen(argv[3])+4) * sizeof(char) );
        strcpy(path, "./");
        strcat(path, argv[2]);
        strcat(path, "/");
        strcat(path, argv[3]);
        strcat(path, "\0");

        //see if file is already in manifest
        char* manifest_contents = getFileContents(manifest_fd);
        if (strstr(manifest_contents, path) != NULL){
            printf("duplicate file added\n");
            exit(1);
        }

        //change manifest version number
        char mani_ver = manifest_contents[0] + 1;
        lseek(manifest_fd, 0, SEEK_SET);
        write(manifest_fd, &mani_ver, sizeof(mani_ver));

        //create hash
        int file_fd = open(path, O_RDONLY | O_CREAT, 0777);
        if (file_fd < 0) { printf("error opening file %s\n", path); exit(1); }
        char* hash = getFileHash(file_fd);
        
        //initialize version #
        char ver_num[] = "0";

        //printf("path: %s\n\n", path);
        //printf("hash: %s\n\n", hash);
        //printf("version #: %s\n\n", ver_num);
        //printf("line that should be added:\n%s\t%s\t%s\n", path, hash, ver_num);

        //write everything to Manifest file
        lseek(manifest_fd, 0, SEEK_END);
        write(manifest_fd, path, strlen(path));
        write(manifest_fd, "\t", strlen("\t"));
        write(manifest_fd, hash, strlen(hash));
        write(manifest_fd, "\t", strlen("\t"));
        write(manifest_fd, ver_num, strlen(ver_num));
        write(manifest_fd, "\n", strlen("\n"));

        //free(hash);
        free(path);

    } else if (strcmp(cmd, "remove") ==  0){
        //open manifest file in specified project
         /* 
        1. Check if the project exists on the client
        2. find file line in the manifest 
        3. remove the file name     hash    version */

        /* 
        arg[1]=project name
        arg[2]=file name
        first allocate space for them 
        then assign them to it - done*/

        char* project_name=(char *)malloc(sizeof(char)*strlen(argv[2]));
        char* file_name=(char *)malloc(sizeof(char)*strlen(argv[3]));
        strcpy(project_name,argv[2]);
        strcpy(file_name, argv[3]);
        printf("Project name - %s, File name - %s\n", project_name, file_name);

        //now find the project on the client
        struct dirent *de;  // Pointer for directory entry 
  
            // opendir() returns a pointer of DIR type.  
        DIR *dr = opendir(project_name); 
    
        if (dr == NULL)  // opendir returns NULL if couldn't open directory 
            { 
                printf("The command has failed - The project does not exist\n"); 
                exit(1);
            }
        // closedir(dr);    
        //found dr is teh struct that found itWT

        //now open the manifest file
        char* manifest_path=malloc(sizeof(char)*(strlen(project_name)+strlen("/.Manifest.txt")));
         strcat(manifest_path,project_name);
        strcat(manifest_path,"/Manifest.txt"); 
        printf("%s The manifest path is\n",manifest_path );

        //now open the manifest file path baby
        // printf("Who is outputting\n");
        int fd= open(manifest_path, O_RDWR);
        if(fd==-1){
            printf("The manifest file not found\n");
            exit(1);
        }
        // printf("Before put %d\n",fd);
        put(fd);//puts in all the manifest entries 
        // printf("Afetr put\n:");
        // printf("%d the fd baby \n", fd);
        // printf("Who is outputting\n");

        // char* the_read=(char *)malloc(sizeof(char)*1000);
        // int man_read=read(fd,the_read,500);
        // printf("%d the number of bytes read\n", man_read);
        // // the_read[man_read] = '\0';
        // printf("This was read from the file %s\n", the_read);
        //the read is working
        //now tokenize a file
        //use get file contents
        int i=0;
        remove(manifest_path);
        int fd2=open(manifest_path, O_RDWR|O_CREAT,0666);
        
        for(i=0;i<n;i++){
            if(strstr(man[i]->path,file_name)){
                // printf("%s %s\n", man[i]->path,file_name);
            }
            else{
                char* toWrite=malloc(1000*sizeof(char));
                // printf("Fuck\n");
                strcat(toWrite,man[i]->path);
                // printf("Fuck\n");
                strcat(toWrite,"\t");
                // printf("Fuck\n");
                strcat(toWrite,man[i]->hash);
                strcat(toWrite,"\t");
                // printf("Fuck\n");
                char str[20];
                sprintf(str, "%d", man[i]->vers);
                strcat(toWrite,str);
                strcat(toWrite,"\n");
                // printf("Fuck\n");
                write(fd2,toWrite,strlen(toWrite));
            }
        }



    } else if (strcmp(cmd, "currentversion") ==  0){
        /* 
        1. you write to the server the command currentversion - done
        2. The server says ok I see you - done
        3. The server produces the manifest number
        4. The server sends the manifest number
        5. The client accepts the manifest number*/

        int socket_fd = config(); //creates connection to server
        if (socket_fd == -1){ printf("config error\n"); exit(0); }
        else {
            //send message to server
            char msg[MAX];
            bzero(msg, sizeof(msg));
            strcpy(msg, cmd);
            strcat(msg, delimiter);
            strcat(msg, argv[2]);
            strcat(msg, delimiter);
            printf("message sending to server: %s\n", msg);
            // write(socket_fd, "Eshaan", sizeof("Eshaan"));
            write(socket_fd, msg, sizeof(msg));

            //sending my own message to the server to see how it works
            

            //receiving message from server
            bzero(msg, sizeof(msg));
            read(socket_fd, msg, sizeof(msg));
            printf("From server: %s\nThis is the information you need\n", msg);
            bzero(msg, sizeof(msg));
            read(socket_fd,msg,sizeof(msg));
            printf("%s\n",msg);
        }//else closed

        //currentversion
    } else if (strcmp(cmd, "history") ==  0){
        int socket_fd = config(); //creates connection to server
        if (socket_fd < 0){ printf("config error\n"); exit(0); }

        //sending message to server
        char msg[MAX];
        bzero(msg, sizeof(msg));
        strcpy(msg, cmd);
        strcat(msg, delimiter);
        strcat(msg, argv[2]);
        strcat(msg, delimiter);
        //printf("message sending to server: %s\n", msg);
        write(socket_fd, msg, sizeof(msg));

        //History
        char* history_path = (char*) malloc( (strlen(argv[2]) + strlen("/./History\0")) * sizeof(char) );
        strcpy(history_path, argv[2]);
        strcat(history_path, "/./History\0");
        remove(history_path);
        int history_fd = open(history_path, O_RDWR | O_APPEND | O_CREAT, 0777);
        if (history_fd < 0){ printf("history creation error\n"); exit(1); }

        writeFileContents(socket_fd, history_fd);

        lseek(history_fd, 0, SEEK_SET);
        char buffer[100];
        printf("PUSH HISTORY: \n");
        while(1){
            int bytes = read(history_fd, buffer, sizeof(buffer));
            if (bytes == 0) { break; }
            printf("%s", buffer);
        }

    } else if (strcmp(cmd, "rollback") ==  0){
        //rollback
    } else {
        printf("invalid command\n");
        exit(0);
    }
}

/*  Retrieves information from generated configure file to create connection to specified endpoint 
    Returns fd of socket after connecting to server to be written to, -1 if configuration failed */
int config(){
    int socket_fd;

    //initialize socket
    socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if(socket_fd == -1){
        printf("socket creation error\n");
        return -1;
    }
    
    //get information from .configure file
    int config_fd = open(".configure", O_RDONLY, 0777);
    if (config_fd < 0){ perror("configure error"); exit(0); return -1; }
    char* config_str = (char*) malloc(1000*sizeof(char));
    int bytes = read(config_fd, config_str, 1000);
    char* token = strtok(config_str, delimiter);
    char* host_str = token;
    token = strtok(NULL, delimiter);
    int port_num = atoi(token);

    //create object representing endpoint (server) to connect to
    struct hostent* host_name = gethostbyname(host_str); //hostname obj
    free(host_str);
    struct sockaddr_in server_address; //object representing endpoint connecting to
    bzero(&server_address, sizeof(server_address));
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(port_num);
    bcopy((char*)host_name->h_addr, (char*)&server_address.sin_addr.s_addr, host_name->h_length);

    //connect to server
    if (connect(socket_fd, (SA*)&server_address, sizeof(server_address)) != 0){
        printf("connection failed\n");
        return -1;
    } else {
        //printf("connected to server\n");
    }
    return socket_fd; //fd corresponding to socket connected to server specified in .configure
}

/*  Helper function that writes from a socket into a given file until specified escape character is reached */
void writeFileContents(int socket_fd, int file_fd){
    char* c = (char*) malloc(sizeof(char));
    int bytes;
    while(1){
        bzero(c, sizeof(c));
        bytes = read(socket_fd, c, 1);
        //printf("%s", c);
        if (bytes == 0 || strcmp(c, "\e") == 0){
            break;
        } else {
            write(file_fd, c, strlen(c));
        }
    }
    free(c);
}

/*  Helper function that loads contents of a file into a string
    Returns string of file contents */
char* getFileContents(int fd){
    char* ret = NULL;
    int n = 0;
    int byte;
    char c;
    while (1){
        byte = read(fd, &c, 1);
        if (byte == 0){
            break;
        } else {
            ret = (char*) realloc(ret, ++n*sizeof(char));
            ret[n-1] = c;
        }
    }
    return ret;
}

/*  Helper function to compare a char* and a char
    Returns 1 if same, 0 if not */
int samechar(const char *str, char c) {
    return *str == c && (c == '\0' || str[1] == '\0');
}

/*  Reads a file until a specified delimiter is reached 
    Returns char bytes read in from file, NULL if none read in */
char* readUntilDelim(int fd, char delim){
    char* ret = NULL;
    int n = 0;
    int bytes;
    char c;
    while (1){
        ret = (char*) realloc(ret, ++n*sizeof(char));
        bytes = read(fd, &c, 1);
        if (c == delim || bytes == 0){
            ret[n-1] = '\0';
            return ret;
        } else {
            ret[n-1] = c;
        }
    }
}

/* Sends contents of a file to socket specified in args */
void sendFileContents(int socket_fd, char* file){
    int file_fd = open(file, O_RDONLY);
    if (file_fd < 0){ perror("error opening file\n"); exit(1); }
    while(1){
        int bytes = sendfile(socket_fd, file_fd, NULL, 100);
        if (bytes == 0) { write(socket_fd, "\e", strlen("\e")); break; }
        if (bytes < 0) { perror("file transfer error\n"); exit(1); }
    }
}



/*  Generates hashcode for given file's contents
    Returns string containing hashcode */
char* getFileHash(int file_fd){
    SHA_CTX ctx;
    SHA1_Init(&ctx);
    int bytes;
    char* block = (char*) malloc(50*sizeof(char));
    while(1){
        bytes = read(file_fd, block, 50);
        if (bytes == 0){ break; }
        SHA1_Update(&ctx, block, bytes);
    }
    unsigned char temp_hash[SHA_DIGEST_LENGTH];
    SHA1_Final(temp_hash, &ctx);
    static unsigned char hash[SHA_DIGEST_LENGTH*2];
    int i = 0;
    for (i=0; i < SHA_DIGEST_LENGTH; i++) {
        sprintf((char*)&(hash[i*2]), "%02x", temp_hash[i]);
    }
    return hash;
}

/* Function designed to update hashcodes in a Manifest file */
void updateHashes(int manifest_fd){
    char manifest_ver_num;
    read(manifest_fd, &manifest_ver_num, 1);
    lseek(manifest_fd, 2, SEEK_SET);
    int change_made = FALSE;
    while(1){
        char* line = readUntilDelim(manifest_fd, '\n');
        if (line == NULL || strcmp(line, "\0") == 0){
            break;
        }

        char* token = strtok(line, "\t");
        char* path = token;

        token = strtok(NULL, "\t");
        char* hash = token;

        token = strtok(NULL, "\t");
        char* ver_num = token;

        int file_fd = open(path, O_RDONLY, 0777);
        if (file_fd < 1){ printf("error opening file %s\n", path); exit(1); }
        char* curr_hash = getFileHash(file_fd);
        if (strcmp(hash, curr_hash) != 0){ //file hash changed - update hash
            change_made = TRUE;
            ver_num[0] = ver_num[0] + 1;
            lseek(manifest_fd, -(strlen(hash)+3), SEEK_CUR);
            write(manifest_fd, curr_hash, strlen(curr_hash));
            write(manifest_fd, "\t", strlen("\t"));
            write(manifest_fd, ver_num, strlen(ver_num));
            write(manifest_fd, "\n", strlen("\n"));
        }
    }
    if (change_made == TRUE){
        manifest_ver_num = manifest_ver_num + 1;
        lseek(manifest_fd, 0, SEEK_SET);
        write(manifest_fd, &manifest_ver_num, 1);
    }
}

void put(int fd){
     char* version=readUntilDelim(fd,'\n');
    printf("%s thsi is the bersion\n",version);
    n=0;
    //first initicialize the struct
    //first open the manifest file - jk you've already got a fd for it
    int i=0;
    // char *blah=readUntilDelim(fd,'\t');
    //     printf("The string needed is blah %s\n", blah);
    // char* content=getFileContents(fd);//gets the file contents of the manifest and loads it into the 
    // printf("%s\n", content);
    // printf("Seg fault here");
    // man[i]->path=(char *)malloc(1000*sizeof(char));
    while(1){
        man[i]=malloc(sizeof(struct manifestEntry));
        char *blah=readUntilDelim(fd,'\t');//the file path
        // printf("%s \n",blah);
        if(strcmp(blah,"")==0){
            break;
        }
        printf("the seqfal\n");
        // printf("The string needed is blah %s\n", blah);
        // printf("the seqfal\n");
        man[i]->path=malloc(1000*sizeof(char));
        // printf("the seqfal\n");
        strcpy(man[i]->path,blah);
        free(blah);
        blah=readUntilDelim(fd,'\t');//the file hash
        // printf("the seqfal\n");
        man[i]->hash=malloc(1000*sizeof(char));
        strcpy(man[i]->hash,blah);
        // printf("The string needed is blah %s\n", blah);
        blah=readUntilDelim(fd,'\n');//the file version
        man[i]->vers=atoi(blah);
        // printf("The string needed is blah %s\n", blah);
        n++;
        i++;
    }
    for(i=0;i<2;i++){
        // printf("%s\t%s\t%d\n", man[i]->path, man[i]->hash, man[i]->vers);
    }
}