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

    if (argc >= 5)
    {
        ip = argv[3];
        port = atoi(argv[4]);
        if (strcmp("send", argv[2]) == 0 && argc >= 6)
        {
            fileName = argv[5];
        }

        if (strcmp("tcp", argv[1]) == 0)
        {
            if (strcmp("send", argv[2]) == 0)
            {
                tcp_server(ip, port, fileName);
            }
            else if (strcmp("recv", argv[2]) == 0)
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
            if (strcmp("send", argv[2]) == 0)
            {
                udp_server(ip, port, fileName);
            }
            else if (strcmp("recv", argv[2]) == 0)
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

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0)
        error("ERROR opening socket");
    bzero((char *)&serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = inet_addr(ip);
    serv_addr.sin_port = htons(port);
    if (bind(sockfd, (struct sockaddr *)&serv_addr,
             sizeof(serv_addr)) < 0)
        error("ERROR on binding");
    listen(sockfd, 5);
    clilen = sizeof(cli_addr);
    while (1)
    {
        newsockfd = accept(sockfd,
                           (struct sockaddr *)&cli_addr,
                           &clilen);
        if (newsockfd < 0)
            error("ERROR on accept");
        if (fileName == NULL)
        {
            n = write(newsockfd, "msg", 3);
            if (n < 0)
                error("ERROR writing to socket");
            send_message(newsockfd);
        }
        else
        {
            n = write(newsockfd, "img", 3);
            if (n < 0)
                error("ERROR writing to socket");
            send_image(newsockfd, fileName);
        }
        close(newsockfd);
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
    server = gethostbyname(ip);
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
    if (connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
        error("ERROR connecting");

    n = read(sockfd, buffer, 3);
    if (n < 0)
        error("ERROR reading from socket");
    if (strcmp(buffer, "msg") == 0)
    {
        recv_message(sockfd);
    }
    else if (strcmp(buffer, "img") == 0)
    {
        recv_image(sockfd);
    }
    bzero(buffer, 256);
    close(sockfd);
}

void udp_server(char *ip, int port, char fileName[256])
{
    int sock;
    const struct timeval sock_timeout = {.tv_sec = 1, .tv_usec = 0};
    if ((sock = socket(PF_INET, SOCK_DGRAM, 0)) < 0)
        ERR_EXIT("socket error");

    struct sockaddr_in servaddr;
    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(port);
    servaddr.sin_addr.s_addr = inet_addr(ip);

    if (bind(sock, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0)
        ERR_EXIT("bind error");

    char send_buf[1024] = {0};
    char recv_buf[1024] = {0};
    struct sockaddr_in peeraddr;
    socklen_t peerlen = sizeof(peeraddr);
    int n;

    while (1)
    {
        n = recvfrom(sock, recv_buf, sizeof(recv_buf), 0,
                     (struct sockaddr *)&peeraddr, &peerlen);
        if (n == -1)
        {
            if (errno == EINTR || errno == EAGAIN)
                continue;
            ERR_EXIT("recvfrom error");
        }
        else if (n > 0)
        {
            if (fileName == NULL)
            {
                if (sendto(sock, "msg", 3, 0,
                           (struct sockaddr *)&peeraddr, peerlen) < 0)
                    perror("ERROR sending to client");

                char message[1024];
                int words = 0;
                bzero(message, 1024);
                printf("Please enter the message: ");
                fgets(message, 255, stdin);
                if (message[strlen(message) - 1] == '\n')
                    message[strlen(message) - 1] = '\0';
                words = strlen(message);
                if (sendto(sock, &words, sizeof(words), 0,
                           (struct sockaddr *)&peeraddr, peerlen) < 0)
                    error("ERROR sending to client");

                int index = 0;
                char send_buf[1];
                setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (char *)&sock_timeout, sizeof(sock_timeout));
                while (index < words)
                {
                    send_buf[0] = message[index];
                    if (sendto(sock, send_buf, sizeof(send_buf), 0,
                               (struct sockaddr *)&peeraddr, peerlen) < 0)
                        error("ERROR sending to client");
                    bzero(recv_buf, sizeof(recv_buf));
                    recvfrom(sock, recv_buf, sizeof(recv_buf), 0,
                             (struct sockaddr *)&peeraddr, &peerlen);
                    //printf("%s\n", recv_buf);
                    while (strcmp(recv_buf, "ACK") != 0)
                    {
                        sendto(sock, send_buf, sizeof(send_buf), 0,
                               (struct sockaddr *)&peeraddr, peerlen);
                        bzero(recv_buf, sizeof(recv_buf));
                        recvfrom(sock, recv_buf, sizeof(recv_buf), 0,
                                 (struct sockaddr *)&peeraddr, &peerlen);
                    }
                    ++index;
                }
                printf("Completed\n");
            }
            else
            {
                if (sendto(sock, "img", 3, 0,
                           (struct sockaddr *)&peeraddr, peerlen) < 0)
                    perror("ERROR sending to client");

                strcat(fileName, "\0");
                if (sendto(sock, fileName, strlen(fileName), 0,
                           (struct sockaddr *)&peeraddr, peerlen) < 0)
                    error("ERROR sending to client");
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
                if (sendto(sock, &size, sizeof(size), 0,
                           (struct sockaddr *)&peeraddr, peerlen) < 0)
                    error("ERROR sending to client");

                char send_buf[1];
                //int count = 1;
                //setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (char *)&sock_timeout, sizeof(sock_timeout));
                while (!feof(image))
                {
                    //printf("%d\n", count);
                    if(fread(send_buf, 1, 1, image)==0)
                        break;
                    if (sendto(sock, send_buf, sizeof(send_buf), 0,
                               (struct sockaddr *)&peeraddr, peerlen) < 0)
                        error("ERROR sending to client");
                    bzero(recv_buf, sizeof(recv_buf));
                    int n = recvfrom(sock, recv_buf, sizeof(recv_buf), 0,
                                     (struct sockaddr *)&peeraddr, &peerlen);
                    if (n < 0)
                        error("289");
                    while (strcmp(recv_buf, "ACK") != 0)
                    {
                        n = sendto(sock, send_buf, sizeof(send_buf), 0,
                                   (struct sockaddr *)&peeraddr, peerlen);
                        if (n < 0)
                            error("293");
                        bzero(recv_buf, sizeof(recv_buf));
                        n = recvfrom(sock, recv_buf, sizeof(recv_buf), 0,
                                     (struct sockaddr *)&peeraddr, &peerlen);
                        if (n < 0)
                            error("298");
                    }
                    //printf("%d\n", count);
                    //++count;
                }
                printf("Completed\n");
            }
        }
    }
    close(sock);
}

void udp_client(char *ip, int port)
{
    const struct timeval sock_timeout = {.tv_sec = 1, .tv_usec = 0};
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
    ret = sendto(sock, "Connected", 9, 0, (struct sockaddr *)&servaddr, addr_len);
    if (ret < 0)
        error("sendto error");
    ret = recvfrom(sock, recvbuf, sizeof(recvbuf), 0, (struct sockaddr *)&servaddr, &addr_len);
    if (ret < 0)
        error("recv error");
    /*if (ret == -1)
    {
        if (errno != EINTR && errno != EAGAIN)
            ERR_EXIT("recvfrom");
    }*/
    if (strcmp(recvbuf, "msg") == 0)
    {
        memset(recvbuf, 0, sizeof(recvbuf));
        int words = 0;
        ret = recvfrom(sock, &words, sizeof(words), 0, (struct sockaddr *)&servaddr, &addr_len);
        /*if (ret == -1)
        {
            if (errno != EINTR && errno != EAGAIN)
                ERR_EXIT("recvfrom");
        }*/
        char message[words];
        bzero(message, words);

        double len = 0;
        int bytesReceived = 0;
        char recv_buf[2];
        long double progress = 0;
        int record = 0;
        bzero(recv_buf, 2);
        //setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (char *)&sock_timeout, sizeof(sock_timeout));
        while (len < words)
        {
            bytesReceived = recvfrom(sock, recv_buf, 1, 0, (struct sockaddr *)&servaddr, &addr_len);
            if (bytesReceived < 1)
            {
                sendto(sock, "LOSS", 4, 0, (struct sockaddr *)&servaddr, addr_len);
                continue;
            }
            sendto(sock, "ACK", 3, 0, (struct sockaddr *)&servaddr, addr_len);
            len += bytesReceived;
            recv_buf[1] = '\0';
            strncat(message, recv_buf, 1);
            progress = len / words * 100;
            if (progress - record >= 5)
            {
                static time_t timep;
                static struct tm *p;
                time(&timep);
                p = localtime(&timep);
                printf("%.0Lf%% %d/%d/%d %d:%d:%d\n", progress, p->tm_year + 1900, p->tm_mon + 1, p->tm_mday, p->tm_hour, p->tm_min, p->tm_sec);
                record += 5;
            }
            bzero(recv_buf, 2);
        }
        printf("message : %s\n", message);
    }
    else if (strcmp(recvbuf, "img") == 0)
    {
        memset(recvbuf, 0, sizeof(recvbuf));

        char fileName[256];
        bzero(fileName, 256);
        ret = recvfrom(sock, fileName, sizeof(fileName), 0, (struct sockaddr *)&servaddr, &addr_len);
        if (ret < 0)
            error("396");
        /*if (ret == -1)
        {
            if (errno != EINTR && errno != EAGAIN)
                ERR_EXIT("recvfrom");
        }*/
        //printf("%s\n", fileName);
        // Read image size
        unsigned long long size;
        ret = recvfrom(sock, &size, sizeof(size), 0, (struct sockaddr *)&servaddr, &addr_len);
        if (ret < 0)
            error("407");
        /*if (ret == -1)
        {
            if (errno != EINTR && errno != EAGAIN)
                ERR_EXIT("recvfrom");
        }*/
        //printf("%lld\n", size);

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
        bzero(recv_buf, 2);
        //setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (char *)&sock_timeout, sizeof(sock_timeout));
        while (sz < size)
        {
            bytesReceived = recvfrom(sock, recv_buf, 1, 0, (struct sockaddr *)&servaddr, &addr_len);
            if (bytesReceived < 1)
            {
                sendto(sock, "LOSS", 4, 0, (struct sockaddr *)&servaddr, addr_len);
                continue;
            }
            ret = sendto(sock, "ACK", 3, 0, (struct sockaddr *)&servaddr, addr_len);
            if (ret < 0)
                error("443");
            sz += bytesReceived;
            recv_buf[1] = '\0';
            progress = sz / size * 100;
            if (progress - record >= 5)
            {
                static time_t timep;
                static struct tm *p;
                time(&timep);
                p = localtime(&timep);
                printf("%.0Lf%% %d/%d/%d %d:%d:%d\n", progress, p->tm_year + 1900, p->tm_mon + 1, p->tm_mday, p->tm_hour, p->tm_min, p->tm_sec);
                record += 5;
            }
            fflush(stdout);
            fwrite(recv_buf, 1, bytesReceived, image);
            bzero(recv_buf, 2);
        }
        fclose(image);
    }
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
        if (progress - record >= 5)
        {
            static time_t timep;
            static struct tm *p;
            time(&timep);
            p = localtime(&timep);
            printf("%.0Lf%% %d/%d/%d %d:%d:%d\n", progress, p->tm_year + 1900, p->tm_mon + 1, p->tm_mday, p->tm_hour, p->tm_min, p->tm_sec);
            record += 5;
        }
        //printf("%s", recv_buf);
        bzero(recv_buf, 2);
    }
    if (bytesReceived < 0)
    {
        printf("Read Error\n");
    }
    printf("message : %s\n", message);
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
    //printf("%s\n", fileName);
    // Read image size
    unsigned long long size;
    read(sock, &size, sizeof(size));
    //printf("%lld\n", size);

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
        if (progress - record >= 5)
        {
            static time_t timep;
            static struct tm *p;
            time(&timep);
            p = localtime(&timep);
            printf("%.0Lf%% %d/%d/%d %d:%d:%d\n", progress, p->tm_year + 1900, p->tm_mon + 1, p->tm_mday, p->tm_hour, p->tm_min, p->tm_sec);
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
    perror(msg);
    exit(1);
}