#include <ctime>
#include <string>
#include <stdio.h>
#include <iostream>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sqlite3.h>
#include <openssl/sha.h>

using namespace std;

char *arg[100];

int Fetch_Command(char recv_buf[]){
    int arg_cnt = 0;
    arg[0] = strtok(recv_buf, " ");
    while(arg[arg_cnt] != NULL){
        arg_cnt++;
        arg[arg_cnt] = strtok(NULL, " ");
        if(!strcmp(arg[0], "post") && arg[arg_cnt] != NULL && arg_cnt == 1){
            arg_cnt++;
            arg[arg_cnt] = strtok(NULL, "\0");
            if(arg[arg_cnt] != NULL)
                arg_cnt++;
        }
    }
    arg_cnt--;
    return arg_cnt;
}

bool Validate_Token(sqlite3 *db, string user){
    bool valid_token;
    sqlite3_stmt *stmt = NULL;
    string sql;
    sql = "SELECT * FROM ValidTokens WHERE token=?;";
    if(sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, NULL) == SQLITE_OK){
        sqlite3_bind_text(stmt, 1, user.c_str(), -1, NULL);
        if(sqlite3_step(stmt) == SQLITE_DONE)
            valid_token = false;
        else
            valid_token = true;
    }
    sqlite3_finalize(stmt);
    return valid_token;
}

