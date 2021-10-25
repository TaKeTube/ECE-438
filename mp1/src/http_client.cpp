/*
** http_client.cpp
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>

#include <arpa/inet.h>
#include <string>
#include <iostream>

#define MAXDATASIZE 1000            // max number of bytes we can get at once

using namespace std;

// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET)
    {
        return &(((struct sockaddr_in *)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6 *)sa)->sin6_addr);
}

int main(int argc, char **argv)
{
    int sockfd, numbytes;
    char buf[MAXDATASIZE];
    struct addrinfo hints, *servinfo, *p;
    int rv;
    char s[INET6_ADDRSTRLEN];
    string host, port, resource;

    if (argc != 2)
    {
        fprintf(stderr, "usage: client hostname\n");
        exit(1);
    }

    int _tmp;
    string url(argv[1]);
    if ((_tmp = url.find("://")) == string::npos)
        exit(1);
    url = url.substr(_tmp+3);
    if ((_tmp = url.find("/")) == string::npos)
        exit(1);
    if ((resource = url.substr(_tmp)).empty())
        exit(1);
    string host_plus_port;
    if ((host_plus_port = url.substr(0, _tmp)).empty())
        exit(1);
    if ((_tmp = host_plus_port.find(":")) == string::npos)
    {
        host = host_plus_port;
        port = string("80");
    }
    else
    {
        if ((host = host_plus_port.substr(0, _tmp)).empty())
            exit(1);
        if ((port = host_plus_port.substr(_tmp+1)).empty())
            exit(1);
    }
    // cout << host.data() << endl;
    // cout << port.data() << endl;
    // cout << resource.data() << endl;

    
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    if ((rv = getaddrinfo(host.data(), port.data(), &hints, &servinfo)) != 0)
    {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return 1;
    }

    // loop through all the results and connect to the first we can
    for (p = servinfo; p != NULL; p = p->ai_next)
    {
        if ((sockfd = socket(p->ai_family, p->ai_socktype,
                                p->ai_protocol)) == -1)
        {
            perror("client: socket");
            continue;
        }

        if (connect(sockfd, p->ai_addr, p->ai_addrlen) == -1)
        {
            close(sockfd);
            perror("client: connect");
            continue;
        }

        break;
    }

    if (p == NULL)
    {
        fprintf(stderr, "client: failed to connect\n");
        return 2;
    }

    inet_ntop(p->ai_family, get_in_addr((struct sockaddr *)p->ai_addr),
                s, sizeof s);
    printf("client: connecting to %s\n", s);

    freeaddrinfo(servinfo); // all done with this structure

    string request = "GET " + resource + " HTTP/1.1\r\n" + "User-Agent: Wget/1.12(linux-gnu)\r\n" + "Host: " + host + ":" + port + "\r\n" + "Connection: Keep-Alive\r\n\r\n";
    // cout << request.data() << endl;
    // cout << "####" << endl;
    if (send(sockfd, request.data(), request.size(), 0) == -1)
    {
        perror("send");
        exit(1);
    }
    FILE *fptr;
    fptr = fopen("output", "wb");

    memset(buf, '\0', MAXDATASIZE);
    if ((numbytes = recv(sockfd, buf, MAXDATASIZE-1, 0)) == -1)
    {
        perror("recv");
        exit(1);
    }
    if (numbytes > 0)
    {
        string _tmp(buf);
        // cout << _tmp.data() << endl;
        // cout << "##########" << endl;
        int idx = _tmp.find("\r\n\r\n");
        cout << _tmp.substr(0, idx) << endl;
        _tmp = _tmp.substr(idx+4);
        // cout << _tmp.data() << endl;
        if (! _tmp.empty())
            fwrite((char*)_tmp.data(), sizeof(char), _tmp.size(), fptr);
            // fputs((char*)_tmp.data(), fptr);
    }

    memset(buf, '\0', MAXDATASIZE);
    while ((numbytes = recv(sockfd, buf, MAXDATASIZE, 0)) > 0)
    {
        if (numbytes == -1)
        {
            perror("recv");
            exit(1);
        }
        // cout << buf << endl;
        // fputs(buf, fptr);
        fwrite(buf, sizeof(char), numbytes, fptr);
        memset(buf, '\0', MAXDATASIZE);
    }
    fclose(fptr);
    close(sockfd);

    return 0;
}
