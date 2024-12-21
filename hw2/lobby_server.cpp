#include <iostream>
#include <string>
#include <cstring>
#include <cstdlib>
#include <unistd.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/socket.h>
#include <vector>
#include <ifaddrs.h>
#include <net/if.h>
#include <limits>
#include <sys/time.h>
#include <map>
#include <vector>
#include <pthread.h>
#include <semaphore.h>
#include <sstream>

using namespace std;

struct Player{
    string username;
    string password;
    int status;
};

struct Room{
    int type, status, port, publicity;
    string name, owner;
};
string game[4] = {"Rock Paper Sciccer", "Game2", "Game3", "Gmae4"};
string room_status[2] = {"Waiting", "In Game"};
string player_status[2] = {"Waiting", "In Game"};
sem_t mutex_lock;
map<string, int>username;
map<string, Player>players;
map<int, string>current_player;
vector<int>clients;
vector<Room>rooms;
vector<pthread_t>threads;
string message_sent = "Welcome! Game client running...\nPlease select an option:\n1. Register\n2. Login\n3. Exit\nYour option: ";

void Register(int clientSocket){
    char buffer[1024];
    int bytesRead;
    string user, passwd, message;
    while(true){
        memset(buffer, 0, 1024);
        bytesRead = read(clientSocket, buffer, 1024);
        if (bytesRead <= 0) continue;
        buffer[bytesRead] = '\0';
        string response(buffer);
        size_t username_start = response.find("username:{");
        size_t password_start = response.find("password:{");
        if (username_start == string::npos || password_start == string::npos) {
            message = "Invalid format\n";
            send(clientSocket, message.c_str(), message.length(), 0);
            continue;
        }
        username_start += 10;
        password_start += 10;
        size_t username_end = response.find("}", username_start);
        size_t password_end = response.find("}", password_start);
        if (username_end == string::npos || password_end == string::npos) {
            message = "Invalid format\n";
            send(clientSocket, message.c_str(), message.length(), 0);
            continue;
        }
        user = response.substr(username_start, username_end - username_start);
        passwd = response.substr(password_start, password_end - password_start);
        sem_wait(&mutex_lock);
        if(username.find(user) != username.end()){
            message = "user already exist\n";
        }else{
            message = "REGISTER OK\n";
            Player player;
            player.username = user;
            player.password = passwd;
            player.status = 0;
            players[user] = player;
            username[user] = clientSocket;
        }
        sem_post(&mutex_lock);
        send(clientSocket, message.c_str(), message.length(), 0);
        if(message == "REGISTER OK\n"){
            break;
        }
    }
    return ;
}

void Login(int clientSocket){
    char buffer[1024];
    int bytesRead;
    string user, passwd, message;
    while(true){
        memset(buffer, 0, 1024);
        bytesRead = read(clientSocket, buffer, 1024);
        if (bytesRead <= 0) continue;
        buffer[bytesRead] = '\0';
        user = string(buffer);
        cout<<user;
        if(username.find(user) == username.end()){
            message = "User doesn't exist\n";
            send(clientSocket, message.c_str(), message.length(), 0);
            continue;
        }else{
            message = "User exist\n";
            send(clientSocket, message.c_str(), message.length(), 0);
        }
        break;
    }
    while(true){
        memset(buffer, 0, 1024);
        bytesRead = read(clientSocket, buffer, 1024);
        if (bytesRead <= 0) continue;
        buffer[bytesRead] = '\0';
        passwd = string(buffer);
        cout<<passwd;
        if(players[user].password != passwd){
            message = "Wrong password\n";
            send(clientSocket, message.c_str(), message.length(), 0);
            continue;
        }else{
            message = "Login success\n";
            send(clientSocket, message.c_str(), message.length(), 0);
            sem_wait(&mutex_lock);
            players[user].status = 0;
            current_player[clientSocket] = user;
            sem_post(&mutex_lock);
            break;
        }
    }
}

