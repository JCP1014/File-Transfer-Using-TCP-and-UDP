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
void tcp_client(char *ip, int port, char *fileName);
void udp_server(char *ip, int port, char *fileName);
void udp_client(char *ip, int port, char *fileName);
void send_message(int sock, char *message);
void recv_message(int sock);
void send_image(int sock, char *fileName);
void recv_image(int sock);
void error(const char *msg);

int main(int argc, char *argv[])
{
    char *ip;
    int port;
    char fileName[256];
    char message[1024];
    if (argc >= 5)
    {
        ip = argv[3];
        port = atoi(argv[4]);
        if (strcmp("send", argv[2]) == 0)
        {
            if (strstr(argv[5], ".") != NULL && (strlen(strstr(argv[5], ".")) == 4 || strlen((argv[5], ".")) == 5))
            {
                strcpy(fileName, argv[5]);
            }
            else
            {
                strcpy(message, argv[5]);
            }
        }

        if (strcmp("tcp", argv[1]) == 0)
        {
            if (strcmp("send", argv[2]) == 0)
            {
                tcp_server(ip, port, fileName);
            }
            else if (strcmp("recv", argv[2]) == 0)
            {
                tcp_client(ip, port, fileName);
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
                udp_client(ip, port, fileName);
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
        send_image(newsockfd,fileName);
        /*bzero(buffer, 256);
        n = recv(newsockfd, buffer, 255, 0);
        if (n < 0)
            error("ERROR reading from socket");
        printf("Here is the message: %s\n", buffer);
        n = write(newsockfd, "I got your message", 18);
        if (n < 0)
            error("ERROR writing to socket");
        */
        close(newsockfd);
    }
    close(sockfd);
}

void tcp_client(char *ip, int port, char *fileName)
{
    int sockfd, n;
    struct sockaddr_in serv_addr;
    struct hostent *server;
    char buffer[256];
    time_t timep;
    struct tm *p;
    time(&timep);
    p = localtime(&timep);

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
    recv_image(sockfd);
    /*
    printf("Please enter the message: ");
    bzero(buffer, 256);
    fgets(buffer, 255, stdin);
    n = write(sockfd, buffer, strlen(buffer));
    if (n < 0)
        error("ERROR writing to socket");
    bzero(buffer, 256);
    n = read(sockfd, buffer, 255);
    if (n < 0)
        error("ERROR reading from socket");
    printf("%s\n", buffer);
    */
    close(sockfd);
}

void udp_server(char *ip, int port, char *fileName)
{
    int sock;
    if ((sock = socket(PF_INET, SOCK_DGRAM, 0)) < 0)
        ERR_EXIT("socket error");

    struct sockaddr_in servaddr;
    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(port);
    servaddr.sin_addr.s_addr = inet_addr(ip);

    if (bind(sock, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0)
        ERR_EXIT("bind error");

    char recvbuf[1024] = {0};
    struct sockaddr_in peeraddr;
    socklen_t peerlen;
    int n;

    while (1)
    {

        peerlen = sizeof(peeraddr);
        memset(recvbuf, 0, sizeof(recvbuf));
        FILE *fp = fopen(fileName, "rb");
        if (fp == NULL)
        {
            printf("File open error\n");
            return;
        }
        while (1)
        {
            unsigned char buff[100] = {0};
            int nread = fread(buff, 1, 100, fp);
            if (nread > 0)
            {
                write(sock, buff, nread);
            }
            if (nread < 100)
            {
                if (feof(fp))
                {
                    printf("End of file\n");
                    printf("File transfer completed for id: %d\n", sock);
                }
                if (ferror(fp))
                {
                    printf("Error reading\n");
                }
                break;
            }
        }
        //n = recvfrom(sock, recvbuf, sizeof(recvbuf), 0,
        //             (struct sockaddr *)&peeraddr, &peerlen);
        //if (n == -1)
        //{

        //    if (errno == EINTR)
        //        continue;

        //    ERR_EXIT("recvfrom error");
        //}
        //else if (n > 0)
        //{

        /*send_image(sock,fileName);
            FILE *image;
            int size;
            image = fopen(fileName, "r");
            fseek(image, 0, SEEK_END);
            size = ftell(image);
            fseek(image, 0, SEEK_SET);

            // Send image size
            write(sock, &size, sizeof(size));

            // Send image as byte array
            char send_buffer[size];
            while (!feof(image))
            {
                fread(send_buffer, 1, sizeof(send_buffer), image);
                write(sock, send_buffer, sizeof(send_buffer));
                bzero(send_buffer, sizeof(send_buffer));
            }*/
        //fputs(recvbuf, stdout);
        //sendto(sock, recvbuf, n, 0,
        //     (struct sockaddr *)&peeraddr, peerlen);
        //}
    }
    close(sock);
}

void udp_client(char *ip, int port, char *fileName)
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
    char recvbuf[100] = {0};
    int bytesReceived = 0;
    FILE *fp;
    printf("Receiving file...\n");
    char filePath[200] = "./Received/";
    strcat(filePath, fileName);
    fp = fopen(filePath, "wb");
    if (fp == NULL)
    {
        printf("Error opening file\n");
        return;
    }
    long double sz = 1;
    while ((bytesReceived = read(sock, recvbuf, 100)) > 0)
    {
        ++sz;
        printf("Received: %Lf Mb\n", (sz / 100));
        fflush(stdout);
        fwrite(recvbuf, 1, bytesReceived, fp);
    }
    if (bytesReceived < 0)
    {
        printf("Read Error\n");
    }
    else
    {
        printf("File OK... Completed\n");
    }
    /*while (fgets(sendbuf, sizeof(sendbuf), stdin) != NULL)
    {

        //sendto(sock, sendbuf, strlen(sendbuf), 0, (struct sockaddr *)&servaddr, sizeof(servaddr));

        //ret = recvfrom(sock, recvbuf, sizeof(recvbuf), 0, NULL, NULL);
        /*recv_image(sock, fileName);
        int size;
        read(sock, &size, sizeof(int));
        printf("%d\n", size);

        // Read image byte array
        char p_array[size];
        read(sock, p_array, size);

        // Convert it back into picture
        FILE *image;
        char filePath[1024] = "./Received/";
        strcat(filePath, fileName);
        image = fopen(filePath, "w");
        fwrite(p_array, 1, sizeof(p_array), image);
        fclose(image);*/
    /*if (ret == -1)
        {
            if (errno == EINTR)
                continue;
            ERR_EXIT("recvfrom");
        }

        //fputs(recvbuf, stdout);
        memset(sendbuf, 0, sizeof(sendbuf));
        memset(recvbuf, 0, sizeof(recvbuf));
    }*/

    close(sock);
}

void send_message(int sock, char *message)
{
    int n = write(sock,message,18);
    if (n < 0) error("ERROR writing to socket");

}
void recv_message(int sock)
{
}
void send_image(int sock, char fileName[256])
{
    strcat(fileName,"\0");
    write(sock,fileName,256);
    printf("send fileName : %s\n",fileName);
    // Get image size
    FILE *image;
    int size;
    image = fopen(fileName, "rb");
    if(image==NULL)
    {
        printf("File open error\n");
        return;
    }
    fseek(image, 0, SEEK_END);
    size = ftell(image);
    fseek(image, 0, SEEK_SET);
    
    // Send image size
    write(sock, &size, sizeof(size));

        while(1)
    {
        unsigned char buff[1] = {0};
        int nread = fread(buff,1,1,image);
        if(nread>0)
        {
            write(sock,buff,nread);
        }
        if(nread < 1)
        {
            if(feof(image))
            {
                printf("End of file\n");
                printf("File transfer completed\n");
            }
            if(ferror(image))
            {
                printf("Error reading\n");
            }
            break;
        }
    }
    // Send image as byte array
    /*char send_buffer[size];
    while (!feof(image))
    {
        fread(send_buffer, 1, sizeof(send_buffer), image);
        write(sock, send_buffer, sizeof(send_buffer));
        bzero(send_buffer, sizeof(send_buffer));
    }*/
}

void recv_image(int sock)
{
    char fileName[256];
    read(sock,fileName,256);
    printf("%s\n",fileName);
    // Read image size
    int size;
    read(sock, &size, sizeof(int));
    printf("%d\n", size);

    // Read image byte array
    char p_array[size];
    read(sock, p_array, size);

    // Convert it back into picture
    FILE *image;
    char filePath[1024] = "./Received/";
    strcat(filePath, fileName);
    image = fopen(filePath, "wb");
    if(image==NULL)
    {
        printf("Error openning file.\n");
        return;
    }
    double sz = 0;
    int bytesReceived = 0;
    char recvBuff[1];
    while( (bytesReceived = read(sock,recvBuff,1)) > 0)
    {
        ++sz;
        printf("Received : %.2f%%\n",sz/size*100);
        fflush(stdout);
        fwrite(recvBuff,1,bytesReceived,image);
    }
    if(bytesReceived<0)
    {
        printf("Read Error\n");
    }
    printf("Completed\n");
    //fwrite(p_array, 1, sizeof(p_array), image);
    fclose(image);
}

void error(const char *msg)
{
    perror(msg);
    exit(1);
}