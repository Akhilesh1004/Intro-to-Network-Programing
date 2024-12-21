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

#define BUFFER_SIZE 1024

const int PORT_START = 13400;
const int PORT_END = 13405;

using namespace std;

string ips[4] = {"140.113.235.151", "140.113.235.152", "140.113.235.153", "140.113.235.154"};
//string ips[1] = {"192.168.2.101"};

struct ServerInfo {
    string ip;
    int port;
};

vector<ServerInfo> discover_servers() {
    vector<ServerInfo> servers;
    for(int i = 0; i<4; i++){
        for (int port = PORT_START; port <= PORT_END; ++port) {
            int sockfd;
            if((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
                perror("socket creation failed");
                continue;
            }
            // Set timeout for the socket
            struct timeval tv;
            tv.tv_sec = 1;  // Timeout in seconds
            tv.tv_usec = 0; // Not used, set to 0
            if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0) {
                perror("Error setting socket timeout");
                close(sockfd);
                continue;
            }
            //cout<<port<<"searching..\n";
            struct sockaddr_in udp_addr;
            memset(&udp_addr, 0, sizeof(udp_addr));
            udp_addr.sin_family = AF_INET;
            udp_addr.sin_port = htons(port);
            udp_addr.sin_addr.s_addr = inet_addr(ips[i].c_str());

            // send DISCOVER
            string message = "DISCOVER";
            if (sendto(sockfd, message.c_str(), message.length(), 0, (struct sockaddr *)&udp_addr, sizeof(udp_addr)) < 0) {
                perror("sendto failed");
                close(sockfd);
                continue;
            }
            char buffer[BUFFER_SIZE];
            memset(buffer, 0, BUFFER_SIZE);
            socklen_t len = sizeof(udp_addr);
            int n = recvfrom(sockfd, buffer, BUFFER_SIZE, 0, (struct sockaddr *)&udp_addr, &len);
            if (n < 0) {
                continue;
            } else {
                buffer[n] = '\0';
                string response(buffer);
                if (response == "Server available") servers.push_back({ips[i], port});
            }
            close(sockfd);
        }
    }
    return servers;
}

