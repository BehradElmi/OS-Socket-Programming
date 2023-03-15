#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <netdb.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>

#define SERVER_ADDRESS "127.0.0.1"
#define SERVER_BROADCAST "127.255.255.255"
#define DEFAULT_SERVER_PORT 8090
#define BUFFER_SIZE 1024
#define RECIEVE_ERROR "Error On Reading From the Server"
#define PATTERN "$$$"
#define EXIT_PAT "exit\n"

int connect_server(int port);

void recieved_handler(char* buffer, int num_of_bytes);

int port_pattern(char* buffer);

void room_communication(int port, int serverFD);

int main(int argc, char* argv)
{
    char buffer[BUFFER_SIZE] = {0};
    int client_fd = connect_server(DEFAULT_SERVER_PORT);
    fd_set temp, master_set;
    FD_ZERO(&master_set);
    FD_SET(0, &master_set);
    FD_SET(client_fd, &master_set); // 3 is client first socket 4 is server sending menu
    while(1)
    {
        temp = master_set;
        select(client_fd+1, &temp, NULL, NULL, NULL); // client 3 server 4
        if(FD_ISSET(0, &temp))
        {
            memset(buffer, 0, sizeof(buffer)/sizeof(buffer[0]));
            read(0, buffer, BUFFER_SIZE); // Read client inp and Sent to Server
            send(client_fd, buffer, strlen(buffer), 0);
        }
        else if(FD_ISSET(3, &temp))
        {
            // recieve from server
            int recieved_bytes = recv(3, buffer, BUFFER_SIZE, 0);
            recieved_handler(buffer, recieved_bytes);
        }
        memset(buffer, 0, (size_t)sizeof(buffer)/sizeof(buffer[0]));
    }
}

int connect_server(int port)
{
    int fd;
    struct sockaddr_in server_address;
    int opt = 1;
    fd = socket(AF_INET, SOCK_STREAM, 0);
    server_address.sin_family = AF_INET; 
    server_address.sin_port = htons(port); 
    server_address.sin_addr.s_addr = inet_addr(SERVER_ADDRESS);
    if (connect(fd, (struct sockaddr *)&server_address, sizeof(server_address)) < 0) { // checking for errors
        printf("Error in connecting to server\n");
    }
    
    return fd;
}

void recieved_handler(char* buffer, int num_of_bytes)
{
    if(num_of_bytes == 0)
    {
        return;
    }
    else if(num_of_bytes > 0)
    {
        int port = port_pattern(buffer);
        if(port <= 0)
        {
            write(1, buffer, num_of_bytes);
            return;
        }
        else
        {
            room_communication(port, 3);
        }
    }
    else 
    {
        perror(RECIEVE_ERROR);
        int log = open("log.txt", O_APPEND, O_CREAT);
        write(log, RECIEVE_ERROR, strlen(RECIEVE_ERROR));
        close(log);
        return;
    }
}

int port_pattern(char* buffer)
{
    if(strncmp(buffer, PATTERN, strlen(PATTERN)) == 0)
    {
        char* token = strtok(buffer, PATTERN);
        int port = atoi(token);
        return port;
    }
    return 0;
}

void room_communication(int port, int serverFD)
{
    struct sockaddr_in sock_adr;
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if(socket < 0)
    {
        perror("error socket");
    }
    // broadcast specs:
    sock_adr.sin_port = htons(port);
    sock_adr.sin_family = AF_INET;
    sock_adr.sin_addr.s_addr = inet_addr(SERVER_BROADCAST);
    int broadcast = 1;
    if(setsockopt(sock, SOL_SOCKET, SO_BROADCAST, &broadcast,
        sizeof(broadcast)) < 0)
        {
            perror("error setsocketopt broadcast");
        }
    if(setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &broadcast,
        sizeof(broadcast)) < 0)
    {
        perror("error setsocket reuse");
    }
    if(bind(sock, (const struct sockaddr*) &sock_adr, 
        (socklen_t)sizeof(sock_adr)) < 0)
    {
        perror("error binding");
    }
    char b_buf[BUFFER_SIZE];
    int sock_adr_len = sizeof(sock_adr);
    memset(b_buf, 0, BUFFER_SIZE);
    fd_set temp, master;
    FD_ZERO(&master);
    FD_SET(0, &master);
    FD_SET(sock, &master);
    FD_SET(serverFD, &master);
    while(1)
    {
        memset(b_buf, 0, BUFFER_SIZE);
        temp = master;
        if(select(sock+1, &temp, NULL, 
            NULL, NULL) < 0)
        {
            perror("socket error");
        }
        if(FD_ISSET(0, &temp)) // read triggered
        {
            read(0, b_buf, BUFFER_SIZE);
            if(strcmp(b_buf, EXIT_PAT) == 0)
            {
                send(serverFD, EXIT_PAT, strlen(EXIT_PAT), 0); // trigger server
                return;
            }
            // int sch = sendto(serverFD, b_buf, strlen(b_buf), 0, // send to server instead of broadcasting in the first place causing problems
            //  (const struct sockaddr*)&sock_adr,
            //     sizeof(sock_adr));
            int sch = send(serverFD, b_buf, strlen(b_buf), 0);
            if(sch <= 0)
            {
                perror("send problem");
            }
        }
        else if(FD_ISSET(sock, &temp)) // broadcast triggered
        {
            int rch = recvfrom(sock, b_buf, BUFFER_SIZE, 0,
            (struct sockaddr*) &sock_adr, (socklen_t*)&sock_adr_len);
            if(rch <= 0)
            {
                perror("recv error");
            }
            write(1, b_buf, strlen(b_buf));
        }
        else
        {
            perror("uknown desc");
            return;
        }
    }
}   
