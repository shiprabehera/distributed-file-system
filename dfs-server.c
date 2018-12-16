#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <signal.h>
#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/time.h>
#include <stdlib.h>
#include <memory.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>

#define MAXBUFSIZE 4000
#define MAXUSERBUFSIZE 30
#define MAXUSERS 10
#define MAXFILES 25

struct conf_struct {
	char user_name[MAXUSERS][MAXUSERBUFSIZE];
	char password[MAXUSERS][MAXUSERBUFSIZE];
};

struct files_list_struct {
    int count;
    char file_name[MAXFILES][MAXBUFSIZE];
    int partA;
    int partB;
};

int get_conf(char *file_name, struct conf_struct *conf_struct) {
    FILE *f;
    f = fopen(file_name, "r");

    //If successfully opened file
    if(f != NULL) {
        char file_buffer[MAXBUFSIZE];
        char temp_user[MAXUSERBUFSIZE];
        char temp_pass[MAXUSERBUFSIZE];
        int i = 0;
        while(fgets(file_buffer, MAXBUFSIZE, f) != NULL) {
        	memset(&temp_user, 0, MAXUSERBUFSIZE);
        	memset(&temp_pass, 0, MAXUSERBUFSIZE);
        	sscanf(file_buffer, "%s %s", temp_user, temp_pass);
        	strcpy(conf_struct->user_name[i], temp_user);
        	strcpy(conf_struct->password[i], temp_pass);
        	i++;
        	if(i == MAXUSERS) {
        		break;
        	}
        }
    } else {
        fclose(f);
        return -1;
    }

    fclose(f);
    return 1;
}

void list(int sock, char *path) {
    int max = 0;
    FILE *f;
    int nbytes;
    char ls[MAXBUFSIZE];
    snprintf(ls, MAXBUFSIZE, "ls -a %s", path);
    
    f = popen(ls, "r");
    char buf[MAXBUFSIZE];
    while(fgets(buf, sizeof(buf), f) != 0) {
        max++;
    }
    pclose(f);
    nbytes = send(sock, &max, sizeof(max), 0);
    f = popen(ls, "r");
    if(max > 2) {
        while(fgets(buf, sizeof(buf), f) != 0) {
            send(sock, buf, MAXBUFSIZE, 0);
        }
    }   
    
}

void put(int sock, char* path, char* file_name, int file_part, struct files_list_struct *file_list) {
    FILE *f;
    char server_path[MAXBUFSIZE];
    char file_buffer[MAXBUFSIZE];
    char return_message[MAXBUFSIZE];

    int nbytes;
    unsigned char ch;

    snprintf(server_path, MAXBUFSIZE, "./%s.%s.%d", path, file_name, file_part);
    printf("Server path is %s", server_path);
    f = fopen(server_path, "w+b");
    if(f != NULL) {
        int total = 0;
        long file_size = 0;

        //Receive the size of the file
        recv(sock, &file_size, sizeof(file_size), 0);
        while(total < file_size) {
            nbytes = recv(sock, &ch, sizeof(ch), 0);
            if(nbytes < 0) {
                perror("error receiving");
            }
            fputc(ch, f);
            total += nbytes;
            memset(&file_buffer, 0, MAXBUFSIZE);
        }
        printf("File written\n");
        fclose(f);

        //Check if we already have this entry
        if(strcmp(file_list->file_name[file_list->count], file_name) == 0) {
            file_list->partB = file_part;
            file_list->count++;
        } else {
            strcpy(file_list->file_name[file_list->count], file_name);
            file_list->partA = file_part;
        }
        
    } else {
        snprintf(return_message, MAXBUFSIZE, "File write not successful.");
        send(sock, return_message, MAXBUFSIZE, 0);
    }
}

