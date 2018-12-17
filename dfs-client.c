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


void list(int sock[], struct files_list_struct *file_list) {
    int completeFile = 0;
    char message[MAXBUFSIZE];
    char buf[MAXBUFSIZE];
    char file_buffer[MAXBUFSIZE];
    int emptyFlag = 0;
    int count;
    int fileCount = 0;
    int part;
    int k = 0;
    int index = 0;
    snprintf(message, MAXBUFSIZE, "LIST / /");
    int fileFlag = 0;
    printf("here ------1 \n");
    memset(&file_list->file_name, 0, sizeof(file_list->file_name));
    memset(&file_list->parts, 0, sizeof(file_list->parts));
    file_list->count = 0;
    
    for(int i=0; i<4; i++) {
        index = 0;
        if(sock[i] != -1) {
            send(sock[i], message, MAXBUFSIZE, 0);
            if(recv(sock[i], &count, sizeof(count), 0) <= 0) {
                continue;;
            }
            printf("here ------5 \n");
            if(count == 2) {
                emptyFlag = 1;
            }
            else {
                emptyFlag = 0;
                for (int j = 0; j<count; j++) {
                    //receive file_name;
                    memset(&buf, 0, MAXBUFSIZE);
                    recv(sock[i], buf, MAXBUFSIZE, 0);
                    printf("here ------6 \n");
                    if(strcmp(buf, ".\n") == 0) {
                        continue;
                    }
                    else if(strcmp(buf, "..\n") == 0) {
                        continue;
                    }
                    else {
                        snprintf(file_buffer, MAXBUFSIZE, "%s", &buf[1]);
                        k = strlen(file_buffer) - 1;
                        while(k >= 0) {
                            if(file_buffer[k] == '.') {
                                part = file_buffer[k+1] - '0';
                                file_buffer[k] = '\0';
                                break;
                            }
                            k--;
                        }
                    }
                    //printf("%s\t", file_buffer);
                    //printf("%d\n", part);
                    if(fileFlag == 0) {
                        snprintf(file_list->file_name[index], MAXBUFSIZE, "%s", file_buffer);
                        fileCount++;
                        fileFlag = 1;
                    }
                    else {
                        if(strcmp(file_list->file_name[index], file_buffer) != 0) {
                            //printf("%s:%s\n", file_list->file_name[index], file_buffer);
                            index++;
                            fileCount++;
                            snprintf(file_list->file_name[index], MAXBUFSIZE, "%s", file_buffer);
                        }
                    }
                    //printf("%d:%d:%d\n", index, fileCount, part);
                    file_list->parts[index][part-1] = 1;
                    if(index > file_list->count) {
                        file_list->count = index;
                    }
                }
            }
        }
        
    }
    printf("here ------2 \n");
    if(emptyFlag == 1) {
        printf(".\n..\n");
    }
    else {
        printf("here ------3 \n");
        for(int i=0; i<=file_list->count; i++) {
            //Check if we have all four parts
            if(file_list->parts[i][0] == 1 && file_list->parts[i][1] == 1
               && file_list->parts[i][2] == 1 && file_list->parts[i][3] == 1) {
                
                completeFile = 1;
                file_list->parts[i][0] = 0;
                file_list->parts[i][1] = 0;
                file_list->parts[i][2] = 0;
                file_list->parts[i][3] = 0;
            }
            
            // Add "incomplete" to file name if we don't have enough parts
            if(!completeFile) {
                //printf("here ------4 \n");
                strcat(file_list->file_name[i], " [incomplete]");
            }
            strcat(file_list->file_name[i], "\n");
            printf("%d. %s", i+1, file_list->file_name[i]);
        }
    }
    //printf("Going back to client main()\n");
}

