#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <ctype.h>
#include <time.h>
#include <errno.h>

#define ERR_EXIT(m)         \
    do                      \
    {                       \
        perror(m);          \
        exit(EXIT_FAILURE); \
    } while (0)

void tcp_server(char *ip, int port, char *fileName);
void tcp_client(char *ip, int port);
void udp_server(char *ip, int port, char *fileName);
void udp_client(char *ip, int port);
void send_message(int sock);
void recv_message(int sock);
void send_image(int sock, char *fileName);
void recv_image(int sock);
void error(const char *msg);

int main(int argc, char *argv[])
{
    char *ip;
    int port;
    char *fileName = NULL;

    if (argc >= 5) // Ensure there are enough input arguments
    {
        ip = argv[3];
        port = atoi(argv[4]);
        if (strcmp("send", argv[2]) == 0 && argc >= 6) // Want to send file, not message
        {
            fileName = argv[5];
        }

        if (strcmp("tcp", argv[1]) == 0)
        {
            if (strcmp("send", argv[2]) == 0) // TCP & send
            {
                tcp_server(ip, port, fileName);
            }
            else if (strcmp("recv", argv[2]) == 0) // TCP & recv
            {
                tcp_client(ip, port);
            }
            else
            {
                printf("input error\n");
            }
        }
        else if (strcmp("udp", argv[1]) == 0)
        {
            if (strcmp("send", argv[2]) == 0) // UDP & send
            {
                udp_server(ip, port, fileName);
            }
            else if (strcmp("recv", argv[2]) == 0) // UDP & recv
            {
                udp_client(ip, port);
            }
            else
            {
                printf("input error\n");
            }
        }
        else
        {
            printf("input error\n");
        }
    }
    else
    {
        printf("input error\n");
    }
}

void tcp_server(char *ip, int port, char *fileName)
{
    int sockfd, newsockfd;
    socklen_t clilen;
    char buffer[256];
    struct sockaddr_in serv_addr, cli_addr;
    int n;

    // Create the socket
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0)
        error("ERROR opening socket");

    bzero((char *)&serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = inet_addr(ip);      // Convert IP string to int
    serv_addr.sin_port = htons(port);               // Host to network short integer
    
    /* Avoid the bind error which says the address already in use */ 
    int on = 1;
    if((setsockopt(sockfd,SOL_SOCKET,SO_REUSEADDR,&on,sizeof(on)))<0)
        error("setsockopt failed");

    if (bind(sockfd, (struct sockaddr *)&serv_addr, // Bind the address to the socket
             sizeof(serv_addr)) < 0)
        error("ERROR on binding");
    listen(sockfd, 5); // Listen for at most five connections on a socket
    clilen = sizeof(cli_addr);
    while (1)
    {
        // Accept a connection on a socket
        newsockfd = accept(sockfd,
                           (struct sockaddr *)&cli_addr,
                           &clilen);
        if (newsockfd < 0)
            error("ERROR on accept");
        if (fileName == NULL) // Want to send message, not image
        {
            n = write(newsockfd, "msg", 3); // Tell client to receive a message
            if (n < 0)
                error("ERROR writing to socket");
            send_message(newsockfd);
        }
        else
        {
            n = write(newsockfd, "img", 3); // Tell client to receive a image file
            if (n < 0)
                error("ERROR writing to socket");
            send_image(newsockfd, fileName);
        }
        close(newsockfd); // Close socket
    }
    close(sockfd);
}

void tcp_client(char *ip, int port)
{
    int sockfd, n;
    struct sockaddr_in serv_addr;
    struct hostent *server;
    char buffer[256];

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0)
        error("ERROR opening socket");
    server = gethostbyname(ip); // Get network host entry
    if (server == NULL)
    {
        fprintf(stderr, "ERROR, no such host\n");
        exit(0);
    }
    bzero((char *)&serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    bcopy((char *)server->h_addr,
          (char *)&serv_addr.sin_addr.s_addr,
          server->h_length);
    serv_addr.sin_port = htons(port);
    if (connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) // Initiate a connection on a socket
        error("ERROR connecting");

    n = read(sockfd, buffer, 3); // Read that server will send message or image file
    if (n < 0)
        error("ERROR reading from socket");
    if (strcmp(buffer, "msg") == 0) // Server will send a message
    {
        recv_message(sockfd);
    }
    else if (strcmp(buffer, "img") == 0) // Server will send a image file
    {
        recv_image(sockfd);
    }
    bzero(buffer, 256);
    close(sockfd);
}