void Create_Room(int clientSocket){
    char buffer[1024];
    int bytesRead;
    string message;
    while(true){
        memset(buffer, 0, 1024);
        bytesRead = read(clientSocket, buffer, 1024);
        if (bytesRead <= 0) continue;
        buffer[bytesRead] = '\0';
        message = string(buffer);
        stringstream ss(message);
        string name;
        int type, port, pub;
        ss>>name>>type>>port>>pub;
        Room new_room;
        new_room.name = name;
        new_room.owner = players[current_player[clientSocket]].username;
        new_room.type = type;
        new_room.port = port;
        new_room.publicity = pub;
        new_room.status = 0;
        players[current_player[clientSocket]].status = 1;
        sem_wait(&mutex_lock);
        rooms.push_back(new_room);
        sem_post(&mutex_lock);
        break;
    }
}

void Join_Room(int clientSocket){
    char buffer[1024];
    int bytesRead;
    string message, response;
    while(true){
        memset(buffer, 0, 1024);
        bytesRead = read(clientSocket, buffer, 1024);
        if (bytesRead <= 0) continue;
        buffer[bytesRead] = '\0';
        message = string(buffer);
        int idx = -1;
        for(int i = 0; i<rooms.size(); i++){
            if(rooms[i].name == message && rooms[i].status == 0){
                idx = i;
                break;
            }
        }
        if(idx == -1){
            response = "room not exit or not available\n";
            send(clientSocket, response.c_str(), response.length(), 0);
        }else{
            sem_wait(&mutex_lock);
            rooms[idx].status = 1;
            players[current_player[clientSocket]].status = 1;
            sem_post(&mutex_lock);
            response = to_string(rooms[idx].type)+" "+to_string(rooms[idx].port);
            send(clientSocket, response.c_str(), response.length(), 0);
            break;
        }
    }
}

