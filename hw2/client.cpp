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
#include <sstream>
#include <termios.h>

using namespace std;

string username, password;
string game[4] = {"Rock Paper Sciccer", "Tic-tac-toe", "Game3", "Gmae4"};
string board[3][3];

string checkWin() {
    for (int i = 0; i < 3; i++) {
        if (board[i][0] == board[i][1] && board[i][1] == board[i][2]) return board[i][0];
        if (board[0][i] == board[1][i] && board[1][i] == board[2][i]) return board[0][i];
    }
    if (board[0][0] == board[1][1] && board[1][1] == board[2][2]) return board[0][0];
    if (board[0][2] == board[1][1] && board[1][1] == board[2][0]) return board[0][2];
    return " ";
}

bool checkDraw() {
    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 3; j++) {
            if (board[i][j] != "X" && board[i][j] != "O") return false;
        }
    }
    return true;
}

void resetBoard(){
    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 3; j++) {
            board[i][j] = ' ';
        }
    }
}

void printBoard(){
    cout<<"-------\n";
    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 3; j++) {
            cout<<"|"<< board[i][j];
        }
        cout<<"|\n";
        cout<<"-------\n";
    }
}

void printRock() {
    cout << "    _______\n";
    cout << "---'   ____)____\n";
    cout << "       (______)    \n";
    cout << "       (______)    \n";
    cout << "       (____)\n";
    cout << "---.__(___)\n";
}

void printPaper() {
    cout << "    _______\n";
    cout << "---'   ____)____\n";
    cout << "          ______)\n";
    cout << "          _______)\n";
    cout << "         _______)\n";
    cout << "---.__________)\n";
}

void printScissors() {
    cout << "    _______\n";
    cout << "---'   ____)____\n";
    cout << "          ______)\n";
    cout << "       __________)\n";
    cout << "      (____)\n";
    cout << "---.__(___)\n";
}

void rock_paper_sciccer(string player1, string chose1, string player2, string chose2){
    cout<<player1<<": ";
    if(chose1 == "rock"){
        cout<<"rock\n";
        printRock();
    }else if(chose1 == "paper"){
        cout<<"paper\n";
        printPaper();
    }else if(chose1 == "sciccer"){
        cout<<"sciccer\n";
        printScissors();
    }
    cout<<player2<<": ";
    if(chose2 == "rock"){
        cout<<"rock\n";
        printRock();
    }else if(chose2 == "paper"){
        cout<<"paper\n";
        printPaper();
    }else if(chose2 == "sciccer"){
        cout<<"sciccer\n";
        printScissors();
    }
    if(chose1 == chose2){
        cout<<"Tie!\n";
    }else if((chose1 == "rock" && chose2 == "paper") || (chose1 == "paper" && chose2 == "sciccer") || (chose1 == "sciccer" && chose2 == "rock")){
        cout<<"Winner: "<<player2<<"\n";
    }else{
        cout<<"Winner: "<<player1<<"\n";
    }
}

char *get_ip_address(string interface_name) {
    struct ifaddrs *ifaddr, *ifa;
    static char ip[INET_ADDRSTRLEN]; // Static to return pointer

    if (getifaddrs(&ifaddr) == -1) {
        perror("Error getting network interfaces");
        return NULL;
    }

    // Initialize IP string
    ip[0] = '\0';

    // Iterate through the linked list of interfaces
    for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
        // Check if interface matches and address family is IPv4
        if (ifa->ifa_addr == NULL)
            continue;

        if ((strcmp(ifa->ifa_name, interface_name.c_str()) == 0) &&
            (ifa->ifa_addr->sa_family == AF_INET)) {

            struct sockaddr_in *sa = (struct sockaddr_in *)ifa->ifa_addr;
            inet_ntop(AF_INET, &(sa->sin_addr), ip, INET_ADDRSTRLEN);
            break; // IP address found, exit loop
        }
    }

    freeifaddrs(ifaddr);

    if (strlen(ip) == 0) {
        cout<<"No IPv4 address found for interface "<<interface_name<<"\n";
        return NULL;
    }

    return ip;
}