void udp_server(char *ip, int port, char fileName[256])
{
    int sock;
    if ((sock = socket(PF_INET, SOCK_DGRAM, 0)) < 0)
        ERR_EXIT("socket error");

    struct sockaddr_in servaddr;
    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(port);
    servaddr.sin_addr.s_addr = inet_addr(ip);
    
    /* Avoid the bind error which says the address already in use */ 
    int on = 1;
    if((setsockopt(sock,SOL_SOCKET,SO_REUSEADDR,&on,sizeof(on)))<0)
        error("setsockopt failed");

    if (bind(sock, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0)
        ERR_EXIT("bind error");

    char send_buf[1024] = {0};
    char recv_buf[1024] = {0};
    struct sockaddr_in peeraddr;
    socklen_t peerlen = sizeof(peeraddr);
    int n;

    while (1)
    {
        // Wait for client sending "connected"
        n = recvfrom(sock, recv_buf, sizeof(recv_buf), 0,
                     (struct sockaddr *)&peeraddr, &peerlen);
        if (n == -1) // Fail
        {
            if (errno == EINTR || errno == EAGAIN)
                continue;
            ERR_EXIT("recvfrom error");
        }
        else if (n > 0) // Success
        {
            if (fileName == NULL)
            {
                // Tell client to receive message
                if (sendto(sock, "msg", 3, 0,
                           (struct sockaddr *)&peeraddr, peerlen) < 0)
                    perror("ERROR sending to client");

                char message[1024];                       // buffer for storing the message that user types
                int words = 0;                            // number of words in the message
                bzero(message, 1024);                     // Clear the buffer
                printf("Please enter the message: ");     // Let user type message
                fgets(message, 255, stdin);               // Reading input into the message buffer
                if (message[strlen(message) - 1] == '\n') // Ignore the newline character
                    message[strlen(message) - 1] = '\0';
                words = strlen(message); // Get how many words are in the message
                // Tell the client the total number of the words to be sent
                if (sendto(sock, &words, sizeof(words), 0,
                           (struct sockaddr *)&peeraddr, peerlen) < 0)
                    error("ERROR sending to client");

                int index = 0; // position of the word in the array buffer to be sent
                char send_buf[1];
                /* Send message to the client */
                while (index < words)
                {
                    send_buf[0] = message[index];
                    if (sendto(sock, send_buf, sizeof(send_buf), 0,
                               (struct sockaddr *)&peeraddr, peerlen) < 0)
                        error("ERROR sending to client");
                    bzero(recv_buf, sizeof(recv_buf)); // Clear the buffer
                    // Wait for the client sending ACK to ensure the packet is not lost
                    recvfrom(sock, recv_buf, sizeof(recv_buf), 0,
                             (struct sockaddr *)&peeraddr, &peerlen);
                    /* If client didn't send ACK, resend this packet */
                    while (strcmp(recv_buf, "ACK") != 0)
                    {
                        sendto(sock, send_buf, sizeof(send_buf), 0, // Resend the packet
                               (struct sockaddr *)&peeraddr, peerlen);
                        bzero(recv_buf, sizeof(recv_buf));
                        recvfrom(sock, recv_buf, sizeof(recv_buf), 0, // Wait for ACK
                                 (struct sockaddr *)&peeraddr, &peerlen);
                    }
                    ++index; // Move to the next word
                }
                printf("Completed\n");
            }
            else
            {
                // Tell client to receive a image file
                if (sendto(sock, "img", 3, 0,
                           (struct sockaddr *)&peeraddr, peerlen) < 0)
                    perror("ERROR sending to client");

                strcat(fileName, "\0"); // Concatenate the null character to avoid sending wrong file name
                // Tell the client the file name
                if (sendto(sock, fileName, strlen(fileName), 0,
                           (struct sockaddr *)&peeraddr, peerlen) < 0)
                    error("ERROR sending to client");

                /* Get image size */
                FILE *image;                   // file descriptor
                unsigned long long size;       // file size
                image = fopen(fileName, "rb"); // Open the file to read
                if (image == NULL)             // Fail to open the file
                {
                    printf("File open error\n");
                    return;
                }
                fseek(image, 0, SEEK_END); // Set the position indicator to the end of the file
                size = ftell(image);       // Get the number of bytes from the begining of the file to the position indicator of the stream
                fseek(image, 0, SEEK_SET); // Reset the position indicator to the begining of the file for reading the file later

                // Tell the client the size of image
                if (sendto(sock, &size, sizeof(size), 0,
                           (struct sockaddr *)&peeraddr, peerlen) < 0)
                    error("ERROR sending to client");

                char send_buf[1];
                int count = 1; // Count how many bytes have been read
                /* Read the file and send to the client */
                while (!feof(image))
                {
                    if (fread(send_buf, 1, 1, image) == 0) // Fail to read the file
                    {
                        if (count > size) // File has been read completely, then stop reading and sending
                            break;
                        else
                            error("fread error"); // The file hasn't been read completely but error occours, print the reason
                    }
                    // Send  the packet to the client
                    if (sendto(sock, send_buf, sizeof(send_buf), 0,
                               (struct sockaddr *)&peeraddr, peerlen) < 0)
                        error("ERROR sending to client");
                    bzero(recv_buf, sizeof(recv_buf)); // Clear buffer
                    // Wait for the client sending ACK to ensure the packet is not lost
                    recvfrom(sock, recv_buf, sizeof(recv_buf), 0,
                             (struct sockaddr *)&peeraddr, &peerlen);
                    /* If client didn't send ACK, resend this packet */
                    while (strcmp(recv_buf, "ACK") != 0)
                    {
                        sendto(sock, send_buf, sizeof(send_buf), 0,  // Resend the packet
                               (struct sockaddr *)&peeraddr, peerlen);
                        bzero(recv_buf, sizeof(recv_buf));
                        n = recvfrom(sock, recv_buf, sizeof(recv_buf), 0,   // Wait for ACK
                                     (struct sockaddr *)&peeraddr, &peerlen);
                    }
                    ++count;
                }
                // Complete transfering the file
                printf("Completed\n");
            }
        }
    }
    close(sock);    // Close the socket
}

void udp_client(char *ip, int port)
{
    int sock;   
    if ((sock = socket(PF_INET, SOCK_DGRAM, 0)) < 0)
        ERR_EXIT("socket");

    struct sockaddr_in servaddr;
    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(port);
    servaddr.sin_addr.s_addr = inet_addr(ip);

    int ret;
    char sendbuf[1024] = {0};
    char recvbuf[1024] = {0};
    bzero(recvbuf, 1024);
    int addr_len = sizeof(servaddr);
    // Tell the server that client is ready to receive packets
    ret = sendto(sock, "Connected", 9, 0, (struct sockaddr *)&servaddr, addr_len);
    if (ret < 0)
        error("sendto error"); 
    // Receive "msg" or "img" from the server to distinguish it will send a message or an image file
    ret = recvfrom(sock, recvbuf, sizeof(recvbuf), 0, (struct sockaddr *)&servaddr, &addr_len);
    if (ret < 0)
        error("recv error");
    if (strcmp(recvbuf, "msg") == 0)    // Server will send a message
    {
        memset(recvbuf, 0, sizeof(recvbuf));    // Clear the buffer
        // Get the number of words in the message 
        int words = 0; 
        ret = recvfrom(sock, &words, sizeof(words), 0, (struct sockaddr *)&servaddr, &addr_len);
        // Initialize an array buffer to store the message 
        char message[words];
        bzero(message, words);

        double len = 0; // how many words have been received
        int bytesReceived = 0;  // how many bytes are received from the socket
        char recv_buf[2];
        long double progress = 0;   // transfer progress rate
        int record = 0; // progress rate of last log
        bzero(recv_buf, 2); 
        /* Receive packets until words received is as many as total number of words */
        while (len < words)
        {
            bytesReceived = recvfrom(sock, recv_buf, 1, 0, (struct sockaddr *)&servaddr, &addr_len);    // Receive the packet
            if (bytesReceived < 1)  // Didn't receive a byte from the socket
            {
                sendto(sock, "LOST", 4, 0, (struct sockaddr *)&servaddr, addr_len); // Tell the server the packet is lost
                continue;   // Wait for a packet again
            }
            sendto(sock, "ACK", 3, 0, (struct sockaddr *)&servaddr, addr_len);  // Tell the server the packet is received successfully and it can send next one
            len += bytesReceived;   // Count the words received
            recv_buf[1] = '\0'; // Concatenate the null character into the receive buffer to avoid error
            strncat(message, recv_buf, 1);  // Concatenate the character received to the message array to compose the whole message
            progress = len / words * 100;   // Calculate the transfer progress rate
            /* Print the log every 5% */
            while (progress - record >= 5)
            {
                static time_t timep;
                static struct tm *p;
                time(&timep);
                p = localtime(&timep);  // Convert to local time
                // Print the progress rate and current time
                printf("%d%% %d/%d/%d %d:%d:%d\n", record + 5, p->tm_year + 1900, p->tm_mon + 1, p->tm_mday, p->tm_hour, p->tm_min, p->tm_sec);
                record += 5;    // Next log will be printed after the rate increases 5%
            }
            bzero(recv_buf, 2); // Clear buffer
        }
        printf("Message : %s\n", message);  // Print the whole message sent from the server
    }
    else if (strcmp(recvbuf, "img") == 0)   // Server will send a image file
    {
        memset(recvbuf, 0, sizeof(recvbuf));    // Clear the buffer
        char fileName[256];
        bzero(fileName, 256);
        // Get the file name from the server
        ret = recvfrom(sock, fileName, sizeof(fileName), 0, (struct sockaddr *)&servaddr, &addr_len);
        // Get the size of the image file from the server
        unsigned long long size;
        ret = recvfrom(sock, &size, sizeof(size), 0, (struct sockaddr *)&servaddr, &addr_len);

        FILE *image;
        char filePath[1024] = "./Received/";    // Save the file in another directory named "Received"
        strcat(filePath, fileName); // Concatenate the file name to the directory name to compose the complete path
        image = fopen(filePath, "wb");  // Open the file to write
        if (image == NULL)
        {
            printf("Error openning file.\n");
            return;
        }

        long double sz = 0; // how many bytes have been received
        int bytesReceived = 0;
        char recv_buf[2];
        long double progress = 0;
        int record = 0;
        bzero(recv_buf, 2);
        /* Receive packets until size received is as many as total size of the file */
        while (sz < size)
        {
            bytesReceived = recvfrom(sock, recv_buf, 1, 0, (struct sockaddr *)&servaddr, &addr_len);
            if (bytesReceived < 1)  // Didn't receive a byte from the socket
            {
                sendto(sock, "LOST", 4, 0, (struct sockaddr *)&servaddr, addr_len); // Tell the server the packet is lost
                continue;    // Wait for a packet again
            }
            ret = sendto(sock, "ACK", 3, 0, (struct sockaddr *)&servaddr, addr_len);    // Tell the server the packet is received successfully and it can send next one
            sz += bytesReceived;    // Count the words received
            recv_buf[1] = '\0'; // Concatenate the null character into the receive buffer to avoid error
            progress = sz / size * 100; // Calculate the transfer progress rate
            /* Print the log every 5% */
            while (progress - record >= 5)
            {
                static time_t timep;
                static struct tm *p;
                time(&timep);
                p = localtime(&timep);
                printf("%d%% %d/%d/%d %d:%d:%d\n", record + 5, p->tm_year + 1900, p->tm_mon + 1, p->tm_mday, p->tm_hour, p->tm_min, p->tm_sec);
                record += 5;
            }
            fflush(stdout);
            fwrite(recv_buf, 1, bytesReceived, image);  // Write the data received into the file opened
            bzero(recv_buf, 2); // Clear the buffer
        }
        // Write file completely, close the file
        fclose(image);  
    }
    // Task is completed, close the socket
    close(sock);
}

void send_message(int sock)
{
    char message[1024];
    int words = 0;
    bzero(message, 1024);
    printf("Please enter the message: ");
    fgets(message, 255, stdin);
    if (message[strlen(message) - 1] == '\n')
        message[strlen(message) - 1] = '\0';
    words = strlen(message);
    int n = write(sock, &words, sizeof(words));
    if (n < 0)
        error("ERROR writing to socket");

    int index = 0;
    char send_buf[1];
    while (index < words)
    {
        send_buf[0] = message[index];
        write(sock, send_buf, 1);
        if (n < 0)
            error("ERROR writing to socket");
        ++index;
    }
    printf("Completed\n");
}
void recv_message(int sock)
{

    int words = 0;
    int n = read(sock, &words, sizeof(words));
    if (n < 0)
        error("ERROR reading from socket");
    char message[words];
    bzero(message, words);

    double len = 0;
    int bytesReceived = 0;
    char recv_buf[2];
    long double progress = 0;
    int record = 0;
    bzero(recv_buf, 2);
    while ((bytesReceived = read(sock, recv_buf, 1)) > 0)
    {
        len += bytesReceived;
        recv_buf[1] = '\0';
        strncat(message, recv_buf, 1);
        progress = len / words * 100;
        while (progress - record >= 5)
        {
            static time_t timep;
            static struct tm *p;
            time(&timep);
            p = localtime(&timep);
            printf("%d%% %d/%d/%d %d:%d:%d\n", record + 5, p->tm_year + 1900, p->tm_mon + 1, p->tm_mday, p->tm_hour, p->tm_min, p->tm_sec);
            record += 5;
        }
        //printf("%s", recv_buf);
        bzero(recv_buf, 2);
    }
    if (bytesReceived < 0)
    {
        printf("Read Error\n");
    }
    printf("Message : %s\n", message);
}
void send_image(int sock, char *fileName)
{
    strcat(fileName, "\0");
    write(sock, fileName, 256);
    // Get image size
    FILE *image;
    unsigned long long size;
    image = fopen(fileName, "rb");
    if (image == NULL)
    {
        printf("File open error\n");
        return;
    }
    fseek(image, 0, SEEK_END);
    size = ftell(image);
    fseek(image, 0, SEEK_SET);

    // Send image size
    write(sock, &size, sizeof(size));

    char send_buf[1];
    while (!feof(image))
    {
        fread(send_buf, 1, 1, image);
        write(sock, send_buf, sizeof(send_buf));
        bzero(send_buf, sizeof(send_buf));
    }
    printf("Completed\n");
}

void recv_image(int sock)
{

    char fileName[256];
    read(sock, fileName, 256);
    // Read image size
    unsigned long long size;
    read(sock, &size, sizeof(size));

    // Convert it back into picture
    FILE *image;
    char filePath[1024] = "./Received/";
    strcat(filePath, fileName);
    image = fopen(filePath, "wb");
    if (image == NULL)
    {
        printf("Error openning file.\n");
        return;
    }

    long double sz = 0;
    int bytesReceived = 0;
    char recv_buf[2];
    long double progress = 0;
    int record = 0;
    while ((bytesReceived = read(sock, recv_buf, 1)) > 0)
    {
        sz += bytesReceived;
        recv_buf[1] = '\0';
        progress = sz / size * 100;
        while (progress - record >= 5)
        {
            static time_t timep;
            static struct tm *p;
            time(&timep);
            p = localtime(&timep);
            printf("%d%% %d/%d/%d %d:%d:%d\n", record + 5, p->tm_year + 1900, p->tm_mon + 1, p->tm_mday, p->tm_hour, p->tm_min, p->tm_sec);
            record += 5;
        }
        fflush(stdout);
        fwrite(recv_buf, 1, bytesReceived, image);
    }
    if (bytesReceived < 0)
    {
        printf("Read Error\n");
    }
    fclose(image);
}

void error(const char *msg)
{
    perror(msg);    // Print the reason of error
    exit(1);    // Abnormal termination
}