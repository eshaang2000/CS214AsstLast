#include <netdb.h> 
#include <stdio.h> 
#include <stdlib.h> 
#include <string.h> 
#include <dirent.h>
#include <sys/socket.h> 
#include<fcntl.h> 
#include<errno.h> 
#include<sys/stat.h>
#include<sys/sendfile.h>
#include <openssl/sha.h>
#include <unistd.h>
#include<signal.h>


#define MAX 1000  
#define SA struct sockaddr
#define TRUE 1
#define FALSE -1
#define delimiter "\n"

void recurDirectory(char* path, int socket_fd, char* cmd);
void sendFileContents(int socket_fd, char* file);
void writeFileContents(int socket_fd, int file_fd);
int samechar(const char *str, char c);
char* readUntilDelim(int fd, char delim);
char* getFileHash(int file_fd);
void updateHashes(int manifest_fd);
char* getFileContents(int fd);
struct manifestEntry{
    char* path;
    char* hash;
    int vers;
};

struct manifestEntry* man[1000];
void put(int fd);
int n;

int sig_handler(int signo)
{
  if (signo == SIGINT){
    printf("\nReceived Signal to quit. Thank You!\n");
     
    exit(0);
    }
return 1;
}




int main(int argc, char* argv[]){
    int socket_fd, connect_fd, len;
    struct sockaddr_in server_address, cli_address;

    //initialize socket
    socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_fd == -1){
        printf("socket creation error\n");
        exit(0);
    } else 
        printf("socket creation successful\n");

    //create object for server addr.
    bzero(&server_address, sizeof(server_address));
    server_address.sin_family = AF_INET;
    server_address.sin_addr.s_addr = htonl(INADDR_ANY);
    int port_num = atoi(argv[1]); //argv[2] == port # of connection
    server_address.sin_port = htons(port_num);

    //bind socket to addr specified by object
    if (bind(socket_fd, (SA*)&server_address, sizeof(server_address)) != 0) {
        printf("socket bind failed\n");
        exit(0);
    } else {
        printf("socket binded successfully\n");
    }

    //listen for connection
    if (listen(socket_fd, 5) != 0){
        printf("listen failed\n");
        exit(0);
    } else 
        printf("server listening\n");




    len = sizeof(cli_address);