void send_file(int sock, int j, long part_size, FILE *part[4], struct conf_struct *conf_struct, long remainder) {
    char file_buffer[MAXBUFSIZE];
    unsigned char ch;
    
    int total = 0;
    unsigned char key[MAXBUFSIZE];
    int i = 0;
    int nbytes = 0;
    while(conf_struct->username[i] != '\0') {
        key[i] = (unsigned char)conf_struct->username[i];
        i++;
    }
    int k = 0;
    while(conf_struct->password[k] != '\0') {
        key[i] = (unsigned char)conf_struct->password[k];
        k++;
        i++;
    }
    key[i] = '\0';
    
    
    unsigned long key_index = 0;
    //Set file pointers to beginning of file
    rewind(part[0]);
    rewind(part[1]);
    rewind(part[2]);
    rewind(part[3]);
    //Send file
    while(total < part_size) {
        if(j == 1) {
            ch = fgetc(part[0]);
        } else if(j == 2) {
            ch = fgetc(part[1]);
        } else if(j == 3) {
            ch = fgetc(part[2]);
        } else {
            ch = fgetc(part[3]);
        }
        if(key_index < strlen(conf_struct->username) + strlen(conf_struct->password)) {
            ch = ch ^ key[key_index++];
        }
        nbytes = send(sock, &ch, sizeof(ch), 0);
        
        total += nbytes;
        memset(&file_buffer, 0, MAXBUFSIZE);
    }
    while(total < part_size+remainder && j == 4) {
        
        ch = fgetc(part[3]);
        if(key_index < (strlen(conf_struct->username) + strlen(conf_struct->password))) {
            ch = ch ^ key[key_index++];
        }
        nbytes = send(sock, &ch, sizeof(ch), 0);
        total += nbytes;
        memset(&file_buffer, 0, MAXBUFSIZE);
    }    
}
void receive_file(struct conf_struct *conf_struct, int sock, FILE *f) {
    long file_size=0;
    int total=0;
    char *file_buffer[MAXBUFSIZE];
    unsigned char ch;
    unsigned char key[MAXBUFSIZE];
    int i = 0;
    int nbytes = 0;
    while(conf_struct->username[i] != '\0') {
        key[i] = (unsigned char)conf_struct->username[i];
        i++;
    }
    int j = 0;
    while(conf_struct->password[j] != '\0') {
        key[i] = (unsigned char)conf_struct->password[j];
        j++;
        i++;
    }
    key[i] = '\0';
    unsigned long key_index = 0;
    recv(sock, &file_size, sizeof(file_size), 0); //Receive size of file of part x
    //printf("file_size: %d\n", file_size);
    while(total < file_size) {
        nbytes = recv(sock, &ch, sizeof(ch), 0);
        if(key_index < strlen(conf_struct->username) + strlen(conf_struct->password)) {
            ch = ch ^ key[key_index++];
        }
        fputc(ch, f);
        total += nbytes;
        memset(&file_buffer, 0, MAXBUFSIZE);
    }
    fclose(f);
}
int get_hash(char *num) {
    int res = 0;
    unsigned long i;
    for(i = 0; i < strlen(num); i++) {
        switch(num[i]) {
            case 'a':
                num[i] = 10;
                break;
            case 'b':
                num[i] = 11;
                break;
            case 'c':
                num[i] = 12;
                break;
            case 'd':
                num[i] = 13;
                break;
            case 'e':
                num[i] = 14;
                break;
            case 'f':
                num[i] = 15;
                break;
            default:
                num[i] = num[i] - '0';
        }
        res = (res*16 + num[i]) % 4; // xy (mod a) ≡ ((x (mod a) * y) (mod a))
    }
    return res;
}

