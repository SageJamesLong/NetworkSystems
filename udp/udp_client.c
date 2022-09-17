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

#define BUFSIZE 2048

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
  socklen_t serverlen;
  struct sockaddr_in serveraddr;
  struct hostent *server;
  char *hostname;
  char buf[BUFSIZE];
  char temp[BUFSIZE];
  /* check command line arguments */
  if (argc != 3) 
  {
    fprintf(stderr,"usage: %s <hostname> <port>\n", argv[0]);
    exit(0);
  }
  hostname = argv[1];
  portno = atoi(argv[2]);

  /* socket: create the socket */

  sockfd = socket(AF_INET, SOCK_DGRAM, 0);
  if (sockfd < 0) {error("ERROR opening socket");}

  /* gethostbyname: get the server's DNS entry */

  server = gethostbyname(hostname);
  if (server == NULL) 
  {
    fprintf(stderr,"ERROR, no such host as %s\n", hostname);
    exit(0);
  }

  /* build the server's Internet address */

  bzero((char *) &serveraddr, sizeof(serveraddr));
  serveraddr.sin_family = AF_INET;
  bcopy((char *)server->h_addr, (char *)&serveraddr.sin_addr.s_addr, server->h_length);
  serveraddr.sin_port = htons(portno); //htons converts to (short) network byte order
  
  bzero(buf, BUFSIZE);
  printf("Enter Command: ");
  fgets(buf, BUFSIZE, stdin);
  
  serverlen = sizeof(serveraddr);

  while(1)
  {
    bzero(temp, BUFSIZE);
    strcpy(temp, buf);
    
    n = sendto(sockfd, buf, strlen(buf), 0, (struct sockaddr *)&serveraddr, serverlen);
    if (n < 0) {error("ERROR in sendto");}
    
    printf("\n");
    bzero(buf, BUFSIZE);

    n = recvfrom(sockfd, buf, BUFSIZE, 0, (struct sockaddr *)&serveraddr, &serverlen);
    if (n < 0) {error("ERROR in recvfrom");}

    if (strcmp(buf, "FAILURE") == 0 || strcmp(buf, "GETFAILURE") == 0)
    {
      printf("%s\n", buf);
      bzero(buf, BUFSIZE);
      n = recvfrom(sockfd, buf, BUFSIZE, 0, (struct sockaddr *)&serveraddr, &serverlen);
      if (n < 0) {error("ERROR in recvfrom");}
      printf("%s\n", buf);

      bzero(buf, BUFSIZE);
      printf("Enter Command: ");
      fgets(buf, BUFSIZE, stdin);
      continue;
    }
    if (strstr(buf, "GETINIT") != NULL)
    {
      FILE *fp;
      char *fname;
      char *bufarg;
      int bytes_to_copy;
      if(strstr(temp, "/"))
      {
        fname = strrchr(temp, '/');
      }
      else
      {
        fname = strrchr(temp, ' ');
      }
      fname = (fname+1);
      fname[strlen(fname)-1] = '\0';
      if ((fp = fopen(fname, "w")) != NULL)
      {
        while(strcmp(buf, "GETSUCCESS") != 0)
        {
          if (strstr(buf, "GETINIT") && (bufarg = strstr(buf, " ")) != NULL)
          {
            bufarg = (bufarg+1);
            bytes_to_copy = atoi(bufarg);
            bzero(buf, BUFSIZE);
            n = recvfrom(sockfd, buf, BUFSIZE, 0, (struct sockaddr *)&serveraddr, &serverlen);
            if (n < 0) {error("ERROR in recvfrom");}

            fwrite(buf, 1, bytes_to_copy, fp);
            n = sendto(sockfd, "SUCCESS", strlen("SUCCESS"), 0, (struct sockaddr *)&serveraddr, serverlen);
            if (n < 0) {error("ERROR in sendto");}

            bzero(buf, BUFSIZE);
            n = recvfrom(sockfd, buf, BUFSIZE, 0, (struct sockaddr *)&serveraddr, &serverlen);
            if (n < 0) {error("ERROR in recvfrom");}
          }
        }
        fclose(fp);
      }
      else
      {
        printf("FAILED to create file '%s'\n", fname);
        bzero(buf, BUFSIZE);
        n = recvfrom(sockfd, buf, BUFSIZE, 0, (struct sockaddr *)&serveraddr, &serverlen);
        if (n < 0) {error("ERROR in recvfrom");}
        n = sendto(sockfd, "FAILED", strlen("FAILED"), 0, (struct sockaddr *)&serveraddr, serverlen);
        if (n < 0) {error("ERROR in sendto");}
        bzero(buf, BUFSIZE);
        printf("\nEnter Command: ");
        fgets(buf, BUFSIZE, stdin);
        continue;
      }
    } 
    else
    {
      if (strcmp(temp, "exit\n") == 0)
      {
        printf("%s\n", buf);
        break;
      }

      if (strcmp(temp, "ls\n") == 0)
      {
        printf("%s\n", buf);
      }
    }

    bzero(buf, BUFSIZE);
    printf("Enter Command: ");
    fgets(buf, BUFSIZE, stdin);

  }
  return 0;
}
