/* 
 * udpserver.c - A simple UDP echo server 
 * usage: udpserver <port>
 */

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <netdb.h>
#include <dirent.h>
#include <sys/types.h> 
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define BUFSIZE 1024

/*
 * error - wrapper for perror
 */
void error(char *msg) 
{
  perror(msg);
  exit(1);
}

int main(int argc, char **argv) 
{
  int sockfd; /* socket */
  int portno; /* port to listen on */
  int clientlen; /* byte size of client's address */
  struct sockaddr_in serveraddr; /* server's addr */
  struct sockaddr_in clientaddr; /* client addr */
  struct hostent *hostp; /* client host info */
  char buf[BUFSIZE]; /* message buf */
  char newmsg[BUFSIZE];
  char *hostaddrp; /* dotted decimal host addr string */
  int optval; /* flag value for setsockopt */
  int n; /* message byte size */
  DIR *currdir;
  struct dirent *dirptr;

  /* 
   * check command line arguments 
   */

  if (argc != 2) 
  {
    fprintf(stderr, "usage: %s <port>\n", argv[0]);
    exit(1);
  }

  portno = atoi(argv[1]);

  /* 
   * socket: create the parent socket 
   */

  sockfd = socket(AF_INET, SOCK_DGRAM, 0);
  if (sockfd < 0) {error("ERROR opening socket");}

  /* setsockopt: Handy debugging trick that lets 
   * us rerun the server immediately after we kill it; 
   * otherwise we have to wait about 20 secs. 
   * Eliminates "ERROR on binding: Address already in use" error. 
   */
  optval = 1;
  setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, (const void *)&optval , sizeof(int));

  /*
   * build the server's Internet address
   */

  bzero((char *) &serveraddr, sizeof(serveraddr));
  serveraddr.sin_family = AF_INET;
  serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);  //converts to (long) network byte order
  serveraddr.sin_port = htons((unsigned short)portno);  //converts to (short) network byte order

  /* 
   * bind: associate the parent socket with a port 
   */

  if (bind(sockfd, (struct sockaddr *) &serveraddr, sizeof(serveraddr)) < 0) {error("ERROR on binding");}

  /* 
   * main loop: wait for a datagram, then echo it
   */

  clientlen = sizeof(clientaddr);

  while (1) 
  {

    /*
     * recvfrom: receive a UDP datagram from a client
     */

    bzero(buf, BUFSIZE);

    n = recvfrom(sockfd, buf, BUFSIZE, 0, (struct sockaddr *) &clientaddr, &clientlen);

    if (n < 0) {error("ERROR in recvfrom");}

    /* 
     * gethostbyaddr: determine who sent the datagram
     */

    hostp = gethostbyaddr((const char *)&clientaddr.sin_addr.s_addr, sizeof(clientaddr.sin_addr.s_addr), AF_INET);

    if (hostp == NULL) {error("ERROR on gethostbyaddr");}

    hostaddrp = inet_ntoa(clientaddr.sin_addr);

    if (hostaddrp == NULL) {error("ERROR on inet_ntoa\n");}

    printf("server received datagram from %s (%s)\n", hostp->h_name, hostaddrp);

    printf("server received %d/%d bytes: %s\n", (int)strlen(buf), n, buf);

    if (strcmp(buf, "ls\n") == 0)
    {
      bzero(buf, BUFSIZE);
      if ((currdir = opendir(".")) == NULL)
      {
        error("couldn't open current directory\n");
        strcpy(buf, "error: ls failed\n");
      }
      else
      {
        while((dirptr = readdir(currdir)) != NULL)
        {
          if (dirptr->d_type == DT_REG)
          {
            strcat(buf, dirptr->d_name);
            strcat(buf, "\n");
          }
        }
        closedir(currdir);
      }
    }

    else if (strcmp(buf, "exit\n") == 0)
    {
      bzero(buf, BUFSIZE);
      strcpy(buf, "Goodbye!\n");
    }

    else
    {
      char errbuf[strlen(buf)];
      bzero(errbuf, strlen(buf));
      strcpy(errbuf, buf);
      errbuf[strlen(errbuf)-1] = '\0';
      bzero(buf, BUFSIZE);
      sprintf(buf, "error: command '%s' unknown\n", errbuf);
    }

    /* 
     * sendto: echo the input back to the client 
     */

    n = sendto(sockfd, buf, (int)strlen(buf), 0, (struct sockaddr *) &clientaddr, clientlen);

    if (n < 0) {error("ERROR in sendto");}
  }
}