void* handleClient(void* arg) {
    int clientSocket = *(int*)arg;
    char buffer[1024];
    string username;
    INIY:
    while (true) {
        try{
            memset(buffer, 0, 1024);
            int bytesRead = read(clientSocket, buffer, 1024);
            if (bytesRead <= 0) break;
            buffer[bytesRead] = '\0';
            string response(buffer);
            response = response.substr(0, response.size()-1);
            if(response == "REGISTER"){
                Register(clientSocket);
            }else if(response == "LOGIN"){
                cout<<"checl\n";
                Login(clientSocket);
                break;
            }else if(response == "EXIT"){
                sem_wait(&mutex_lock);
                if(current_player.find(clientSocket) != current_player.end()){
                    players[current_player[clientSocket]].status = 0;
                    current_player.erase(clientSocket);
                }
                sem_post(&mutex_lock);
                close(clientSocket);
                return NULL;
            }
        }catch(exception){
            sem_wait(&mutex_lock);
            if(current_player.find(clientSocket) != current_player.end()){
                players[current_player[clientSocket]].status = 0;
                current_player.erase(clientSocket);
            }
            sem_post(&mutex_lock);
            close(clientSocket);
            return NULL;
        }
    }
    string message_sent2;
    message_sent2 = "Online players:\n";
    bool check = true;
    map<int, string>::iterator it;
    for(it = current_player.begin(); it != current_player.end(); it++){
        if(it->first == clientSocket) continue;
        message_sent2 += "Username: "+ it->second+", Status: "+player_status[players[it->second].status]+"\n";
        check = false;
    }
    if(check) message_sent2 += "Currently, no players are online.";
    message_sent2 += "\n\nGame Rooms:\n";
    check = true;
    for(int i = 0; i<rooms.size(); i++){
        if(rooms[i].publicity == 2) continue;
        message_sent2 += "Room Name: "+rooms[i].name+", Game Type: "+game[rooms[i].type-1]+", Public: Yes"+
                            ", Status: "+room_status[rooms[i].status]+", Owner: "+rooms[i].owner+"\n";
        check = false;
    }
    if(check) message_sent2 += "No public rooms waiting for players.\n";
    message_sent2 += "\n\n------------------------------------------\n";
    //message_sent2 += "Please select an option:\n1. Logout\n2. List rooms\n3. Create room\n4. Join room\nYour option: ";
    send(clientSocket, message_sent2.c_str(), message_sent2.length(), 0);
    while(true){
        try{
            memset(buffer, 0, 1024);
            int bytesRead = read(clientSocket, buffer, 1024);
            if (bytesRead <= 0) break;
            buffer[bytesRead] = '\0';
            string response(buffer);
            response = response.substr(0, response.size()-1);
            if(response == "LOGOUT"){
                sem_wait(&mutex_lock);
                if(current_player.find(clientSocket) != current_player.end()){
                    players[current_player[clientSocket]].status = 0;
                    current_player.erase(clientSocket);
                }
                sem_post(&mutex_lock);
                goto INIY;
            }else if(response == "LIST"){
                string message = "\n\nGame Rooms:\n";
                check = true;
                for(int i = 0; i<rooms.size(); i++){
                    if(rooms[i].publicity == 2) continue;
                    message += "Room Name: "+rooms[i].name+", Game Type: "+game[rooms[i].type-1]+", Public: Yes"+
                                        ", Status: "+room_status[rooms[i].status]+", Owner: "+rooms[i].owner+"\n";
                    check = false;
                }
                if(check) message = "No public rooms waiting for players.\n";
                send(clientSocket, message.c_str(), message.length(), 0);
            }else if(response == "CREATE"){
                Create_Room(clientSocket);
            }else if(response == "JOIN"){
                Join_Room(clientSocket);
            }else if(response.substr(0, 8) == "END_GAME"){
                string find_name = response.substr(9);
                //cout<<find_name<<"\n";
                sem_wait(&mutex_lock);
                for(int i = 0; i<rooms.size(); i++){
                    if(rooms[i].owner == find_name){
                        rooms.erase(rooms.begin()+i);
                    }
                }
                sem_post(&mutex_lock);
            }else if(response.substr(0, 9) == "2END_GAME"){
                string find_name = response.substr(10);
                //cout<<find_name<<"\n";
                sem_wait(&mutex_lock);
                players[find_name].status = 0;
                sem_post(&mutex_lock);
            }
        }catch(exception){
            sem_wait(&mutex_lock);
            if(current_player.find(clientSocket) != current_player.end()){
                players[current_player[clientSocket]].status = 0;
                current_player.erase(clientSocket);
            }
            sem_post(&mutex_lock);
            close(clientSocket);
            return NULL;
        }
    }
    close(clientSocket);
    return NULL;
}

int main(){
    string tcp_ip;
    int tcp_port, tcp_sock;
    while(true){
        cout<<"Please enter the server port(13400-13405):";
        cin>>tcp_port;
        // Create TCP socket
        tcp_sock = socket(AF_INET, SOCK_STREAM, 0);
        if (tcp_sock < 0) {
            perror("TCP socket creation failed");
            exit(EXIT_FAILURE);
        }
        // Server address setup
        struct sockaddr_in tcp_server_addr;
        memset(&tcp_server_addr, 0, sizeof(tcp_server_addr));
        tcp_server_addr.sin_family = AF_INET;
        tcp_server_addr.sin_port = htons(tcp_port);
        tcp_server_addr.sin_addr.s_addr = INADDR_ANY;

        if (bind(tcp_sock, (struct sockaddr *)&tcp_server_addr, sizeof(tcp_server_addr)) < 0) {
            cout<<"Can't not bind the TCP port, try another\n";
            close(tcp_sock);
            continue;
        }
        cout<< "TCP server listening on Port: " << tcp_port <<"\n";
        if (listen(tcp_sock, 10) < 0) {
            perror("Listen failed");
            close(tcp_sock);
            exit(EXIT_FAILURE);
        }
        break;
    }
    sem_init(&mutex_lock, 0, 1);
    int client_len = 0;
    while(true){
        pthread_t thread;
        int client = accept(tcp_sock, nullptr, nullptr);
        clients.push_back(client);
        threads.push_back(thread);
        pthread_create(&threads.back(), NULL, handleClient, &clients[client_len]);
        client_len++;
    }
    close(tcp_sock);
    return 0;
}