while(1){

    // int sig_handler(int signo)
    signal(SIGINT,sig_handler);
        // printf("Boo");
    
//   if (signo == SIGINT){
//     printf("\nReceived Signal to quit. Thank You!\n");
//     exit(0);
//     }
// return 1;
// }

    //accept data from client
    connect_fd = accept(socket_fd, (SA*)&cli_address, &len);
    if (connect_fd < 0) { 
        printf("server accept failed\n");
        // exit(0);
    } else
    {
        printf("server has accepted client\n");
    }

    //get command from message
    char msg[MAX];
    char* temp = NULL;
    read(connect_fd, msg, sizeof(msg));
    printf("full msg: %s\n", msg);
    char* token = strtok_r(msg, delimiter, &temp);
    char* cmd = (char*) malloc(strlen(token)*sizeof(char));
    strcpy(cmd, token);
    //printf("command: %s\n", cmd);
    token = strtok_r(NULL, delimiter, &temp);

    //commands
    if (strcmp(cmd, "checkout") ==  0){
        //checkout
        char* proj_name = (char*) malloc(strlen(token)*sizeof(char));
        strcpy(proj_name, token);
        token = strtok_r(NULL, delimiter, &temp);
        recurDirectory(proj_name, connect_fd, cmd);

        bzero(msg, sizeof(msg));
        read(connect_fd, msg, sizeof(msg));
        if (strcmp(msg, "OK") == 0){
            close(connect_fd);
        }

    } else if (strcmp(cmd, "update") ==  0){
        char* proj_name = (char*) malloc(strlen(token)*sizeof(char));
        strcpy(proj_name, token);
        token = strtok_r(NULL, delimiter, &temp);

        write(connect_fd, "OK", strlen("OK"));

        //receiving message from client
        bzero(msg, sizeof(msg));
        read(connect_fd, msg, sizeof(msg));

        //open client manifest
        char* manifest_path = (char*) malloc( (strlen(proj_name) + strlen("/.Manifest\0")) * sizeof(char) );
        strcpy(manifest_path, proj_name);
        strcat(manifest_path, "/.Manifest\0");
        int manifest_fd = open(manifest_path, O_RDWR | O_CREAT);
        if (manifest_fd < 0){ perror("error opening manifest file"); exit(1); }
        free(manifest_path);

        //compare version #s
        char serv_man_num;
        read(manifest_fd, &serv_man_num, 1);
        if (msg[0] > serv_man_num){
            printf("server does not have latest version - please push\n");
            exit(1);
        }

        //update the server manifest
        lseek(manifest_fd, 0, SEEK_SET);
        updateHashes(manifest_fd);

        //create .update
        char* update_path = (char*) malloc( (strlen(proj_name) + strlen("/.Update\0")) * sizeof(char) );
        strcpy(update_path, proj_name);
        strcat(update_path, "/.Update\0");
        remove(update_path);
        int update_fd = open(update_path, O_RDWR | O_APPEND | O_CREAT);
        if (update_fd < 0){ perror("error opening update file"); exit(0); }
        chmod(update_path, S_IRWXU);

        //searching server manifest for changes
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
                //write add to update
                write(update_fd, "A\t", strlen("A\t"));
                write(update_fd, path, strlen(path));
                write(update_fd, "\t", strlen("\t"));
                write(update_fd, hash, strlen(hash));
                write(update_fd, "\t", strlen("\t"));
                write(update_fd, ver_num, strlen(ver_num));
                write(update_fd, "\n", strlen("\n"));
                printf("A %s\n", path);

            } else {
                char* temp = strstr(msg, path);
                char server_hash[SHA_DIGEST_LENGTH*2+1];
                memcpy(server_hash, &temp[1], SHA_DIGEST_LENGTH*2);
                server_hash[SHA_DIGEST_LENGTH*2] = '\0';
                if (strcmp(hash, server_hash) != 0){ //modify case
                    write(update_fd, "M\t", strlen("A\t"));
                    write(update_fd, path, strlen(path));
                    write(update_fd, "\t", strlen("\t"));
                    write(update_fd, hash, strlen(hash));
                    write(update_fd, "\t", strlen("\t"));
                    write(update_fd, ver_num, strlen(ver_num));
                    write(update_fd, "\n", strlen("\n"));
                    printf("M %s\n", path);
                }
            }
        }

        int i;
        char* line = NULL;
        int n = 0;
        lseek(manifest_fd, 0, SEEK_SET);
        char* serv_manifest_contents = getFileContents(manifest_fd);
        
        //searching client manifest for changes
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

                if (strstr(serv_manifest_contents, path) == NULL){ //delete case
                    //write delete to commit
                    write(update_fd, "D\t", strlen("A\t"));
                    write(update_fd, path, strlen(path));
                    write(update_fd, "\t", strlen("\t"));
                    write(update_fd, hash, strlen(hash));
                    write(update_fd, "\t", strlen("\t"));
                    write(update_fd, ver_num, strlen(ver_num));
                    write(update_fd, "\n", strlen("\n"));
                    printf("D %s\n", path);
                }
                line = NULL;
                n = 0;

            } else {
                line[n-1] = c;
            }
        }
        free(serv_manifest_contents);
        free(line);

        close(update_fd);
        sendFileContents(connect_fd, update_path);
        close(connect_fd);
        free(update_path);

    } else if (strcmp(cmd, "upgrade") ==  0){
        //printf("inside upgrade\n");
        char* proj_name = (char*) malloc(strlen(token)*sizeof(char));
        strcpy(proj_name, token);
        token = strtok_r(NULL, delimiter, &temp);

        //printf("%s\n", proj_name);

        char* update_path = (char*) malloc( (strlen(proj_name) + strlen("/.Update\0")) * sizeof(char) );
        strcpy(update_path, proj_name);
        strcat(update_path, "/.Update\0");
        //printf("%s\n", update_path);
        int update_fd = open(update_path, O_RDONLY);
        if (update_fd < 0){ perror("error opening update file"); exit(0); }

        //send indicated files (modify, add cases)
        while(1){
            char* line = readUntilDelim(update_fd, '\n');
            printf("%s", line);
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
                write(connect_fd, "F", strlen("F"));
                write(connect_fd, delimiter, strlen(delimiter));
                write(connect_fd, path, strlen(path));
                write(connect_fd, delimiter, strlen(delimiter));
                sendFileContents(connect_fd, path);
                write(connect_fd, "\e", strlen("\e"));
                write(connect_fd, delimiter, strlen(delimiter));
            }
        }
        //write(connect_fd, "M", 1);
       char* manifest_path = (char*) malloc( (strlen(proj_name) + strlen("/.Manifest\0") + 2) * sizeof(char) );
        strcpy(manifest_path, "./");
        strcat(manifest_path, proj_name);
        strcat(manifest_path, "/.Manifest\0");
        char* entry = (char*) malloc( (strlen(manifest_path) + strlen(delimiter) + strlen(delimiter) + strlen("M"))*sizeof(char));
        strcpy(entry, "M");
        strcat(entry, delimiter);
        strcat(entry, manifest_path);
        strcat(entry, delimiter);
        write(connect_fd, entry, strlen(entry));
        sendFileContents(connect_fd, manifest_path);
        write(connect_fd, "\e", strlen("\e"));
        write(connect_fd, delimiter, strlen(delimiter));

        free(manifest_path);
        remove(update_path);
        free(update_path);
        free(proj_name);
        close(connect_fd);

    } else if (strcmp(cmd, "commit") ==  0){
        
        //send project manifest to client
        char* proj_name = (char*) malloc(strlen(token)*sizeof(char));
        strcpy(proj_name, token);
        token = strtok_r(NULL, delimiter, &temp);
        char* path = (char*) malloc( (strlen(proj_name) + strlen("/.Manifest\0")) * sizeof(char) );
        strcpy(path, proj_name);
        strcat(path, "/.Manifest\0");
        sendFileContents(connect_fd, path);

        //write commit file written by client to server project folder
        char* commit_path = (char*) malloc( (strlen(proj_name) + strlen("/Commit.txt\0")) * sizeof(char) );
        strcpy(commit_path, proj_name);
        strcat(commit_path, "/Commit.txt\0");
        remove(commit_path);
        int commit_fd = open(commit_path, O_RDWR | O_APPEND | O_CREAT);
        if (commit_fd < 0){ perror("error opening commit file"); exit(0); }
        chmod(commit_path, S_IRWXU);
        writeFileContents(connect_fd, commit_fd);
        free(path);
        free(proj_name);

    } else if (strcmp(cmd, "push") ==  0){

        char* proj_name = (char*) malloc(strlen(token)*sizeof(char));
        strcpy(proj_name, token);
        token = strtok_r(NULL, delimiter, &temp);

        write(connect_fd, "OK", strlen("OK"));

        //receiving message from client
        char c;
        int bytes;
        int terminate = FALSE;

        //reads incoming message from server & creates appropriate files/dirs on server side
        //message format: "D:<dir_name>:F:<filename>:<file contents>:...."
        while(1){
            bytes = read(connect_fd, &c, 1);
            //printf("%c", c);
            if (bytes == 0) { break; }
            else if (bytes < 0) { perror("error reading file\n"); exit(1); }
            if (samechar(delimiter, c) == 1)
                continue;
            else if (c == 'D'){ //incoming directory
                //printf("\nnow creating directory\n");
                bytes = read(connect_fd, &c, 1);
                //printf("%c", c);
                if (samechar(delimiter, c) == 1){
                    //OK
                }
                char dir_name[500];
                int n = 0;
                while (n < 499){
                    bytes = read(connect_fd, &c, 1);
                    //printf("%c", c);
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
                bytes = read(connect_fd, &c, 1);
                //printf("%c", c);
                if (samechar(delimiter, c) == 1){
                    //OK
                }
                char f_name[500];
                int n = 0;
                while (n < 499){
                    bytes = read(connect_fd, &c, 1);
                    //printf("%c", c);
                    if (samechar(delimiter, c) == 1){ break; }
                    f_name[n++] = c;
                }
                f_name[n] = '\0';
                //printf("client now creating file: %s\n", f_name);
                remove(f_name);
                int file_fd = open(f_name, O_RDWR | O_CREAT);
                chmod(f_name, S_IRWXU);
                if (file_fd < 0){ printf("error opening file %s\n", f_name); exit(1); }
                else {
                    //printf("writing file contents of %s\n", f_name);
                    writeFileContents(connect_fd, file_fd);
                    //printf("writing done\n");
                    if (terminate == TRUE){
                        break;
                    }
                    continue;
                }
            }
        }

        //open .commit
        char* commit_path = (char*) malloc( (strlen(proj_name) + strlen("/Commit.txt\0")) * sizeof(char) );
        strcpy(commit_path, proj_name);
        strcat(commit_path, "/Commit.txt\0");
        int commit_fd = open(commit_path, O_RDWR | O_APPEND);
        if (commit_fd < 0){ perror("error opening commit file"); exit(0); }
        
        //find files for deletion in .commit
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

            if (strcmp(flag, "D") == 0){
                remove(path);
            }
        }

        //History
        char* history_path = (char*) malloc( (strlen(proj_name) + strlen("/.History\0")) * sizeof(char) );
        strcpy(history_path, proj_name);
        strcat(history_path, "/.History\0");
        int history_fd = open(history_path, O_RDWR | O_APPEND | O_CREAT, 0777);
        if (history_fd < 0){ printf("history creation error\n"); exit(1); }
        
        lseek(commit_fd, 0, SEEK_SET);
        writeFileContents(commit_fd, history_fd);
        write(commit_fd, "\n\n", strlen("\n"));

        remove(commit_path);
        free(commit_path);
        free(history_path);
        free(proj_name);

        
        //send success message
        write(connect_fd, "push successful\n", strlen("push successful\n"));
    } else if (strcmp(cmd, "create") ==  0){
        //initialize folder & Manifest file on server side

        //directory
        char* proj_name = (char*) malloc(strlen(token)*sizeof(char));
        strcpy(proj_name, token);
        token = strtok_r(NULL, delimiter, &temp);
        //printf("project name: %s\n", proj_name);
        int dir = mkdir(proj_name, 0777);
        if (dir < 0){ printf("unable to create directory\n"); exit(1); }

        //Manifest
        char* path = (char*) malloc( (strlen(proj_name) + strlen("/.Manifest\0")) * sizeof(char) );
        strcpy(path, proj_name);
        strcat(path, "/.Manifest\0");
        int manifest_fd = open(path, O_RDWR | O_CREAT, 0777);
        if (manifest_fd < 0){ printf("manifest creation error\n"); exit(1); }
        write(manifest_fd, "0\n", strlen("0\n"));

        //History
        char* history_path = (char*) malloc( (strlen(proj_name) + strlen("/.History\0")) * sizeof(char) );
        strcpy(history_path, proj_name);
        strcat(history_path, "/.History\0");
        int history_fd = open(history_path, O_RDWR | O_APPEND | O_CREAT, 0777);
        if (history_fd < 0){ printf("history creation error\n"); exit(1); }
        //write(history_fd, "0\n\n", strlen("0\n\n"));

        //write back to client that project was successfully created
        write(connect_fd, "project folder created\n", strlen("project folder created\n"));
        free(path);
        free(history_path);
        free(proj_name);

    } else if (strcmp(cmd, "destroy") ==  0){
        printf("We are now in destroy\n");//indication that it has reached destroy in the server
        write(connect_fd, "Executing command\n", strlen("Executing command\n"));
        printf("The project name is - %s\n", token);
        deleteFilesRecursively(token);
        //open the manifest and read the first line
        write(connect_fd, "Command received\n", strlen("Command received\n"));

        //destroy
    } else if (strcmp(cmd, "add") ==  0){
        //add
    } else if (strcmp(cmd, "remove") ==  0){
        //remove
    } else if (strcmp(cmd, "currentversion") ==  0){
         // printf("Tell the client I see him\n");
        printf("The project name is - %s\n", token);
        //open the manifest and read the first line
        write(connect_fd, "Command received\n", strlen("Command received\n"));
        char* path = (char*) malloc( (strlen(token) + strlen("/.Manifest\0")) * sizeof(char) );
        strcpy(path, token);
        strcat(path, "/.Manifest\0");
        printf("The path of the manifest is as follows %s\n", path);
        int fd=open(path,O_RDONLY);
        printf("The fd is %d\n",fd);
        // char* data=readUntilDelim(fd,'\t');
        // printf("The data is %s\nbhljfldk\n",data);
        // char* data_to_be_sent=malloc(1000*sizeof(char));
        // printf("%s the data to be sent\n",data_to_be_sent);
        // write(connect_fd, data_to_be_sent, strlen(data_to_be_sent));
        char* toWrite=malloc(1000*sizeof(char));
        if(toWrite==NULL){
            printf("Memory fail to allocate");
        }
        printf("Reaches here");
        put(fd);
        printf("%d the number",n);
        int i;
        for(i=0;i<n;i++){
            
                
                // printf("Fuck\n");
                strcat(toWrite,man[i]->path);
                // printf("Fuck\n");
                strcat(toWrite,"\t");
                // printf("Fuck\n");
                // strcat(toWrite,man[i]->hash);
                // strcat(toWrite,"\t");
                // printf("Fuck\n");
                char str[20];
                sprintf(str, "%d", man[i]->vers);
                strcat(toWrite,str);
                strcat(toWrite,"\n");
                // printf("Fuck\n");
            
        }

             write(connect_fd,toWrite,strlen(toWrite));



        //currentversion
    } else if (strcmp(cmd, "history") ==  0){
        char* proj_name = (char*) malloc(strlen(token)*sizeof(char));
        strcpy(proj_name, token);
        token = strtok_r(NULL, delimiter, &temp);

        char* history_path = (char*) malloc( (strlen(proj_name) + strlen("/.History\0")) * sizeof(char) );
        strcpy(history_path, proj_name);
        strcat(history_path, "/.History\0");

        sendFileContents(connect_fd, history_path);
        free(history_path);


    } else if (strcmp(cmd, "rollback") ==  0){
        //rollback
    } else {
        printf("invalid command\n");
        exit(0);
    }
}
    close(socket_fd);
    // free(cmd);

    /*
    //write message to client
    bzero(msg, MAX);
    strcpy(msg, "skeet\n");
    write(connect_fd, msg, sizeof(msg));

    close(socket_fd);*/

    
}

