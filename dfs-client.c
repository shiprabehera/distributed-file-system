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

#define MAXBUFSIZE 1024
#define NUMSERVERS 4
#define SERVER 50
#define LISTSIZE 4096
#define MAXFILES 25

struct conf_struct {
    char server_name[NUMSERVERS][SERVER];
    char server_addr[NUMSERVERS][SERVER];
    int ports[NUMSERVERS];
    char username[SERVER];
    char password[SERVER];
};

struct files_list_struct {
    int count;
    char file_name[MAXFILES][MAXBUFSIZE];
    int parts[MAXFILES][4];
};


int get_conf(char *file_name, struct conf_struct *conf_struct) {
    FILE *f;
    f = fopen(file_name, "r");
    if(f != NULL) {
        char file_buffer[MAXBUFSIZE];
        char temp_buffer[MAXBUFSIZE];
        const char delims[8] = " \n";
        int i = 0;
        while(fgets(file_buffer, MAXBUFSIZE, f) != NULL) {
            
            strncpy(temp_buffer, file_buffer, MAXBUFSIZE);
            char *token = strtok(temp_buffer, delims);
            if(strcmp(token, "Server") == 0) {
                token = strtok(NULL, " :\n");
                strcpy(conf_struct->server_name[i], token);
                token = strtok(NULL, " :\n");
                strcpy(conf_struct->server_addr[i], token);
                token = strtok(NULL, " :\n");
                conf_struct->ports[i] = atoi(token);
                i++;
            } else if(strcmp(token, "Username:") == 0) {
                token = strtok(NULL, "\n");
                strcpy(conf_struct->username, token);
            } else {
                token = strtok(NULL, "\n");
                strcpy(conf_struct->password, token);
            }
        }
    } 
    else {
        printf("Cannot open conf file! \n");
        fclose(f);
        return -1;
    }
    
    fclose(f);
    return 1;
}

int main(int argc, char *argv[]) {
    
    // Make sure user passes in a .conf file
    if (argc < 2) {
        perror("USAGE: ./dfs dfc.conf\n");
        return -1;
    }
    char *file_name;
    file_name = argv[1];
    struct conf_struct conf_struct;
    memset(&conf_struct.username, 0, sizeof(conf_struct.username));
    memset(&conf_struct.password, 0, sizeof(conf_struct.password));

    if(get_conf(file_name, &conf_struct) == 1) {
        struct sockaddr_in remote;
        struct files_list_struct file_list;
        memset(&file_list.parts, 0, sizeof(file_list.parts));
        file_list.count = 0;
        
        
        char username[MAXBUFSIZE];
        char password[MAXBUFSIZE];
        char message[MAXBUFSIZE];
        char buffer[MAXBUFSIZE];
        char command[MAXBUFSIZE];
        char path[MAXBUFSIZE];
        char file[MAXBUFSIZE];
        int sockets[4];
        int authenticated = 0;
        struct timeval timeout;
        timeout.tv_sec = 1;
        timeout.tv_usec = 0;
        int nbytes;
    
        for (int i = 0; i < 4; i++) {
            //Create sockets
            if((sockets[i] = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
                fprintf(stderr, "Error creating socket\n");
                return -1;
            }
            
            remote.sin_family = AF_INET;
            remote.sin_port = htons(conf_struct.ports[i]);
            remote.sin_addr.s_addr = inet_addr(conf_struct.server_addr[i]);
            
            //Connect the socket to the server
            if (connect(sockets[i], (struct sockaddr *)&remote, sizeof(remote)) < 0) {
                close(sockets[i]);
                sockets[i] = -1;
            }
        }
    
    
        while(1) {
            memset(&message, 0, MAXBUFSIZE);
            memset(&buffer, 0, MAXBUFSIZE);
            memset(&command, 0, MAXBUFSIZE);
            memset(&path, 0, MAXBUFSIZE);
            memset(&file, 0, MAXBUFSIZE);
            
            if(!authenticated) {
                memset(&username, 0, MAXBUFSIZE);
                memset(&password, 0, MAXBUFSIZE);
                snprintf(message, MAXBUFSIZE, "%s %s", conf_struct.username, conf_struct.password);
                for(int i=0; i<4; i++) {
                    send(sockets[i], message, MAXBUFSIZE, 0);
                    if (setsockopt (sockets[i], SOL_SOCKET, SO_SNDTIMEO, (char *)&timeout,
                                    sizeof(timeout)) < 0)
                        perror("setsockopt failed\n");
                    nbytes = recv(sockets[i], buffer, MAXBUFSIZE, 0);
                    if(nbytes == 0) {
                        printf("Timeout occurred. Server not available.\n");
                        sockets[i] = -1;
                    }
                    printf("%s\n", buffer);
                    if(strcmp(buffer, "Invalid username or password.") == 0) {
                        
                        authenticated = 0;
                        memset(&buffer, 0, MAXBUFSIZE);
                        exit(1);
                        break;
                    } else {
                        authenticated = 1;
                        memset(&buffer, 0, MAXBUFSIZE);
                    }
                }
                if(authenticated) {
                    printf("***USER AUTHENTICATED***\n");
                }
            } else {
                fflush(stdin);
                char *message_for_user = "-------------------\nUSAGE:\nget [FILENAME.ext]\nput [FILENAME.ext]\ndelete [FILENAME.ext]\nlist\nexit\nEnter command to execute on the server: ";
                printf("%s", message_for_user);
                fgets(command, MAXBUFSIZE, stdin);
                int index = 0;
                file[index] = '\n';
                while(command[index] != '\n') {
                    if(command[index] == ' ') {
                        strcpy(file, &command[index + 1]);
                        command[index] = '\0';
                        file[strlen(file)-1] = '\0';
                        break;
                    }
                    index++;
                }
                
                fflush(stdin);
                snprintf(path, MAXBUFSIZE, "/%s/%s", conf_struct.username, file);
                fflush(stdin);
                
                memset(&message, 0, MAXBUFSIZE);
                memset(&buffer, 0, MAXBUFSIZE);
                
                
                
                if(strcmp(command, "exit\n") == 0) {
                    printf("Closing sockets.\n");
                    //Close and cleanup sockets
                    for(int i = 0; i < 4; i++) {
                        if (sockets[i] != -1) {
                            close(sockets[i]);
                        }
                    }
                    exit(0);
                    break;
                }
                else {
                    printf("Invalid command. Acceptable commands are: 'list', 'get', 'put', 'exit'\n");
                }
                
            }
        }
    
        printf("Closing sockets\n");
        //Close and cleanup sockets
        for(int i = 0; i < 4; i++) {
            if (sockets[i] != -1) {
                close(sockets[i]);
            }
        }
        return 0;  
    }
    
}

