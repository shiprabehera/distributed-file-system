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
            
            //Close sockets and terminate child process
            close(client_sock);
            exit(0);
            
        }
    }
    close(socket_desc);
    
    return 0;
}