string Command_Case(int arg_cnt){
    sqlite3 *db;
    int rc;

    /* Open DB */
    rc = sqlite3_open("test.db", &db);
    if(rc){
        fprintf(stderr, "Can't open database: %s\n", sqlite3_errmsg(db));
    }else{
        fprintf(stderr, "Opened database successfully\n");
    }
    /* FKEY Mode On */
    sqlite3_db_config(db, SQLITE_DBCONFIG_ENABLE_FKEY, 1, NULL);

    string send_buf, input_cmd, user;
    input_cmd = arg[0];
    if(arg_cnt > 0) user = arg[1];

    if(input_cmd == "register"){
        if(arg_cnt != 2){
            send_buf = "{\n\"status\":1,\n\"message\":\"Usage: register <id> <password>\"\n}";
        }else{
            sqlite3_stmt *stmt = NULL;
            string sql;
            sql = "INSERT INTO Users (user,password) VALUES (?,?);";
            if(sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, NULL) == SQLITE_OK){
                sqlite3_bind_text(stmt, 1, user.c_str(), -1, NULL);
                sqlite3_bind_text(stmt, 2, arg[2], -1, NULL);
                sqlite3_step(stmt);
                int result_code = sqlite3_extended_errcode(db);
                if(result_code == SQLITE_DONE){
                    send_buf = "{\n\"status\":0,\n\"message\":\"Success!\"\n}";
                }else if(result_code == SQLITE_CONSTRAINT_UNIQUE){
                    send_buf = "{\n\"status\":1,\n\"message\":\"" + user + " is already used\"\n}";
                }
            }
            sqlite3_finalize(stmt);
        }
    }else if(input_cmd == "login"){
        if(arg_cnt != 2){
            send_buf = "{\n\"status\":1,\n\"message\":\"Usage: login <id> <password>\"\n}";
        }else{
            sqlite3_stmt *stmt = NULL;
            string sql;
            sql = "SELECT * FROM Users WHERE user=? AND password=?;";
            if(sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, NULL) == SQLITE_OK){
                sqlite3_bind_text(stmt, 1, user.c_str(), -1, NULL);
                sqlite3_bind_text(stmt, 2, arg[2], -1, NULL);
                if(sqlite3_step(stmt) == SQLITE_DONE){
                    send_buf = "{\n\"status\":1,\n\"message\":\"No such user or password error\"\n}";
                }else{
                    int userID = sqlite3_column_int(stmt, 0);
                    /* Generate Token */
                    time_t now = time(0);
                    string now_time = ctime(&now);
                    string tok_src = user + "@" + now_time;
                    tok_src.pop_back();
                    unsigned char token_tmp[SHA256_DIGEST_LENGTH];
                    SHA256((const unsigned char*)tok_src.c_str(), tok_src.size(), token_tmp);
                    char tmp[3];
                    string token;
                    for(int i = 0; i < SHA256_DIGEST_LENGTH; i++){
                        sprintf(tmp, "%02X", token_tmp[i]);
                        token += tmp;
                    }
                    /* Store Token Or Update Token */
                    sqlite3_stmt *stmt2 = NULL;
                    string sql2;
                    sql2 = "INSERT INTO ValidTokens (userID,token) VALUES (?,?);";
                    if(sqlite3_prepare_v2(db, sql2.c_str(), -1, &stmt2, NULL) == SQLITE_OK){
                        sqlite3_bind_int(stmt2, 1, userID);
                        sqlite3_bind_text(stmt2, 2, token.c_str(), -1, NULL);
                        sqlite3_step(stmt2);
                        int result_code = sqlite3_extended_errcode(db);
                        if(result_code == SQLITE_CONSTRAINT_UNIQUE){
                            string sql3;
                            sqlite3_stmt *stmt3 = NULL;
                            sql3 = "SELECT token FROM ValidTokens WHERE userID=?;";
                            if(sqlite3_prepare_v2(db, sql3.c_str(), -1, &stmt3, NULL) == SQLITE_OK){
                                sqlite3_bind_int(stmt3, 1, userID);
                                sqlite3_step(stmt3);
                                char exist_token[64];
                                strcpy(exist_token, (const char *)sqlite3_column_text(stmt3, 0));
                                token = exist_token;
                            }
                            sqlite3_finalize(stmt3);
                        }
                        send_buf = "{\n\"status\":0,\n\"token\":\"" + token + "\",\n\"message\":\"Success!\"\n}";
                    }
                    sqlite3_finalize(stmt2);
                }
            }
            sqlite3_finalize(stmt);
        }
    }else if(input_cmd == "delete"){
        if(arg_cnt == 0){
            send_buf = "{\n\"status\":1,\n\"message\":\"Not login yet\"\n}";
        }else if(arg_cnt >= 1){
            if(Validate_Token(db, user) == false)
                send_buf = "{\n\"status\":1,\n\"message\":\"Not login yet\"\n}";
            else{
                sqlite3_stmt *stmt = NULL;
                string sql;
                sql = "DELETE FROM Users WHERE id=(SELECT userID FROM ValidTokens WHERE token=?);";
                if(sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, NULL) == SQLITE_OK){
                    sqlite3_bind_text(stmt, 1, user.c_str(), -1, NULL);
                    if(sqlite3_step(stmt) == SQLITE_DONE)
                        send_buf = "{\n\"status\":0,\n\"message\":\"Success!\"\n}";
                }
                sqlite3_finalize(stmt);
            }
        }
    }else if(input_cmd == "logout"){
        if(arg_cnt == 0){
            send_buf = "{\n\"status\":1,\n\"message\":\"Not login yet\"\n}";
        }else if(arg_cnt >= 1){
            if(Validate_Token(db, user) == false)
                send_buf = "{\n\"status\":1,\n\"message\":\"Not login yet\"\n}";
            else{
                if(arg_cnt > 1)
                    send_buf = "{\n\"status\":1,\n\"message\":\"Usage: logout <user>\"\n}";
                else{
                    sqlite3_stmt *stmt = NULL;
                    string sql;
                    sql = "DELETE FROM ValidTokens WHERE token=?;";
                    if(sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, NULL) == SQLITE_OK){
                        sqlite3_bind_text(stmt, 1, user.c_str(), -1, NULL);
                        if(sqlite3_step(stmt) == SQLITE_DONE)
                            send_buf = "{\n\"status\":0,\n\"message\":\"Bye!\"\n}";
                    }
                    sqlite3_finalize(stmt);
                }
            }
        }
    }else if(input_cmd == "invite"){
        if(arg_cnt == 0){
            send_buf = "{\n\"status\":1,\n\"message\":\"Not login yet\"\n}";
        }else if(arg_cnt >= 1){
            if(Validate_Token(db, user) == false)
                send_buf = "{\n\"status\":1,\n\"message\":\"Not login yet\"\n}";
            else{
                if(arg_cnt != 2)
                    send_buf = "{\n\"status\":1,\n\"message\":\"Usage: invite <user> <id>\"\n}";
                else{
                    string invitee = arg[2];
                    sqlite3_stmt *stmt = NULL;
                    string sql;
                    /* Find Inviter Id */
                    sql = "SELECT id FROM Users WHERE id=(SELECT userID FROM ValidTokens WHERE token=?);";
                    if(sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, NULL) == SQLITE_OK){
                        sqlite3_bind_text(stmt, 1, user.c_str(), -1, NULL);
                        sqlite3_step(stmt);
                        int inviter_id = sqlite3_column_int(stmt, 0);

                        /* Find If Invitee Is Exist */
                        sqlite3_stmt *stmt2 = NULL;
                        string sql2;
                        sql2 = "SELECT id FROM Users WHERE user=?;";
                        if(sqlite3_prepare_v2(db, sql2.c_str(), -1, &stmt2, NULL) == SQLITE_OK){
                            sqlite3_bind_text(stmt2, 1, arg[2], -1, NULL);
                            if(sqlite3_step(stmt2) == SQLITE_DONE){
                                /* Invitee Is Not Exist */
                                send_buf = "{\n\"status\":1,\n\"message\":\"" + invitee + " does not exist\"\n}";
                            }else{
                                /* Invitee Is Exist */
                                int invitee_id = sqlite3_column_int(stmt2, 0);
                                if(inviter_id == invitee_id)
                                    send_buf = "{\n\"status\":1,\n\"message\":\"You cannot invite yourself\"\n}";
                                else{
                                    sqlite3_stmt *stmt3 = NULL;
                                    string sql3;
                                    sql3 = "SELECT inviterID, inviteeID FROM Invites "
                                            "WHERE (inviterID=? AND inviteeID=?) OR (inviterID=? AND inviteeID=?);";
                                    if(sqlite3_prepare_v2(db, sql3.c_str(), -1, &stmt3, NULL) == SQLITE_OK){
                                        sqlite3_bind_int(stmt3, 1, inviter_id);
                                        sqlite3_bind_int(stmt3, 2, invitee_id);
                                        sqlite3_bind_int(stmt3, 3, invitee_id);
                                        sqlite3_bind_int(stmt3, 4, inviter_id);
                                        if(sqlite3_step(stmt3) == SQLITE_DONE){
                                            sqlite3_stmt *stmt4 = NULL;
                                            string sql4;
                                            sql4 = "SELECT * FROM Friends WHERE friendAID=? AND friendBID=?;";
                                            if(sqlite3_prepare_v2(db, sql4.c_str(), -1, &stmt4, NULL) == SQLITE_OK){
                                                sqlite3_bind_int(stmt4, 1, inviter_id);
                                                sqlite3_bind_int(stmt4, 2, invitee_id);
                                                if(sqlite3_step(stmt4) == SQLITE_DONE){
                                                    sqlite3_stmt *stmt5 = NULL;
                                                    string sql5;
                                                    sql5 = "INSERT INTO Invites (inviterID,inviteeID) VALUES (?,?);";
                                                    if(sqlite3_prepare_v2(db, sql5.c_str(), -1, &stmt5, NULL) == SQLITE_OK){
                                                        sqlite3_bind_int(stmt5, 1, inviter_id);
                                                        sqlite3_bind_int(stmt5, 2, invitee_id);
                                                        if(sqlite3_step(stmt5) == SQLITE_DONE)
                                                            send_buf = "{\n\"status\":0,\n\"message\":\"Success!\"\n}";
                                                    }
                                                    sqlite3_finalize(stmt5);
                                                }else{
                                                    send_buf = "{\n\"status\":1,\n\"message\":\"" + invitee + " is already your friend\"\n}";
                                                }
                                            }
                                            sqlite3_finalize(stmt4);
                                        }else{
                                            int inviter_in_table = sqlite3_column_int(stmt3, 0);
                                            int invitee_in_table = sqlite3_column_int(stmt3, 1);
                                            if(inviter_id == inviter_in_table)
                                                send_buf = "{\n\"status\":1,\n\"message\":\"Already invited\"\n}";
                                            else if(inviter_id == invitee_in_table)
                                                send_buf = "{\n\"status\":1,\n\"message\":\"" + invitee + " has invited you\"\n}";
                                        }
                                    }
                                    sqlite3_finalize(stmt3);
                                }
                            }
                        }
                        sqlite3_finalize(stmt2);
                    }
                    sqlite3_finalize(stmt);
                }
            }
        }
    }else if(input_cmd == "list-invite"){
        if(arg_cnt == 0){
            send_buf = "{\n\"status\":1,\n\"message\":\"Not login yet\"\n}";
        }else if(arg_cnt >= 1){
            if(Validate_Token(db, user) == false)
                send_buf = "{\n\"status\":1,\n\"message\":\"Not login yet\"\n}";
            else{
                if(arg_cnt > 1)
                    send_buf = "{\n\"status\":1,\n\"message\":\"Usage: list-invite <user>\"\n}";
                else{
                    sqlite3_stmt *stmt = NULL;
                    string sql;
                    sql = "SELECT user FROM Users INNER JOIN Invites ON Invites.inviterID=Users.id WHERE inviteeID="
                            "(SELECT id FROM Users WHERE id=(SELECT userID FROM ValidTokens WHERE token=?));";
                    if(sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, NULL) == SQLITE_OK){
                        sqlite3_bind_text(stmt, 1, user.c_str(), -1, NULL);
                        send_buf = "{\n\"status\":0,\n\"invite\":[";
                        while(sqlite3_step(stmt) == SQLITE_ROW){
                            send_buf = send_buf + "\n";
                            char inviter_tmp[256];
                            string inviter;
                            strcpy(inviter_tmp, (const char *)sqlite3_column_text(stmt, 0));
                            inviter = inviter_tmp;
                            send_buf = send_buf + "\"" + inviter + "\",";
                        }
                        if(send_buf.back() == ',')
                            send_buf.pop_back();
                        send_buf = send_buf + "\n]\n}";
                    }
                    sqlite3_finalize(stmt);
                }
            }
        }
    }else if(input_cmd == "accept-invite"){
        if(arg_cnt == 0){
            send_buf = "{\n\"status\":1,\n\"message\":\"Not login yet\"\n}";
        }else if(arg_cnt >= 1){
            if(Validate_Token(db, user) == false)
                send_buf = "{\n\"status\":1,\n\"message\":\"Not login yet\"\n}";
            else{
                if(arg_cnt != 2)
                    send_buf = "{\n\"status\":1,\n\"message\":\"Usage: accept-invite <user> <id>\"\n}";
                else{
                    string inviter = arg[2];
                    sqlite3_stmt *stmt = NULL;
                    string sql;
                    sql = "SELECT id FROM Users WHERE user=?;";
                    if(sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, NULL) == SQLITE_OK){
                        sqlite3_bind_text(stmt, 1, arg[2], -1, NULL);
                        if(sqlite3_step(stmt) == SQLITE_DONE)
                            send_buf = "{\n\"status\":1,\n\"message\":\"" + inviter + " did not invite you\"\n}";
                        else{
                            int inviter_id = sqlite3_column_int(stmt, 0);
                            sqlite3_stmt *stmt2 = NULL;
                            string sql2;
                            sql2 = "SELECT * FROM Invites WHERE inviterID=? "
                                    "AND inviteeID=(SELECT userID FROM ValidTokens WHERE token=?);";
                            if(sqlite3_prepare_v2(db, sql2.c_str(), -1, &stmt2, NULL) == SQLITE_OK){
                                sqlite3_bind_int(stmt2, 1, inviter_id);
                                sqlite3_bind_text(stmt2, 2, user.c_str(), -1, NULL);
                                if(sqlite3_step(stmt2) == SQLITE_DONE)
                                    send_buf = "{\n\"status\":1,\n\"message\":\"" + inviter + " did not invite you\"\n}";
                                else{
                                    sqlite3_stmt *stmt3 = NULL;
                                    string sql3;
                                    sql3 = "DELETE FROM Invites WHERE inviterID=? "
                                            "AND inviteeID=(SELECT userID FROM ValidTokens WHERE token=?);";
                                    if(sqlite3_prepare_v2(db, sql3.c_str(), -1, &stmt3, NULL) == SQLITE_OK){
                                        sqlite3_bind_int(stmt3, 1, inviter_id);
                                        sqlite3_bind_text(stmt3, 2, user.c_str(), -1, NULL);
                                        if(sqlite3_step(stmt3) == SQLITE_DONE){
                                            sqlite3_stmt *stmt4 = NULL;
                                            string sql4;
                                            sql4 = "INSERT INTO Friends (friendAID,friendBID) "
                                                    "VALUES (?,(SELECT userID FROM ValidTokens WHERE token=?)),"
                                                    "((SELECT userID FROM ValidTokens WHERE token=?),?);";
                                            if(sqlite3_prepare_v2(db, sql4.c_str(), -1, &stmt4, NULL) == SQLITE_OK){
                                                sqlite3_bind_int(stmt4, 1, inviter_id);
                                                sqlite3_bind_text(stmt4, 2, user.c_str(), -1, NULL);
                                                sqlite3_bind_text(stmt4, 3, user.c_str(), -1, NULL);
                                                sqlite3_bind_int(stmt4, 4, inviter_id);
                                                if(sqlite3_step(stmt4) == SQLITE_DONE)
                                                    send_buf = "{\n\"status\":0,\n\"message\":\"Success!\"\n}";
                                            }
                                            sqlite3_finalize(stmt4);
                                        }
                                    }
                                    sqlite3_finalize(stmt3);
                                }
                            }
                            sqlite3_finalize(stmt2);
                        }
                    }
                    sqlite3_finalize(stmt);
                }
            }
        }
    }else if(input_cmd == "list-friend"){
        if(arg_cnt == 0){
            send_buf = "{\n\"status\":1,\n\"message\":\"Not login yet\"\n}";
        }else if(arg_cnt >= 1){
            if(Validate_Token(db, user) == false)
                send_buf = "{\n\"status\":1,\n\"message\":\"Not login yet\"\n}";
            else{
                if(arg_cnt > 1)
                    send_buf = "{\n\"status\":1,\n\"message\":\"Usage: list-friend <user>\"\n}";
                else{
                    sqlite3_stmt *stmt = NULL;
                    string sql;
                    sql = "SELECT user FROM Users INNER JOIN Friends ON Friends.friendBID=Users.id WHERE friendAID="
                            "(SELECT id FROM Users WHERE id=(SELECT userID FROM ValidTokens WHERE token=?));";
                    if(sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, NULL) == SQLITE_OK){
                        sqlite3_bind_text(stmt, 1, user.c_str(), -1, NULL);
                        send_buf = "{\n\"status\":0,\n\"friend\":[";
                        while(sqlite3_step(stmt) == SQLITE_ROW){
                            send_buf = send_buf + "\n";
                            char friends_tmp[256];
                            string friends;
                            strcpy(friends_tmp, (const char *)sqlite3_column_text(stmt, 0));
                            friends = friends_tmp;
                            send_buf = send_buf + "\"" + friends + "\",";
                        }
                        if(send_buf.back() == ',')
                            send_buf.pop_back();
                        send_buf = send_buf + "\n]\n}";
                    }
                    sqlite3_finalize(stmt);
                }
            }
        }
    }else if(input_cmd == "post"){
        if(arg_cnt == 0){
            send_buf = "{\n\"status\":1,\n\"message\":\"Not login yet\"\n}";
        }else if(arg_cnt >= 1){
            if(Validate_Token(db, user) == false)
                send_buf = "{\n\"status\":1,\n\"message\":\"Not login yet\"\n}";
            else{
                if(arg_cnt == 1)
                    send_buf = "{\n\"status\":1,\n\"message\":\"Usage: post <user> <message>\"\n}";
                else{
                    sqlite3_stmt *stmt = NULL;
                    string sql;
                    sql = "INSERT INTO Posts (poster,message) VALUES ((SELECT userID FROM ValidTokens WHERE token=?),?);";
                    if(sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, NULL) == SQLITE_OK){
                        sqlite3_bind_text(stmt, 1, user.c_str(), -1, NULL);
                        sqlite3_bind_text(stmt, 2, arg[2], -1, NULL);
                        if(sqlite3_step(stmt) == SQLITE_DONE)
                            send_buf = "{\n\"status\":0,\n\"message\":\"Success!\"\n}";
                    }
                    sqlite3_finalize(stmt);
                }
            }
        }
    }else if(input_cmd == "receive-post"){
        if(arg_cnt == 0){
            send_buf = "{\n\"status\":1,\n\"message\":\"Not login yet\"\n}";
        }else if(arg_cnt >= 1){
            if(Validate_Token(db, user) == false)
                send_buf = "{\n\"status\":1,\n\"message\":\"Not login yet\"\n}";
            else{
                if(arg_cnt > 1)
                    send_buf = "{\n\"status\":1,\n\"message\":\"Usage: receive-post <user>\"\n}";
                else{
                    sqlite3_stmt *stmt = NULL;
                    string sql;
                    sql = "SELECT user, message FROM Users INNER JOIN Posts ON poster=id "
                            "INNER JOIN Friends ON friendBID=id WHERE friendAID="
                            "(SELECT userID FROM ValidTokens WHERE token=?);";
                    if(sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, NULL) == SQLITE_OK){
                        sqlite3_bind_text(stmt, 1, user.c_str(), -1, NULL);
                        send_buf = "{\n\"status\":0,\n\"post\":[";
                        while(sqlite3_step(stmt) == SQLITE_ROW){
                            send_buf = send_buf + "\n{\n\"id\":";
                            char id_tmp[256];
                            string id;
                            strcpy(id_tmp, (const char *)sqlite3_column_text(stmt, 0));
                            id = id_tmp;
                            send_buf = send_buf + "\"" + id + "\",\n\"message\":";
                            char message_tmp[256];
                            string message;
                            strcpy(message_tmp, (const char *)sqlite3_column_text(stmt, 1));
                            message = message_tmp;
                            send_buf = send_buf + "\"" + message + "\"\n},";
                        }
                        if(send_buf.back() == ',')
                            send_buf.pop_back();
                        send_buf = send_buf + "\n]\n}";
                    }
                    sqlite3_finalize(stmt);
                }
            }
        }
    }
    sqlite3_close(db);
    return send_buf;
}