void recurDirectory(char* path, int socket_fd, char* cmd){
    char currPath[1000];
    struct dirent *dir;
    DIR *d = opendir(path);
    if (d){
        char* temp = (char*) malloc( (strlen(path)+3) * sizeof(char));
        strcpy(temp, "D");
        strcat(temp, delimiter);
        strcat(temp, path);
        strcat(temp, delimiter);
        write(socket_fd, temp, strlen(temp));
        while ((dir = readdir(d)) != NULL){
            if (strcmp(dir->d_name, ".") != 0 && strcmp(dir->d_name, "..") != 0){
                bzero(currPath, 1000);
                strcpy(currPath, path);
                strcat(currPath, "/");
                strcat(currPath, dir->d_name);
                //printf("%s\n", currPath);
                recurDirectory(currPath, socket_fd, cmd);
            }
        }
        free(temp);
        closedir(d);
    } else { //likely a file
        //add filename & file contents to message to be yeeted over
        char* temp = (char*) malloc( (strlen(path)+3) * sizeof(char));
        strcpy(temp, "F");
        strcat(temp, delimiter);
        strcat(temp, path);
        strcat(temp, delimiter);
        write(socket_fd, temp, strlen(temp));
        sendFileContents(socket_fd, path);
        write(socket_fd, "\e", strlen("\e"));
        write(socket_fd, delimiter, strlen(delimiter));
        free(temp);
    }
}