void get(int sock, char *path, char *file_name) {
    FILE *f;
    int total = 0;
    int nbytes;
    char file_buffer[MAXBUFSIZE];
    char path_buffer[MAXBUFSIZE];
    unsigned char ch;
    char return_message[MAXBUFSIZE];
    char server_path[MAXBUFSIZE];
    char buffer[MAXBUFSIZE];
    snprintf(server_path, MAXBUFSIZE, "./%s.%s.", path, file_name);
    int i;
    int file_part = 0;
    for(i = 1; i <= 4; i++) {
        sprintf(buffer, "%s%d", server_path, i);
        if((f = fopen(buffer, "r")) != NULL) {
            file_part = file_part*10 + i;
        }
        fclose(f);
    }
    // now we have a 2 digit number
    send(sock, &file_part, sizeof(file_part), 0);
    
    // send first file then second file
    strcpy(path_buffer, server_path);
    snprintf(server_path, MAXBUFSIZE, "%s%d", server_path, file_part/10);
    f = fopen(server_path, "r+b");
    if(f != NULL) {
        fseek(f, 0, SEEK_END);
        long file_size = ftell(f);
        rewind(f);
        send(sock, &file_size, sizeof(file_size), 0);
        
        while(total < file_size) {
            //read in the file in blocks of MAXBUFSIZE, then send to client
            ch = fgetc(f);
            nbytes = send(sock, &ch, sizeof(ch), 0);
            if(nbytes == -1) {
                printf("Error sending file: %s\n", strerror(errno));
                break;
            }
            
            total += nbytes;
            
            //Clear the file buffer
            //Useful for when data sent is less than MAXBUFSIZE (i.e. last packet)
            memset(&file_buffer, 0, MAXBUFSIZE);
        }
    } else {
        snprintf(return_message, MAXBUFSIZE, "Invalid file name. File does not exist.");
        send(sock, return_message, MAXBUFSIZE, 0);
    }
    fclose(f);
    total = 0;
    int modFile = file_part % 10;
    snprintf(path_buffer, MAXBUFSIZE, "%s%d", path_buffer, modFile);
    strcpy(server_path, path_buffer);
    f = fopen(server_path, "r+b");
    if(f != NULL) {
        fseek(f, 0, SEEK_END);
        long file_size = ftell(f);
        rewind(f);
        send(sock, &file_size, sizeof(file_size), 0);
        
        while(total < file_size) {
            //read in the file in blocks of MAXBUFSIZE, then send to client
            ch = fgetc(f);
            nbytes = send(sock, &ch, sizeof(ch), 0);
            if(nbytes == -1) {
                printf("Error sending file\n");
                break;
            }
            
            total += nbytes;
            
            //Clear the file buffer
            //Useful for when data sent is less than MAXBUFSIZE (i.e. last packet)
            memset(&file_buffer, 0, MAXBUFSIZE);
        }
    } else {
        snprintf(return_message, MAXBUFSIZE, "Invalid file name. File does not exist.");
        send(sock, return_message, MAXBUFSIZE, 0);
    }
    fclose(f);
}
int main(int argc , char *argv[]) {
    if(argc < 3) {
        printf("USAGE: ./dfs [FOLDER] [PORT NUMBER]\n");
        return 1;
    }
    char *file_name;
    file_name = "./dfs.conf";
    struct conf_struct conf_struct;
    //Read conf file into conf struct
    int status = get_conf(file_name, &conf_struct);
    int errnum = 0;
    if(status == -1) {
        fprintf(stderr, "Error opening file %s: %s\n", file_name, strerror(errnum));
        return 1;
    }
    int port;    
    int length = 0;
    char path[MAXBUFSIZE];
    length += snprintf(path, MAXBUFSIZE, "%s", argv[1]);
    port = atoi(argv[2]);
    int socket_desc, client_sock, c, read_size;
    struct sockaddr_in server, client;   
    struct files_list_struct file_list;
    file_list.count = 0;
    pid_t pid;
    
    
    //Create directory for users if none exists
    struct stat st = {};
    if(stat(path, &st) == -1) {
        mkdir(path, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
    }
    
    //Create socket
    socket_desc = socket(AF_INET , SOCK_STREAM , 0);
    if (socket_desc == -1) {
        printf("Could not create socket");
    }
    
    //Alow the socket to be used immediately
    //This resolves "Address already in use" error
    int option = 1;
    setsockopt(socket_desc, SOL_SOCKET, SO_REUSEADDR, &option, sizeof(option));
    
    //Prepare the sockaddr_in structure
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = INADDR_ANY;
    server.sin_port = htons( port );
    
    //Bind
    if( bind(socket_desc,(struct sockaddr *)&server , sizeof(server)) < 0) {
        //print the error message
        perror("bind failed. Error");
        return 1;
    }
    
    //Listen
    if ( listen(socket_desc , 3) < 0 ) {
        perror("Listen failed. Error");
        return 1;
    }
    
    //Accept and incoming connection
    c = sizeof(struct sockaddr_in);
    
    while(1) {
        char client_message[MAXBUFSIZE];
        char user_cmd[MAXBUFSIZE];
        char user_file[MAXBUFSIZE];
        char user_name[MAXBUFSIZE];
        char user_pwd[MAXBUFSIZE];
        char file_part[MAXBUFSIZE];
        char return_msg[MAXBUFSIZE];
        
        //accept connection from an incoming client
        client_sock = accept(socket_desc, (struct sockaddr *)&client, (socklen_t*)&c);
        if (client_sock < 0) {
            perror("accept failed");
            return 1;
        }
        
        //Fork a child process to handle requests
        if ((pid = fork()) == 0) {
            
            memset(&user_name, 0, MAXBUFSIZE);
            memset(&user_pwd, 0, MAXBUFSIZE);
            //Child closes the listening socket
            close(socket_desc);
            
            //Do some stuff with client_sock
            int authenticated = 0;
            
            //Compare user_name and password to conf
            //Make a folder for the user if one doesn't exist
            while(!authenticated) {
                recv(client_sock, client_message, MAXBUFSIZE, 0);
                sscanf(client_message, "%s %s", user_name, user_pwd);
                for(int i=0; i<MAXUSERS; i++) {
                    //printf("%s", user_name);
                    //printf("%s", user_pwd);
                    if( (strcmp(conf_struct.user_name[i], user_name) == 0)
                       && (strcmp(conf_struct.password[i], user_pwd) == 0) ) {
                        authenticated = 1;
                        struct stat st = {};
                        length += snprintf(path+length, MAXBUFSIZE-length, "/%s/", conf_struct.user_name[i]);
                        //Check status of existing directory
                        if(stat(path, &st) == -1) {
                            mkdir(path, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
                        }
                        snprintf(return_msg, MAXBUFSIZE, "Successfully authenticated.");
                        
                        send(client_sock, return_msg, MAXBUFSIZE, 0);
                        break; //stop comparing once we find a match
                    }
                }
                if(!authenticated) {
                    snprintf(return_msg, MAXBUFSIZE, "Invalid Username/Password. Please try again.");
                    send(client_sock, return_msg, MAXBUFSIZE, 0);
                }
                memset(&client_message, 0, MAXBUFSIZE);
            }
            //Receive a message from client
            while( (read_size = recv(client_sock , client_message , MAXBUFSIZE , 0)) > 0 ) {
 
                //clear the buffers
                memset(&user_cmd, 0, MAXBUFSIZE);
                memset(&user_file, 0, MAXBUFSIZE);
                memset(&file_part, 0, MAXBUFSIZE);
                
                int file_partNum = 0;
                
                
                //read in the client request in format "[COMMAND] [/PATH/FILE] [PART NUMBER]"
                sscanf(client_message, "%s %s %s", user_cmd, user_file, file_part);
                file_partNum = atoi(file_part);
                
                char extracted_path[MAXBUFSIZE];
                char extracted_file[MAXBUFSIZE];
                //Extract path and file_name into two parts
                //Find the last occurence in the string
                const char *dot = strrchr(user_file, '/');
                if(dot == NULL) {
                    // strcpy(dot, "\n");
                }
                char* token = strtok(user_file, dot);
                snprintf(extracted_path, MAXBUFSIZE, "%s", token);
                snprintf(extracted_file, MAXBUFSIZE, "%s", dot+1);
                printf("Path is %s ", path);
                //Test against user commands and process requests
                if( (strcmp(user_cmd, "LIST") == 0) || (strcmp(user_cmd, "list") == 0) ) {
                    list(client_sock, path);
                } else if( (strcmp(user_cmd, "PUT") == 0) || (strcmp(user_cmd, "put") == 0) ) {
                    put(client_sock, path, extracted_file, file_partNum, &file_list);
                } else if( (strcmp(user_cmd, "GET") == 0) || (strcmp(user_cmd, "get") == 0) ) {
                    get(client_sock, path, extracted_file);
                }
                
                memset(&client_message, 0, MAXBUFSIZE);
            }
            
            if(read_size == 0) {
                // printf("Client disconnected: %d\n", client_sock);
                fflush(stdout);
            } else if(read_size == -1) {
                perror("recv failed");
            }
            //Close sockets and terminate child process
            close(client_sock);
            exit(0);
            
        }
    }
    close(socket_desc);
    
    return 0;
}