void Initial_Server(int argc, char *argv[]){
    /*Create Socket */
    int sock_fd = 0, client_sock_fd = 0;
    sock_fd = socket(PF_INET, SOCK_STREAM, 0);
    if(sock_fd == -1) printf("Fail to create socket.\n");


    /*Info of Socket (Server) */
    struct sockaddr_in serverinfo, clientinfo;
    socklen_t client_addrlen = sizeof(clientinfo);
    memset(&serverinfo, 0, sizeof(serverinfo));
    serverinfo.sin_family = AF_INET;
    if(argc == 1){
        serverinfo.sin_port = htons(9000);
        inet_pton(AF_INET, "127.0.0.1", &serverinfo.sin_addr.s_addr);
    }else if(argc == 3){
        int port;
        sscanf(argv[2], "%d", &port);
        serverinfo.sin_port = htons(port);
        inet_pton(AF_INET, argv[1], &serverinfo.sin_addr.s_addr);
    }

    /* Connection */
    int conn_rv = bind(sock_fd, (struct sockaddr *)&serverinfo, sizeof(serverinfo));
    if(conn_rv == -1) printf("Fail to construct the server.\n");

    /* Listen */
    listen(sock_fd, 1);
    client_sock_fd = accept(sock_fd, (struct sockaddr*)&clientinfo, &client_addrlen);
    char recv_buf[256] = {};
    recv(client_sock_fd, recv_buf, sizeof(recv_buf), 0);
    cout << recv_buf << endl;
    int arg_cnt = Fetch_Command(recv_buf);
    cout << arg_cnt << endl;
    string send_buf = Command_Case(arg_cnt);

    /* Deal With send_msg Then Send It To Client */
    char send_msg[send_buf.size()] = {'\0'};
    strcpy(send_msg, send_buf.c_str());
    cout << send_msg << endl;
    send(client_sock_fd, send_msg, sizeof(send_msg), 0);
    close(sock_fd);
}

int main(int argc, char *argv[]){
    while(1){
        Initial_Server(argc, argv);
    }
    return 0;
}