/* Sends contents of a file to socket specified in args */
void sendFileContents(int socket_fd, char* file){
    int file_fd = open(file, O_RDONLY);
    lseek(file_fd, 0, SEEK_SET);
    if (file_fd < 0){ perror("error opening file\n"); exit(1); }
    while(1){
        int bytes = sendfile(socket_fd, file_fd, NULL, 100);
        if (bytes == 0) { break; }
        if (bytes < 0) { perror("file transfer error\n"); exit(1); }
    }
}

/*  Helper function that writes from a socket into a given file until specified escape character is reached */
void writeFileContents(int socket_fd, int file_fd){
    char* c = (char*) malloc(sizeof(char));
    int bytes;
    while(1){
        bzero(c, sizeof(c));
        bytes = read(socket_fd, c, 1);
        if (bytes == 0 || strcmp(c, "\e") == 0){
            break;
        } else {
            write(file_fd, c, strlen(c));
        }
    }
    free(c);
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

        int file_fd = open(path, O_RDONLY);
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

void deleteFilesRecursively(char *basePath)
{
    char path[1000];
    struct dirent *dp;
    DIR *dir = opendir(basePath);

    // Unable to open directory stream
    if (!dir)
        return;

    while ((dp = readdir(dir)) != NULL)
    {
        if (strcmp(dp->d_name, ".") != 0 && strcmp(dp->d_name, "..") != 0)
        {
            // printf("This is getting removed %s\n", dp->d_name);
            // if(remove(checkout_test/yeet1.txt)==0){
            //     printf("success bitch\n");
            // }

            // Construct new path from our base path
            strcpy(path, basePath);
            strcat(path, "/");
            strcat(path, dp->d_name);

            deleteFilesRecursively(path);
            remove(path);
            printf("This is getting removed %s\n", dp->d_name);
        }
    }
    remove(basePath);
    closedir(dir);
}

void put(int fd){
    char* version=readUntilDelim(fd,'\n');
    // printf("%s thsi is the bersion\n",version);
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
        if(blah==NULL){
            break;
        }
        if(strcmp(blah,"")==0){
            break;
        }
        // printf("the seqfal\n");
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
    // for(i=0;i<2;i++){
    //     // printf("%s\t%s\t%d\n", man[i]->path, man[i]->hash, man[i]->vers);
    // }
}