void Register(int tcp_sock){
    string message = "REGISTER\n";
    send(tcp_sock, message.c_str(), message.length(), 0);
    char buffer[1024];
    int bytesRead;
    termios oldt, newt;
    while(true){
        cout<<"Please enter your uaername:";
        cin>>username;
        tcgetattr(STDIN_FILENO, &oldt);
        newt = oldt;
        // 關閉回顯
        newt.c_lflag &= ~ECHO;
        tcsetattr(STDIN_FILENO, TCSANOW, &newt);

        // 顯示提示訊息並獲取輸入
        cout<<"Please enter your password:";
        cin>>password;
        cout << endl;
        // 恢復原始終端設定
        tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
        string message = "username:{"+username+"}, password:{"+password+"}\n";
        send(tcp_sock, message.c_str(), message.length(), 0);
        memset(buffer, 0, 1024);
        bytesRead = read(tcp_sock, buffer, 1024);
        if (bytesRead <= 0) continue;
        buffer[bytesRead] = '\0';
        string response(buffer);
        response = response.substr(0, response.size()-1);
        if(response == "user already exist"){
            cout<<response<<endl;
        }else if(response == "REGISTER OK"){
            cout<<"REGISTER SUCCES\n";
            break;
        }
    }
    return ;
}

void Login(int tcp_sock){
    string message = "LOGIN\n";
    send(tcp_sock, message.c_str(), message.length(), 0);
    char buffer[1024];
    int bytesRead;
    string response;
    termios oldt, newt;
    while(true){
        cout<<"Please enter your username:";
        cin>>username;
        send(tcp_sock, username.c_str(), username.length(), 0);
        memset(buffer, 0, 1024);
        bytesRead = read(tcp_sock, buffer, 1024);
        if (bytesRead <= 0) continue;
        buffer[bytesRead] = '\0';
        response = string(buffer);
        response = response.substr(0, response.size()-1);
        if(response == "User doesn't exist"){
            cout<<response<<endl;
            continue;
        }
        break;
    }
    while(true){
        tcgetattr(STDIN_FILENO, &oldt);
        newt = oldt;
        // 關閉回顯
        newt.c_lflag &= ~ECHO;
        tcsetattr(STDIN_FILENO, TCSANOW, &newt);

        // 顯示提示訊息並獲取輸入
        cout<<"Please enter your password:";
        cin>>password;
        cout << endl;
        // 恢復原始終端設定
        tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
        send(tcp_sock, password.c_str(), password.length(), 0);
        memset(buffer, 0, 1024);
        bytesRead = read(tcp_sock, buffer, 1024);
        //cout<<bytesRead<<endl;
        if (bytesRead <= 0) continue;
        buffer[bytesRead] = '\0';
        response = string(buffer);
        response = response.substr(0, response.size()-1);
        cout<<response<<endl;
        if(response == "Wrong password"){
            cout<<response<<endl;
            continue;
        }
        break;
    }
    return ;
}

void Exit(int tcp_sock){
    string message = "EXIT\n";
    send(tcp_sock, message.c_str(), message.length(), 0);
    close(tcp_sock);
    return ;
}

void Logout(int tcp_sock){
    string message = "LOGOUT\n";
    send(tcp_sock, message.c_str(), message.length(), 0);
    return ;
}

void List(int tcp_sock){
    string message = "LIST\n";
    send(tcp_sock, message.c_str(), message.length(), 0);
    char buffer[1024];
    int bytesRead;
    memset(buffer, 0, 1024);
    bytesRead = read(tcp_sock, buffer, 1024);
    if (bytesRead <= 0) return ;
    buffer[bytesRead] = '\0';
    string response = string(buffer);
    cout<<response;
    return ;
}

