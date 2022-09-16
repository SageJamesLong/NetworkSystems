/* 
 * udpclient.c - A simple UDP client
 * usage: udpclient <host> <port>
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h> 

#define BUFSIZE 1024

/* 
 * error - wrapper for perror
 */
void error(char *msg) {
  perror(msg);
  exit(0);
}

int main(int argc, char **argv) 
{
  int sockfd, portno, n;
  int serverlen;
  struct sockaddr_in serveraddr;
  struct hostent *server;
  char *hostname;
  char buf[BUFSIZE];
  char temp[BUFSIZE];

  /* check command line arguments */
  if (argc != 3) {
    fprintf(stderr,"usage: %s <hostname> <port>\n", argv[0]);
    exit(0);
  }
  hostname = argv[1];
  portno = atoi(argv[2]);

  /* socket: create the socket */
  sockfd = socket(AF_INET, SOCK_DGRAM, 0);
  if (sockfd < 0) 
    error("ERROR opening socket");

  /* gethostbyname: get the server's DNS entry */
  server = gethostbyname(hostname);
  if (server == NULL) {
    fprintf(stderr,"ERROR, no such host as %s\n", hostname);
    exit(0);
  }

  /* build the server's Internet address */
  bzero((char *) &serveraddr, sizeof(serveraddr));
  serveraddr.sin_family = AF_INET;
  bcopy((char *)server->h_addr, (char *)&serveraddr.sin_addr.s_addr, server->h_length);
  serveraddr.sin_port = htons(portno); //htons converts to (short) network byte order

  /* get a message from the user */
  bzero(buf, BUFSIZE);
  printf("Please enter msg: ");
  fgets(buf, BUFSIZE, stdin);
  
  serverlen = sizeof(serveraddr);

  while(1){

    strcpy(temp, buf);

    /* send the message to the server */
    n = sendto(sockfd, buf, strlen(buf), 0, (struct sockaddr *)&serveraddr, serverlen);

    if (n < 0) {error("ERROR in sendto");}
    
    /* print the server's reply */
    printf("\n");
    n = recvfrom(sockfd, buf, BUFSIZE, 0, (struct sockaddr *)&serveraddr, &serverlen);

    if (n < 0) {error("ERROR in recvfrom");}

    printf("%s\n", buf);

    if (strcmp(temp, "exit\n") == 0){break;}

    bzero(buf, BUFSIZE);
    printf("Enter Command: ");
    fgets(buf, BUFSIZE, stdin);

  }
  return 0;
}