int hash_file(char *file_name) {
    char command[MAXBUFSIZE];
    snprintf(command, MAXBUFSIZE, "md5 %s", file_name);
    FILE *f = popen(command, "r");
    char buf[MAXBUFSIZE];
    while (fgets(buf, sizeof(buf), f) != 0) {
        //printf("%s\n", buf);
    }
    pclose(f);
    buf[strlen(buf) - 1] = '\0';
    int i = 0;
    while(buf[i] != '=') {
        i++;
    }
    
    int hash = get_hash(&buf[i+2]);
    return hash;
}
void put(int sock[], char* path, char* file_name, struct conf_struct *conf_struct) {
    FILE *f, *part[4];
    long file_size, part_size, remainder;
    int hash;
    char message[MAXBUFSIZE];
    int j =0;
    
    //Create files to be written to
    part[0] = tmpfile();
    part[1] = tmpfile();
    part[2] = tmpfile();
    part[3] = tmpfile();
    
    f = fopen(file_name, "r+b");
    //Read the size of the file and then return the pointer to beginning of file
    fseek(f, 0, SEEK_END);
    file_size = ftell(f);
    rewind(f);
    
    part_size = file_size / 4;
    remainder = file_size % 4;
    
    //printf("remainder: %ld\n", remainder);
    
    //Write the parts to the files
    unsigned char ch;
    int written =0;
    while(!feof(f)) {
        ch = fgetc(f);
        if(written < part_size) {
            fputc(ch, part[0]);
        }
        else if(written < (part_size * 2)) {
            fputc(ch, part[1]);
        }
        else if(written < (part_size * 3)) {
            fputc(ch, part[2]);
        }
        else {
            fputc(ch, part[3]);
        }
        written++;
    }
    hash = hash_file(file_name);
    //printf("chars written: %d\n", written);
    int offlineServerCount =0;
    for(int i=0; i<4; i++) {
        if(sock[i] == -1) {
            offlineServerCount++;
        }
    }
    if(offlineServerCount>=1) {
        printf("Too many servers offline to send file.\n");
        return;
    }
    //Figure which parts to send to which server
    //printf("hash: %d\n", hash);
    switch(hash) {
            //(1,2), (2,3), (3,4), (4,1)
        case 0:
            for(int i=0; i<4; i++) {
                j = i+1;
                //Correct j
                if(j>4) {j = j-4;}
                snprintf(message, MAXBUFSIZE, "%s %s %d", "PUT", path, j);
                int n = send(sock[i], message, MAXBUFSIZE, 0); //Send command to server
                
                if(n < 0) {
                    printf("Server shut down, trying next one.\n");
                    continue;
                }
                if(j == 4) {
                    part_size += remainder;
                    send(sock[i], &part_size, sizeof(part_size), 0); //Send size
                    part_size -= remainder;
                }
                else {
                    send(sock[i], &part_size, sizeof(part_size), 0); //Send size of file
                }                ////printf("%d,", j);

                //Send first file
                send_file(sock[i], j, part_size, part, conf_struct, remainder);
                
                memset(&message, 0, MAXBUFSIZE);
                
                j = i+2;
                //Correct j
                if(j>4) {j = j-4;}
                snprintf(message, MAXBUFSIZE, "%s %s %d", "PUT", path, j);
                send(sock[i], message, MAXBUFSIZE, 0); //Send command to server
                if(j == 4) {
                    part_size += remainder;
                    send(sock[i], &part_size, sizeof(part_size), 0); //Send size
                    part_size -= remainder;
                }
                else {
                    send(sock[i], &part_size, sizeof(part_size), 0); //Send size of file
                }                //printf("%d\n", j);

                //Send the second file
                send_file(sock[i], j, part_size, part, conf_struct, remainder);
                
                memset(&message, 0, MAXBUFSIZE);
            }
            break;
            //(4,1), (1,2), (2,3), (3,4)
        case 1:
            for(int i=0; i<4; i++) {
                j = i+4;
                //Correct j
                if(j>4) {j = j-4;}
                snprintf(message, MAXBUFSIZE, "%s %s %d", "PUT", path, j);
                int n = send(sock[i], message, MAXBUFSIZE, 0); //Send command to server
                if(n < 0) {
                    printf("Server offline down, trying next one. n: %d\n", n);
                    continue;
                }
                if(j == 4) {
                    part_size += remainder;
                    send(sock[i], &part_size, sizeof(part_size), 0); //Send size
                    part_size -= remainder;
                }
                else {
                    send(sock[i], &part_size, sizeof(part_size), 0); //Send size of file
                }                //printf("%d,", j);

                //Send first file
                send_file(sock[i], j, part_size, part, conf_struct, remainder);
                
                memset(&message, 0, MAXBUFSIZE);
                
                j = i+1;
                //Correct j
                if(j>4) {j = j-4;}
                snprintf(message, MAXBUFSIZE, "%s %s %d", "PUT", path, j);
                send(sock[i], message, MAXBUFSIZE, 0); //Send command to server
                if(j == 4) {
                    part_size += remainder;
                    send(sock[i], &part_size, sizeof(part_size), 0); //Send size
                    part_size -= remainder;
                }
                else {
                    send(sock[i], &part_size, sizeof(part_size), 0); //Send size of file
                }                //printf("%d\n", j);

                //Send the second file
                send_file(sock[i], j, part_size, part, conf_struct, remainder);
                
                memset(&message, 0, MAXBUFSIZE);
            }
            break;
            //(3,4), (4,1), (1,2), (2,3)
        case 2:
            for(int i=0; i<4; i++) {
                j = i+3;
                //Correct j
                if(j>4) {j = j-4;}
                snprintf(message, MAXBUFSIZE, "%s %s %d", "PUT", path, j);
                int n= send(sock[i], message, MAXBUFSIZE, 0); //Send command to server
                if(n < 0) {
                    printf("Server shut down, trying next one.\n");
                    continue;
                }
                if(j == 4) {
                    part_size += remainder;
                    send(sock[i], &part_size, sizeof(part_size), 0); //Send size
                    part_size -= remainder;
                }
                else {
                    send(sock[i], &part_size, sizeof(part_size), 0); //Send size of file
                }
                //printf("%d,", j);

                //Send first file
                send_file(sock[i], j, part_size, part, conf_struct, remainder);
                
                memset(&message, 0, MAXBUFSIZE);
                
                j = i+4;
                //Correct j
                if(j>4) {j = j-4;}
                snprintf(message, MAXBUFSIZE, "%s %s %d", "PUT", path, j);
                send(sock[i], message, MAXBUFSIZE, 0); //Send command to server
                if(j == 4) {
                    part_size += remainder;
                    send(sock[i], &part_size, sizeof(part_size), 0); //Send size
                    part_size -= remainder;
                }
                else {
                    send(sock[i], &part_size, sizeof(part_size), 0); //Send size of file
                }
                //printf("%d\n", j);

                //Send the second file
                send_file(sock[i], j, part_size, part, conf_struct, remainder);
                
                memset(&message, 0, MAXBUFSIZE);
            }
            break;
            //(2,3), (3,4), (4,1), (1,2)
        case 3:
            for(int i=0; i<4; i++) {
                j = i+2;
                //Correct j
                if(j>4) {j = j-4;}
                snprintf(message, MAXBUFSIZE, "%s %s %d", "PUT", path, j);
                int n =send(sock[i], message, MAXBUFSIZE, 0); //Send command to server
                if(n < 0) {
                    printf("Server shut down, trying next one.\n");
                    continue;
                }
                if(j == 4) {
                    part_size += remainder;
                    send(sock[i], &part_size, sizeof(part_size), 0); //Send size
                    part_size -= remainder;
                }
                else {
                    send(sock[i], &part_size, sizeof(part_size), 0); //Send size of file
                }
                //printf("%d,", j);
                //Send first file

                send_file(sock[i], j, part_size, part, conf_struct, remainder);

                
                memset(&message, 0, MAXBUFSIZE);
                
                j = i+3;
                //Correct j
                if(j>4) {j = j-4;}
                snprintf(message, MAXBUFSIZE, "%s %s %d", "PUT", path, j);
                send(sock[i], message, MAXBUFSIZE, 0); //Send command to server
                if(j == 4) {
                    part_size += remainder;
                    send(sock[i], &part_size, sizeof(part_size), 0); //Send size
                    part_size -= remainder;
                }
                else {
                    send(sock[i], &part_size, sizeof(part_size), 0); //Send size of file
                }
                //printf("%d\n", j);
                //Send the second file

                send_file(sock[i], j, part_size, part, conf_struct, remainder);

                memset(&message, 0, MAXBUFSIZE);
            }
            break;
    }
    fclose(part[0]);
    fclose(part[1]);
    fclose(part[2]);
    fclose(part[3]);
    fclose(f);
}