bool send_invitation(string server_ip, int server_port) {
    int sockfd;
    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("socket creation failed");
        exit(EXIT_FAILURE);
    }
    // Set timeout for the socket
    struct timeval tv;
    tv.tv_sec = 10;  // Timeout in seconds
    tv.tv_usec = 0; // Not used, set to 0
    if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0) {
        perror("Error setting socket timeout");
        close(sockfd);
        exit(EXIT_FAILURE);
    }
    struct sockaddr_in udp_server_addr;
    memset(&udp_server_addr, 0, sizeof(udp_server_addr));
    udp_server_addr.sin_family = AF_INET;
    udp_server_addr.sin_port = htons(server_port);
    udp_server_addr.sin_addr.s_addr = inet_addr(server_ip.c_str());

    string message = "INVITE";
    if (sendto(sockfd, message.c_str(), message.length(), 0, (struct sockaddr *)&udp_server_addr, sizeof(udp_server_addr)) < 0) {
        perror("sendto failed");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    cout << "Sent invitaion to server "<<server_ip<<":"<<server_port<<".\n";
    bool flag = false;
    char buffer[BUFFER_SIZE];
    memset(buffer, 0, BUFFER_SIZE);
    socklen_t len = sizeof(udp_server_addr);
    int n = recvfrom(sockfd, buffer, BUFFER_SIZE, 0, (struct sockaddr *)&udp_server_addr, &len);
    if (n < 0) {
        cout << "Server " << server_ip << ":" << server_port << " no response" << endl;
    } else {
        buffer[n] = '\0';
        string response(buffer);
        if (response == "ACCEPT") {
            cout << "Server " << server_ip << ":" << server_port<< " accept the invitaion." << endl;
            flag = true;
        } else if (response == "REJECT") {
            cout << "Server " << server_ip << ":" << server_port<< " reject the invitaion." << endl;
        } else {
            cout << "Unkown response: " << response << endl;
        }
    }

    close(sockfd);
    return flag;
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


void send_tcp(string server_ip, int server_port) {
    // Create UDP socket
    int udp_sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (udp_sock < 0) {
        perror("UDP socket creation failed");
        exit(EXIT_FAILURE);
    }

    // Server address setup
    struct sockaddr_in udp_server_addr;
    memset(&udp_server_addr, 0, sizeof(udp_server_addr));
    udp_server_addr.sin_family = AF_INET;
    udp_server_addr.sin_port = htons(server_port);
    udp_server_addr.sin_addr.s_addr = inet_addr(server_ip.c_str());

    // Create TCP server
    int tcp_listen_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (tcp_listen_sock < 0) {
        perror("TCP socket creation failed");
        close(udp_sock);
        exit(EXIT_FAILURE);
    }

    string interface_name = "ens224";
    char *ip_address = get_ip_address(interface_name);

    if (ip_address == NULL) {
        fprintf(stderr, "Failed to get IP address for interface %s\n", interface_name);
        exit(EXIT_FAILURE);
    }

    // Bind to any available port
    struct sockaddr_in tcp_server_addr;
    memset(&tcp_server_addr, 0, sizeof(tcp_server_addr));
    tcp_server_addr.sin_family = AF_INET;
    tcp_server_addr.sin_port = 0; // Let system choose the port
    tcp_server_addr.sin_addr.s_addr = inet_addr(ip_address);

    if (bind(tcp_listen_sock, (struct sockaddr *)&tcp_server_addr, sizeof(tcp_server_addr)) < 0) {
        perror("TCP bind failed");
        close(tcp_listen_sock);
        close(udp_sock);
        exit(EXIT_FAILURE);
    }

    // Get the port number assigned
    socklen_t tcp_server_addr_len = sizeof(tcp_server_addr);
    if (getsockname(tcp_listen_sock, (struct sockaddr *)&tcp_server_addr, &tcp_server_addr_len) == -1) {
        perror("getsockname failed");
        close(tcp_listen_sock);
        close(udp_sock);
        exit(EXIT_FAILURE);
    }
    int tcp_port = ntohs(tcp_server_addr.sin_port);
    char tcp_ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &(tcp_server_addr.sin_addr), tcp_ip, INET_ADDRSTRLEN);

    cout<< "TCP server listening on IP: " << tcp_ip << " Port: " << tcp_port <<"\n";

    if (listen(tcp_listen_sock, 1) < 0) {
        perror("Listen failed");
        close(tcp_listen_sock);
        close(udp_sock);
        exit(EXIT_FAILURE);
    }

    // Send TCP server IP and port to UDP server
    char tcp_info[BUFFER_SIZE];
    snprintf(tcp_info, BUFFER_SIZE, "%s:%d", tcp_ip, tcp_port);
    if (sendto(udp_sock, tcp_info, strlen(tcp_info), 0,
               (struct sockaddr *)&udp_server_addr, sizeof(udp_server_addr)) < 0) {
        perror("Failed to send TCP info to server");
        close(tcp_listen_sock);
        close(udp_sock);
        exit(EXIT_FAILURE);
    }
    cout<<"TCP server info sent to UDP server.\n";

    // Close UDP socket as it's no longer needed
    close(udp_sock);

    // Wait for server to connect via TCP
    struct sockaddr_in client_addr;
    socklen_t client_addr_len = sizeof(client_addr);
    int tcp_conn_sock = accept(tcp_listen_sock, (struct sockaddr *)&client_addr, &client_addr_len);
    if (tcp_conn_sock < 0) {
        perror("Accept failed");
        close(tcp_listen_sock);
        exit(EXIT_FAILURE);
    }
    cout << "TCP connection established with server.\n";

    // Now exchange messages over TCP
    char buffer[BUFFER_SIZE];
    while (true) {
        // Send message to server
        cout << "Enter your choise(rock/paper/sciccer): ";
        string message;
        getline(cin, message);
        if (message == "exit") {
            break;
        }
        cout<<"Wait for the result...\n";
        memset(buffer, 0, BUFFER_SIZE);
        ssize_t n = recv(tcp_conn_sock, buffer, BUFFER_SIZE, 0);
        if (n <= 0) {
            cout << "Client disconnected or error occurred.\n";
            break;
        }
        buffer[n] = '\0';
        string response(buffer);
        string result_sent = "You: ";
        string result = "tie.\n";
        result_sent += response;
        result_sent += "\n";
        result_sent += "Server: ";
        result_sent += message;
        result_sent += "\n";
        result_sent += "result: ";
        if(response == message) result_sent += "tie.\n";
        else if(response == "paper"){
            if(message == "sciccer"){
                result_sent += "you lose.\n";
                result = "you win.\n";
            }
            else if(message == "rock"){
                result_sent += "you win.\n";
                result = "you lose.\n";
            }
        }else if(response == "sciccer"){
            if(message == "rock"){
                result_sent += "you lose.\n";
                result = "you win.\n";
            }
            else if(message == "paper"){
                result_sent += "you win.\n";
                result = "you lose.\n";
            }
        }else if(response == "rock"){
            if(message == "paper"){
                result_sent += "you lose.\n";
                result = "you win.\n";
            }
            else if(message == "sciccer"){
                result_sent += "you win.\n";
                result = "you lose.\n";
            }
        }else if(response == "exit"){
            cout<<"Server leave the game\n";
            break;
        }
        if (send(tcp_conn_sock, result_sent.c_str(), result_sent.length(), 0) < 0) {
            perror("Failed to send message");
            break;
        }
        cout<<"You: "<<message<<"\n";
        cout<<"Server: "<<response<<"\n";
        cout<<"result: "<<result;
        /*cout << "You: ";
        string message;
        getline(cin, message);
        if (message == "exit") {
            break;
        }
        if (send(tcp_conn_sock, message.c_str(), message.length(), 0) < 0) {
            perror("Failed to send message");
            break;
        }
        // Receive message from server
        memset(buffer, 0, BUFFER_SIZE);
        ssize_t n = recv(tcp_conn_sock, buffer, BUFFER_SIZE, 0);
        if (n <= 0) {
            cout << "Server disconnected or error occurred.\n";
            break;
        }
        buffer[n] = '\0';
        cout << "Server: "<<buffer<<"\n";*/

    }

    // Close sockets
    close(tcp_conn_sock);
    close(tcp_listen_sock);
}

int main() {
    vector<ServerInfo> servers = discover_servers();

    if (servers.empty()) {
        cout << "No service available.\n";
    } else {
        cout << "Availbale server:\n";
        for (size_t i = 0; i < servers.size(); ++i) {
            cout << i + 1 << ". " << servers[i].ip << ":" << servers[i].port<<"\n";
        }
        string server_ip;
        int server_port;
        while(true){
            cout << "Enter the number to choose server: ";
            int choice;
            cin >> choice;
            cin.ignore(numeric_limits<streamsize>::max(), '\n');
            if (choice >= 1 && choice <= servers.size()) {
                if(send_invitation(servers[choice - 1].ip, servers[choice - 1].port)){
                    server_ip = servers[choice - 1].ip;
                    server_port =  servers[choice - 1].port;
                    break;
                }
            } else {
                cout<<"Number out of range.\n";
            }
        }
        send_tcp(server_ip, server_port);
    }

    return 0;
}
