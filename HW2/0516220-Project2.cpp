#include <iostream>
#include <list>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <json-c/json.h>

using namespace std;

typedef struct{
    char id[100];
    char token[100];
} id_token;

list<id_token> login_users;

int main(int argc, char *argv[]){
    while(1){
        char *arg[100];
        char *input_cmd = NULL;
        char *user = NULL;
        char input_buf[100] = {'\0'};
        char recv_buf[300] = {'\0'};
        read(STDIN_FILENO, input_buf, 100);
        strtok(input_buf, "\n");

        /* Delete Saved Tokens And Exit The Program. */
        if(!strcmp(input_buf, "exit")){
            if(login_users.size() != 0) login_users.clear();    // Delete saved tokens
            break;
        }

        /* Fetch Command And User From Input */
        char tmp[500] = {'\0'};

        int arg_cnt = 0;
        arg[0] = strtok(input_buf, " ");
        while(arg[arg_cnt] != NULL){
            arg_cnt++;
            arg[arg_cnt] = strtok(NULL, " ");
        }
        arg_cnt--;

        input_cmd = arg[0];
        if(arg_cnt > 0)user = arg[1];

        if(strcmp(input_cmd, "register") != 0 && strcmp(input_cmd, "login") != 0 && arg_cnt > 0){   // If the command is NOT "register" and "login" means
            int find_user = 0;                                                                      // second argument must be <user> and must be replaced by token
            for(list<id_token>::iterator it = login_users.begin(); it != login_users.end(); it++){
                if(!strcmp(user, (*it).id)){                // Find tokens that we saved
                    user = (*it).token;                     // If there's one replace <user> to token
                    find_user = 1;                          // Flag that we find <user> is in the token list (also means that he is already login)
                    break;
                }
            }
            if(!find_user) user = (char*)"\0";              // If the token list doesn't have this <user> means he isn't login, leave <user> empty
        }

        if(arg_cnt == 0){
            strcat(tmp, input_cmd);
        }else if(arg_cnt == 1){
            strcat(tmp, input_cmd);
            strcat(tmp, " ");
            strcat(tmp, user);
        }else if(arg_cnt > 1){
            strcat(tmp, input_cmd);
            strcat(tmp, " ");
            strcat(tmp, user);
            for(int i = 2; i <= arg_cnt; i++){
                strcat(tmp, " ");
                strcat(tmp, arg[i]);
            }
        }
        char send_msg[strlen(tmp)] = {'\0'};                    // Declare send_msg (It's the string that we'll send to server)
        strcpy(send_msg, tmp);                                  // Copy tmp to send_msg

        /* Create Socket */
        int sock_fd = 0;
        sock_fd = socket(PF_INET, SOCK_STREAM, 0);
        if(sock_fd == -1) printf("Fail to create socket.\n");

        /* Info of Socket (Server) */
        struct sockaddr_in sock2sever;
        if(argc == 1){                                          // Default server setting
            memset(&sock2sever, 0, sizeof(sock2sever));
            sock2sever.sin_family = AF_INET;
            sock2sever.sin_port = htons(8008);
            inet_pton(AF_INET, "140.113.207.51", &sock2sever.sin_addr.s_addr);
        }else if(argc == 3){                                    // Optional server setting
            int port;
            sscanf(argv[2], "%d", &port);
            memset(&sock2sever, 0, sizeof(sock2sever));
            sock2sever.sin_family = AF_INET;
            sock2sever.sin_port = htons(port);
            inet_pton(AF_INET, argv[1], &sock2sever.sin_addr.s_addr);
        }

        /* Connection */
        int conn_rv = connect(sock_fd, (struct sockaddr *)&sock2sever, sizeof(sock2sever));
        if(conn_rv == -1) printf("Fail to connect to the server.\n");

        /* Send Command to Server */
        send(sock_fd, send_msg, sizeof(send_msg), 0);
        /* Receive Command from Server */
        recv(sock_fd, recv_buf, sizeof(recv_buf), 0);


        /* Unpack JSON And Print Out */
        struct json_object *root, *status, *token, *message, *invite, *my_friend, *post;
        root = json_tokener_parse(recv_buf);
        if(root == NULL) printf("Parse failed.\n");
        status = json_object_object_get(root, "status");
        token = json_object_object_get(root, "token");
        invite = json_object_object_get(root, "invite");
        my_friend = json_object_object_get(root, "friend");
        post = json_object_object_get(root, "post");
        message = json_object_object_get(root, "message");

        if(invite != NULL && !strcmp(input_cmd, "list-invite") && json_object_get_int(status) == 0){
            int arraylen = json_object_array_length(invite);
            for(int i = 0; i < arraylen; i++){
                printf("%s\n", json_object_get_string(json_object_array_get_idx(invite, i)));
            }
            if(arraylen == 0) printf("No invitation\n");
        }else if(my_friend != NULL && !strcmp(input_cmd, "list-friend") && json_object_get_int(status) == 0){
            int arraylen = json_object_array_length(my_friend);
            for(int i = 0; i < arraylen; i++){
                printf("%s\n", json_object_get_string(json_object_array_get_idx(my_friend, i)));
            }
            if(arraylen == 0) printf("No friends\n");
        }else if(post != NULL && !strcmp(input_cmd, "receive-post") && json_object_get_int(status) == 0){
            int arraylen = json_object_array_length(post);
            for(int i = 0; i < arraylen; i++){
                struct json_object *id, *post_msg;
                id = json_object_object_get(json_object_array_get_idx(post, i), "id");
                post_msg = json_object_object_get(json_object_array_get_idx(post, i), "message");
                printf("%s: %s\n", json_object_get_string(id), json_object_get_string(post_msg));
            }
            if(arraylen == 0) printf("No posts\n");
        }else{
            if(token != NULL && !strcmp(input_cmd, "login") && json_object_get_int(status) == 0){
                id_token new_item;
                strcpy(new_item.id, user);
                strcpy(new_item.token, json_object_get_string(token));
                login_users.push_back(new_item);
            }
            if((!strcmp(input_cmd, "logout")) || (!strcmp(input_cmd, "delete")) && json_object_get_int(status) == 0){
                for(list<id_token>::iterator it = login_users.begin(); it != login_users.end(); it++){
                    if(!strcmp(user, (*it).token)){
                        login_users.erase(it);
                        break;
                    }
                }
            }
            printf("%s\n", json_object_get_string(message));
        }
    }

    return 0;
}