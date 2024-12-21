#include <iostream>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <limits>

#define BUFFER_SIZE 1024      // 缓冲区大小

using namespace std;

int main() {
    int sockfd, PORT;
    struct sockaddr_in server_addr;
    char buffer[BUFFER_SIZE];
    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("socket creation failed");
        exit(EXIT_FAILURE);
    }
    cout<<"Input your port(13400-13405): ";
    cin>>PORT;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);
    if (bind(sockfd, (const struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("bind failed");
        close(sockfd);
        exit(EXIT_FAILURE);
    }
    cout << "Waiting for invitation....\n";
    while(true){
        char buffer[BUFFER_SIZE];
        struct sockaddr_in client_addr;
        socklen_t len = sizeof(client_addr);
        memset(&client_addr, 0, sizeof(client_addr));
        ssize_t recv_len = recvfrom(sockfd, buffer, BUFFER_SIZE, 0, (struct sockaddr *)&client_addr, &len);
        if (recv_len < 0) {
            perror("Failed to receive invitation.");
            close(sockfd);
            exit(EXIT_FAILURE);
        }
        buffer[recv_len] = '\0';
        string message(buffer);

        if (message == "DISCOVER") {
            string response = "Server available";
            sendto(sockfd, response.c_str(), response.length(), 0, (const struct sockaddr *)&client_addr, len);
        }else if(message == "INVITE"){
            cout << "Receive invitation from "<<inet_ntoa(client_addr.sin_addr)<<":"<<ntohs(client_addr.sin_port)<<"\n";
            cout << "Accept invitaion?(y/n): ";
            char choice;
            cin >> choice;
            cin.ignore(numeric_limits<streamsize>::max(), '\n');
            string response;
            if (choice == 'y' || choice == 'Y') {
                response = "ACCEPT";
            } else {
                response = "REJECT";
            }
            sendto(sockfd, response.c_str(), response.length(), 0, (const struct sockaddr *)&client_addr, len);
            if(response == "ACCEPT") break;
        }
    }
    struct sockaddr_in client_addr;
    socklen_t len = sizeof(client_addr);
    memset(buffer, 0, BUFFER_SIZE);
    memset(&client_addr, 0, sizeof(client_addr));
    ssize_t recv_len = recvfrom(sockfd, buffer, BUFFER_SIZE, 0, (struct sockaddr *)&client_addr, &len);
    if (recv_len < 0) {
        perror("Failed to receive TCP info.");
        close(sockfd);
        exit(EXIT_FAILURE);
    }
    buffer[recv_len] = '\0';
    string message(buffer);
    cout<<"Received TCP server info: "<<message<<"\n";
    close(sockfd);

    char tcp_ip[INET_ADDRSTRLEN];
    int tcp_port;
    sscanf(buffer, "%[^:]:%d", tcp_ip, &tcp_port);
    cout << "Connecting to TCP server at IP: "<<tcp_ip<<" Port: "<<tcp_port<<"\n";
    // Create TCP socket
    int tcp_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (tcp_sock < 0) {
        perror("TCP socket creation failed");
        exit(EXIT_FAILURE);
    }
    // Server address setup
    struct sockaddr_in tcp_server_addr;
    memset(&tcp_server_addr, 0, sizeof(tcp_server_addr));
    tcp_server_addr.sin_family = AF_INET;
    tcp_server_addr.sin_port = htons(tcp_port);
    tcp_server_addr.sin_addr.s_addr = inet_addr(tcp_ip);
    // Connect to TCP server
    if (connect(tcp_sock, (struct sockaddr *)&tcp_server_addr, sizeof(tcp_server_addr)) < 0) {
        perror("Failed to connect to TCP server");
        close(tcp_sock);
        exit(EXIT_FAILURE);
    }
    cout<<"Connected to TCP server.\n";
    // Now exchange messages over TCP
    while (true) {
        // Receive message from client
        cout << "Enter your choise(rock/paper/sciccer): ";
        string message;
        getline(cin, message);
        if (send(tcp_sock, message.c_str(), message.length(), 0) < 0) {
            perror("Failed to send message");
            break;
        }
        if (message == "exit") {
            break;
        }
        cout<<"Wait for the result...\n";
        memset(buffer, 0, BUFFER_SIZE);
        ssize_t n = recv(tcp_sock, buffer, BUFFER_SIZE, 0);
        if (n <= 0) {
            cout << "Client disconnected or error occurred.\n";
            break;
        }
        buffer[n] = '\0';
        cout << buffer;
        /*memset(buffer, 0, BUFFER_SIZE);
        ssize_t n = recv(tcp_sock, buffer, BUFFER_SIZE, 0);
        if (n <= 0) {
            cout << "Client disconnected or error occurred.\n";
            break;
        }
        buffer[n] = '\0';
        cout << "Server: " << buffer <<endl;
        // Send message to client
        cout << "You: ";
        string message;
        getline(cin, message);
        if (message == "exit") {
            break;
        }
        if (send(tcp_sock, message.c_str(), message.length(), 0) < 0) {
            perror("Failed to send message");
            break;
        }*/
    }
    // Close TCP socket
    close(tcp_sock);
    return 0;
}