void get(int sock[], char* path, char* file_name, struct conf_struct *conf_struct, struct sockaddr_in remote) {
    printf("here-----a \n");
    char message[MAXBUFSIZE];
    char buffer[MAXBUFSIZE];

    snprintf(message, MAXBUFSIZE, "%s %s %d", "GET", path, 0);
    
    int fileparts[4] = {0,0,0,0};
    printf("fileparts: %d\n", fileparts[0]);
    printf("fileparts: %d\n", fileparts[1]);
    printf("fileparts: %d\n", fileparts[2]);
    printf("fileparts: %d\n", fileparts[3]);
    
    
    int offline_servers =0;
    int complete_file = 1;
    FILE *f;
    
    for(int i=0; i<4; i++) {
        if(sock[i] == -1) {
            offline_servers++;
        }
    }
    printf("offline: %d\n", offline_servers);
    if(offline_servers>=3) {
        complete_file = 0;
    }
    if(complete_file) {
        int hash;
        /*
         x DFS1  DFS2   DFS3 DFS4
         0 (1,2) (2,3) (3,4) (4,1)
         1 (4,1) (1,2) (2,3) (3,4)
         2 (3,4) (4,1) (1,2) (2,3)
         3 (2,3) (3,4) (4,1) (1,2)
         */
        int inc[4] = {0,0,0,0};
        for(int i = 0; i < 4; i++) {
            /*if (connect(sock[i], (struct sockaddr *)&remote, sizeof(remote)) < 0) {
                close(sock[i]);
                sock[i] = -1;
                printf("Couldnt connect\n");
            }*/
            if(sock[i] != -1) {
                int s = send(sock[i], message, MAXBUFSIZE, 0);
                printf("ser stat s %d\n", s);
                int r = recv(sock[i], &fileparts[i], sizeof(fileparts[i]), 0);
                printf("ser stat r %d\n", r);
                if (r == 0 || r == -1) {
                    sock[i] = -1;
                    inc[i] = 1;
                }
            } else {
                inc[i] = 1;
            }
            //printf("fileparts: %d\n", fileparts[i]);
        }
        int missing = 0;
        //printf("fileparts[1]: %d\n", fileparts[1]);
        //printf("%d\n", fileparts[1] == 0);
        if(fileparts[0] == 12 || fileparts[1] == 23 || fileparts[2] == 34 || fileparts[3] == 14) {
            hash = 0;
        } else if(fileparts[1] == 12 || fileparts[2] == 23 || fileparts[3] == 34 || fileparts[0] == 14) {
            hash = 1;
        } else if(fileparts[2] == 12 || fileparts[3] == 23 || fileparts[0] == 34 || fileparts[1] == 14) {
            hash = 2;
        } else {
            hash = 3;
        }
        
        printf("hash client: %d\n", hash);
        if (missing == 0) {
            int parta = 0;
            int partb = 0;
            switch(hash) {
                case 0:
                    // (1,2) (2,3) (3,4) (4,1)
                    
                    // get part 1 and 2 from dfs1
                    // get part 3 and 4 from dfs3
                    
                    if(inc[0] != 1 && inc[2] != 1) {
                        //Parts 1 and 2
                        parta = 1;
                        printf("here ---1 \n");
                        f = fopen(file_name, "w+b");
                        receive_file(conf_struct, sock[0], f);
                        f = fopen(file_name, "ab");
                        receive_file(conf_struct, sock[0], f);
                        //Parts 3 and 4
                        printf("here ---2 \n");
                        f = fopen(file_name, "ab");
                        receive_file(conf_struct, sock[2], f);
                        f = fopen(file_name, "ab");
                        receive_file(conf_struct, sock[2], f);
                    }else if(inc[1] != 1 && inc[3] != 1) {
                        partb = 1;
                        //Parts 1 and 2
                        printf("here ---3 \n");
                        f = fopen(file_name, "w+b");
                        receive_file(conf_struct, sock[3], f);
                        f = fopen(file_name, "ab");
                        receive_file(conf_struct, sock[1], f);
                        //Parts 3 and 4
                        printf("here ---4 \n");
                        f = fopen(file_name, "ab");
                        receive_file(conf_struct, sock[1], f);
                        f = fopen(file_name, "ab");
                        receive_file(conf_struct, sock[3], f);
                    }
                    if (parta == 0 && partb == 0){
                        printf("File is incomplete, cannot be downloaded from server.\n"); 
                    }
                    break;
                case 1:
                    // (4,1) (1,2) (2,3) (3,4)
                    
                    // get part 1 and 2 from dfs1
                    // get part 3 and 4 from dfs3
                    if(inc[0] != 1 && inc[2] != 1) {
                        parta = 1;
                        //Parts 1 and 2
                        printf("here ---5 \n");
                        f = fopen(file_name, "w+b");
                        receive_file(conf_struct, sock[0], f);
                        f = fopen(file_name, "ab");
                        receive_file(conf_struct, sock[0], f);
                        //Parts 3 and 4
                        printf("here ---6 \n");
                        f = fopen(file_name, "ab");
                        receive_file(conf_struct, sock[2], f);
                        f = fopen(file_name, "ab");
                        receive_file(conf_struct, sock[2], f);
                    }
                    if(inc[1] != 1 && inc[3] != 1) {
                        partb = 1;
                        //Parts 1 and 2
                        printf("here ---7 \n");
                        f = fopen(file_name, "w+b");
                        receive_file(conf_struct, sock[3], f);
                        f = fopen(file_name, "ab");
                        receive_file(conf_struct, sock[1], f);
                        //Parts 3 and 4
                        printf("here ---8 \n");
                        f = fopen(file_name, "ab");
                        receive_file(conf_struct, sock[1], f);
                        f = fopen(file_name, "ab");
                        receive_file(conf_struct, sock[3], f);
                    }
                    if (parta == 0 && partb == 0){
                        printf("File is incomplete, cannot be downloaded from server.\n"); 
                    }
                    break;
                
                case 2:
                    // (3,4) (4,1) (1,2) (2,3)

                    if(inc[0] != 1 && inc[2] != 1) {
                        parta = 1;
                        //Parts 1 and 2
                        printf("here ---9 \n");
                        f = fopen(file_name, "w+b");
                        receive_file(conf_struct, sock[2], f);
                        f = fopen(file_name, "ab");
                        receive_file(conf_struct, sock[2], f);
                        //Parts 3 and 4
                        printf("here ---10 \n");
                        f = fopen(file_name, "ab");
                        receive_file(conf_struct, sock[0], f);
                        f = fopen(file_name, "ab");
                        receive_file(conf_struct, sock[0], f);
                    }
                    if(inc[1] != 1 && inc[3] != 1) {
                        partb = 1;
                        //Parts 1 and 2
                        printf("here ---11 \n");
                        f = fopen(file_name, "w+b");
                        receive_file(conf_struct, sock[1], f);
                        f = fopen(file_name, "ab");
                        receive_file(conf_struct, sock[3], f);
                        //Parts 3 and 4
                        printf("here ---12 \n");
                        f = fopen(file_name, "ab");
                        receive_file(conf_struct, sock[3], f);
                        f = fopen(file_name, "ab");
                        receive_file(conf_struct, sock[1], f);
                    }
                    if (parta == 0 && partb == 0){
                        printf("File is incomplete, cannot be downloaded from server.\n"); 
                    }
                    break;
                
                case 3:
                    // (2,3) (3,4) (4,1) (1,2)
                    // get part 1 and 2 from dfs1
                    if(inc[0] != 1 && inc[2] != 1) {
                        parta = 1;
                        printf("here ---13 \n");
                        //Parts 1 and 2
                        f = fopen(file_name, "w+b");
                        receive_file(conf_struct, sock[2], f);
                        f = fopen(file_name, "ab");
                        receive_file(conf_struct, sock[0], f);
                        //Parts 3 and 4
                        printf("here ---14 \n");
                        f = fopen(file_name, "ab");
                        receive_file(conf_struct, sock[0], f);
                        f = fopen(file_name, "ab");
                        receive_file(conf_struct, sock[2], f);
                        f = fopen(file_name, "rb");
                        fgets(buffer, MAXBUFSIZE, f);
                        printf("Buffer: %s\n", buffer);
                    }
                    if(inc[1] != 1 && inc[3] != 1) {
                        partb = 1;
                        printf("here ---15 \n");
                        //Parts 1 and 2
                        f = fopen(file_name, "w+b");
                        receive_file(conf_struct, sock[3], f);
                        f = fopen(file_name, "ab");
                        receive_file(conf_struct, sock[3], f);
                        //Parts 3 and 4
                        printf("here ---16 \n");
                        f = fopen(file_name, "ab");
                        receive_file(conf_struct, sock[1], f);
                        f = fopen(file_name, "ab");
                        receive_file(conf_struct, sock[1], f);
                        f = fopen(file_name, "rb");
                        fgets(buffer, MAXBUFSIZE, f);
                        printf("Buffer: %s\n", buffer);
                    }
                    if (parta == 0 && partb == 0){
                        printf("File is incomplete, cannot be downloaded from server.\n"); 
                    }
                    break;
            }   
        } else {
           printf("File is incomplete, cannot be downloaded from server.\n"); 
        }
        
    } else {
        printf("File is incomplete, cannot be downloaded from server.\n");
    }
}
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
struct sockaddr_in remote;
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
                
                
                
                if(strcmp(command, "list\n") == 0) {
                    list(sockets, &file_list);
                } else if(strcmp(command, "put") == 0) {
                    put(sockets, path, file, &conf_struct);
                } else if(strcmp(command, "get") == 0) {
                    get(sockets, path, file, &conf_struct, remote);
                } else if(strcmp(command, "exit\n") == 0) {
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