void Create(int tcp_sock){
    string message = "CREATE\n";
    send(tcp_sock, message.c_str(), message.length(), 0);
    string name;
    int type, pub, port;
    while(true){
        cout<<"Please enter the room name: ";
        cin>>name;
        cout<<"Please enter the game type(1: Rock, Paper, Scissor / 2: Tic-tac-toe): ";
        cin>>type;
        cout<<"Is this room public?(1:Yes, 2:No): ";
        cin>>pub;
        if(pub <=0 || pub > 2 || type <= 0 || type > 2){
            cout<<"Variable out of range\n";
            continue;
        }
        break;
    }
    int sockfd;
    struct sockaddr_in server_addr;
    while(true){
        cout<<"Please enter the port number to bind(10000-65535): ";
        cin>>port;
        // Create TCP socket
        sockfd = socket(AF_INET, SOCK_STREAM, 0);
        if (sockfd < 0) {
            cout<<"TCP socket creation failed\n";
            continue;
        }
        // Server address setup
        memset(&server_addr, 0, sizeof(server_addr));
        server_addr.sin_family = AF_INET;
        server_addr.sin_port = htons(port);
        server_addr.sin_addr.s_addr = INADDR_ANY;

        if (bind(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
            cout<<"Can't not bind the TCP port, try another\n";
            close(sockfd);
            continue;
        }
        if (listen(sockfd, 2) < 0) {
            cout<<"Listen failed\n";
            close(sockfd);
            continue;
        }
        break;
    }
    message = name+" "+to_string(type)+" "+to_string(port)+" "+to_string(pub)+"\n";
    send(tcp_sock, message.c_str(), message.length(), 0);
    TYPE1:
    cout << "Waiting for Player....\n";
    char buffer[1024];
    int bytesRead;
    TRY2:
    int client = accept(sockfd, nullptr, nullptr);
    string player;
    while(true){
        memset(buffer, 0, 1024);
        bytesRead = read(client, buffer, 1024);
        if (bytesRead <= 0) continue;
        buffer[bytesRead] = '\0';
        string response = string(buffer);
        TRY1:
        cout<<response<<" want to join the room? (1:accept, 2:reject): ";
        int ac;
        cin>>ac;
        if(ac == 1){
            message = "Accept "+username+"\n";
            player = response;
            send(client, message.c_str(), message.length(), 0);
            break;
        }else if(ac == 2){
            message = "Reject "+username+"\n";
            send(client, message.c_str(), message.length(), 0);
            close(client);
            goto TRY2;
        }else{
            cout<<"Invalid option\n";
            goto TRY1;
        }
    }

    cout << "Welcome to "<<game[type-1]<<"\n";
    resetBoard();
    while(true){
        if(type == 1){
            cout<<"Enter your choise(rock/paper/sciccer) or 'exit' to quit: ";
            cin>>message;
            if(message == "exit"){
                send(client, message.c_str(), message.length(), 0);
                break;
            }else if(message != "rock" && message != "paper" && message != "sciccer"){
                cout<<"Invalid input\n";
                continue;
            }
            send(client, message.c_str(), message.length(), 0);
            memset(buffer, 0, 1024);
            bytesRead = read(client, buffer, 1024);
            if (bytesRead <= 0) continue;
            buffer[bytesRead] = '\0';
            string response = string(buffer);
            if(response == "exit"){
                cout<<player<<" leave the room\nGame end\n";
                break;
            }
            cout<<"Wait for the result...\n";
            rock_paper_sciccer(username, message, player, response);
        }else if(type == 2){
            cout<<"Enter your coordinate(0<=x,y<3, ex: 0 1) or -1 -1 to quit: ";
            int x, y;
            cin>>x>>y;
            if(x == -1 && y == -1){
                message = to_string(x)+" "+to_string(y);
                send(client, message.c_str(), message.length(), 0);
                break;
            }else if(x < 0 || x >= 3 || y < 0 || y > 3){
                cout<<"Invalid input\n";
                continue;
            }
            message = to_string(x)+" "+to_string(y);
            send(client, message.c_str(), message.length(), 0);
            cout<<username<<": "<<message<<"\n";
            board[x][y] = "O";
            printBoard();
            string result = checkWin();
            if(result == "O"){\
                cout<<"Winner: "<<username<<"\n";
                break;
            }
            else if(result == "X"){
                cout<<"Winner: "<<player<<"\n";
                break;
            }
            else if(checkDraw()){
                cout<<"Tie!\n";
                break;
            }
            memset(buffer, 0, 1024);
            bytesRead = read(client, buffer, 1024);
            if (bytesRead <= 0) continue;
            buffer[bytesRead] = '\0';
            string response = string(buffer);
            stringstream ss1(response);
            ss1>>x>>y;
            if(x == -1 && y == -1){
                cout<<player<<" leave the room\nGame end\n";
                break;
            }
            cout<<username<<": "<<response<<"\n";
            board[x][y] = "X";
            printBoard();
            result = checkWin();
            if(result == "O"){\
                cout<<"Winner: "<<username<<"\n";
                break;
            }
            else if(result == "X"){
                cout<<"Winner: "<<player<<"\n";
                break;
            }
            else if(checkDraw()){
                cout<<"Tie!\n";
                break;
            }

        }
    }
    close(sockfd);
    close(client);
    message = "END_GAME "+username+"\n";
    send(tcp_sock, message.c_str(), message.length(), 0);
    cout<<"Game end\n";
    return ;
}

void Join(int tcp_sock){
    string message = "JOIN\n";
    send(tcp_sock, message.c_str(), message.length(), 0);
    string name;
    char buffer[1024];
    int bytesRead;
    int type, port;
    while(true){
        cout<<"Please enter the room name: ";
        cin>>name;
        send(tcp_sock, name.c_str(), name.length(), 0);
        memset(buffer, 0, 1024);
        bytesRead = read(tcp_sock, buffer, 1024);
        if (bytesRead <= 0) continue;
        buffer[bytesRead] = '\0';
        string response = string(buffer);
        if(response == "room not exit or not available\n"){
            cout<<response;
            continue;
        }
        char *ip_address = get_ip_address("ens224");
        stringstream ss(response);
        ss>>type>>port;
        cout<<"Game type: "<<game[type-1]<<"\n";
        cout<<"Game server ip: "<<ip_address<<"\n";
        cout<<"Game server port: "<<port<<"\n";
        break;
    }
    // Create TCP socket
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("TCP socket creation failed");
        exit(EXIT_FAILURE);
    }
    // Server address setup
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    server_addr.sin_addr.s_addr = INADDR_ANY;
    // Connect to TCP server
    if (connect(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        cout<<"Failed to connect to TCP server"<<"\n";
        close(sockfd);
    }
    message = username;
    send(sockfd, message.c_str(), message.length(), 0);
    memset(buffer, 0, 1024);
    bytesRead = read(sockfd, buffer, 1024);
    buffer[bytesRead] = '\0';
    string response = string(buffer);
    stringstream ss1(response);
    string Ac, uname;
    ss1>>Ac>>uname;
    if(Ac == "Reject"){
        cout<<uname<<"reject your entering\n";
        return ;
    }
    cout << "Welcome to "<<game[type-1]<<"\n";
    resetBoard();
    while(true){
        if(type == 1){
            cout<<"Enter your choise(rock/paper/sciccer) or 'exit' to quit: ";
            cin>>message;
            if(message == "exit"){
                send(sockfd, message.c_str(), message.length(), 0);
                break;
            }else if(message != "rock" && message != "paper" && message != "sciccer"){
                cout<<"Invalid input\n";
                continue;
            }
            send(sockfd, message.c_str(), message.length(), 0);
            memset(buffer, 0, 1024);
            bytesRead = read(sockfd, buffer, 1024);
            if (bytesRead <= 0) continue;
            buffer[bytesRead] = '\0';
            string response = string(buffer);
            if(response == "exit"){
                cout<<uname<<" leave the room\nGame end\n";
                break;
            }
            cout<<"Wait for the result...\n";
            rock_paper_sciccer(username, message, uname, response);
        }else if(type == 2){
            memset(buffer, 0, 1024);
            bytesRead = read(sockfd, buffer, 1024);
            if (bytesRead <= 0) continue;
            buffer[bytesRead] = '\0';
            string response = string(buffer);
            stringstream ss1(response);
            int x, y;
            ss1>>x>>y;
            if(x == -1 && y == -1){
                cout<<uname<<" leave the room\nGame end\n";
                break;
            }
            cout<<username<<": "<<response<<"\n";
            board[x][y] = "O";
            printBoard();
            string result = checkWin();
            if(result == "X"){\
                cout<<"Winner: "<<username<<"\n";
                break;
            }
            else if(result == "O"){
                cout<<"Winner: "<<uname<<"\n";
                break;
            }
            else if(checkDraw()){
                cout<<"Tie!\n";
                break;
            }
            cout<<"Enter your coordinate(0<=x,y<3, ex: 0 1) or -1 -1 to quit: ";
            cin>>x>>y;
            if(x == -1 && y == -1){
                message = to_string(x)+" "+to_string(y);
                send(sockfd, message.c_str(), message.length(), 0);
                break;
            }else if(x < 0 || x >= 3 || y < 0 || y > 3){
                cout<<"Invalid input\n";
                continue;
            }
            message = to_string(x)+" "+to_string(y);
            send(sockfd, message.c_str(), message.length(), 0);
            cout<<username<<": "<<message<<"\n";
            board[x][y] = "X";
            printBoard();
            result = checkWin();
            if(result == "X"){\
                cout<<"Winner: "<<username<<"\n";
                break;
            }
            else if(result == "O"){
                cout<<"Winner: "<<uname<<"\n";
                break;
            }
            else if(checkDraw()){
                cout<<"Tie!\n";
                break;
            }

        }
    }   
    message = "2END_GAME "+username+"\n";
    send(tcp_sock, message.c_str(), message.length(), 0);
    close(sockfd);
    cout<<"Game end\n";
    return ;
}

int main(){
    string tcp_ip;
    string message_sent2;
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

        // Connect to TCP server
        if (connect(tcp_sock, (struct sockaddr *)&tcp_server_addr, sizeof(tcp_server_addr)) < 0) {
            cout<<"Failed to connect to TCP server"<<"\n";
            close(tcp_sock);
        }
        break;
    }
    struct timeval tv;
    tv.tv_sec = 5;  // 設置5秒超時
    tv.tv_usec = 0;
    setsockopt(tcp_sock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof tv);
    WELCOME:
    string message_sent = "Welcome! Game client running...\n";
    cout<<message_sent;
    int option;
    message_sent2 = "Please select an option:\n1. Register\n2. Login\n3. Exit\nYour option: ";
    while(true){
        cout<<message_sent2;
        cin>>option;
        if(option == 1){
            Register(tcp_sock);
        }else if(option == 2){
            Login(tcp_sock);
            break;
        }else if(option == 3){
            Exit(tcp_sock);
            break;
        }else{
            cout<<"Option out of range!\n";
        }
    }
    if(option == 2){
        char buffer[1024];
        int bytesRead;
        string response;
        memset(buffer, 0, 1024);
        //cout<<"check1"<<endl;
        bytesRead = read(tcp_sock, buffer, 1024);
        //cout<<bytesRead<<endl;
        //cout<<"check2"<<endl;
        buffer[bytesRead] = '\0';
        response = string(buffer);
        cout<<response<<endl;
        //cout<<"check3"<<endl;
        message_sent2 = "Please select an option:\n1. Logout\n2. List rooms\n3. Create room\n4. Join room\nYour option: ";
        while(true){
            cout<<message_sent2;
            cin>>option;
            if(option == 1){
                Logout(tcp_sock);
                goto WELCOME;
            }else if(option == 2){
                List(tcp_sock);
            }else if(option == 3){
                Create(tcp_sock);
            }else if(option == 4){
                Join(tcp_sock);
            }else{
                cout<<"Option out of range!\n";
            }
        }
    }
    close(tcp_sock);
    return 0;
}