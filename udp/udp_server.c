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

#define BUFSIZE 2048

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
  socklen_t clientlen; /* byte size of client's address */
  struct sockaddr_in serveraddr; /* server's addr */
  struct sockaddr_in clientaddr; /* client addr */
  struct hostent *hostp; /* client host info */
  char buf[BUFSIZE]; /* message buf */
  char splitbuf[BUFSIZE];
  char *bufargs[2];
  char *bufpos;
  char *bufarg;
  int argcount;
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
  
  clientlen = sizeof(clientaddr);
  /*
  * main loop: listen for incoming client datagrams
  */
  while (1) 
  {
    memset(buf, '\0', BUFSIZE);
    memset(splitbuf, '\0', BUFSIZE);
    memset(bufargs, '\0', sizeof(bufargs));
    /*
    * recvfrom: receive a UDP datagrams from a client
    */
    n = recvfrom(sockfd, buf, BUFSIZE, 0, (struct sockaddr *) &clientaddr, &clientlen);
    if (n < 0) {error("ERROR in recvfrom");}
    /* 
     * splitbuf: to parse whitespace seperated arguments while preserving buf
     */
    strcpy(splitbuf, buf);
    bufpos = splitbuf;
    argcount = 0;
    while ((bufarg = strtok_r(bufpos, " ", &bufpos)))
    {
      if (argcount <= 1)
      {
        bufargs[argcount] = bufarg;
      }
      argcount++;
    }
    /* 
     * gethostbyaddr: determine who sent the datagram
     */
    hostp = gethostbyaddr((const char *)&clientaddr.sin_addr.s_addr, sizeof(clientaddr.sin_addr.s_addr), AF_INET);
    if (hostp == NULL) {error("ERROR on gethostbyaddr");}
    hostaddrp = inet_ntoa(clientaddr.sin_addr);
    if (hostaddrp == NULL) {error("ERROR on inet_ntoa\n");}

    printf("\nserver received datagram from %s (%s)\n", hostp->h_name, hostaddrp);
    printf("server received %d/%d bytes: %s\n", (int)strlen(buf), n, buf);

    /* 
     * handle client requests below
     */
    if (strcmp(buf, "ls\n") == 0)
    {
      memset(buf, '\0', BUFSIZE);
      if ((currdir = opendir(".")) == NULL)
      {
        strcpy(buf, "error: server couldn't open current directory\n"); //HERE
      }
      else
      {
        while((dirptr = readdir(currdir)) != NULL)
        {
          if (dirptr->d_type == DT_REG || DT_DIR)
          {
            if (dirptr->d_type == DT_DIR)
            {
              strcat(buf, "\033[0;34m");
              strcat(buf, dirptr->d_name);
              strcat(buf, "\033[0m");
              strcat(buf, "\n");
            }
            else
            {
              strcat(buf, dirptr->d_name);
              strcat(buf, "\n"); 
            }
          }
        }
        closedir(currdir);
      }
      n = sendto(sockfd, buf, (int)strlen(buf), 0, (struct sockaddr *) &clientaddr, clientlen);
      if (n < 0) {error("ERROR in sendto");}
    }

    else if ((strcmp(bufargs[0], "get") == 0) && argcount == 2)
    {
      *(bufargs[1]+(strlen(bufargs[1]))-1) = '\0';
      char *fname = bufargs[1];

      FILE *fp;

      int fpos = 0;
      int ftop = 0;
      int fsend = 0;
      int fsum = 0;

      if ((fp = fopen(fname, "r")) != NULL)
      {
        fseek(fp, 0, SEEK_END);
        fpos = ftell(fp);
        fseek(fp, 0, SEEK_SET);

        printf("server sending '%s' (%d bytes) to client (%s)\n", fname, fpos, hostaddrp);

        while(fsum != fpos)
        {
          ftop = fpos-fsum;
          if (ftop > BUFSIZE)
          {
            fsend = BUFSIZE;
            fsum += BUFSIZE;
          }
          else
          {
            fsend = ftop;
            fsum += ftop;
          }

          char success[32];
          memset(buf, '\0', BUFSIZE);
          memset(success, '\0', 32);

          sprintf(success, "GETINIT %d", fsend);

          n = sendto(sockfd, success, (int)strlen(success), 0, (struct sockaddr *) &clientaddr, clientlen);
          if (n < 0) {error("ERROR in sendto");}

          printf("progress: %d/%d bytes\n", fsum, fpos);

          fread(buf, 1, fsend, fp); 

          n = sendto(sockfd, buf, fsend, 0, (struct sockaddr *) &clientaddr, clientlen);
          if (n < 0) {error("ERROR in sendto");}

          memset(buf, '\0', BUFSIZE);

          n = recvfrom(sockfd, buf, BUFSIZE, 0, (struct sockaddr *) &clientaddr, &clientlen);
          if (n < 0) {error("ERROR in recvfrom");}

          if (strcmp(buf, "FAILED") == 0)
          {
            printf("'GET' STOPPED BY CLIENT\n");
            fclose(fp);
            break;
          }
        }
        if (strcmp(buf, "FAILED") == 0)
        {
          continue;
        }
        n = sendto(sockfd, "GETSUCCESS", (int)strlen("GETSUCCESS"), 0, (struct sockaddr *) &clientaddr, clientlen);
        if (n < 0) {error("ERROR in sendto");}

        fclose(fp);
      }
      else
      {
        n = sendto(sockfd, "GETFAILURE", (int)strlen("GETFAILURE"), 0, (struct sockaddr *) &clientaddr, clientlen);
        if (n < 0) {error("ERROR in sendto");}

        sprintf(buf, "error: file '%s' not found\n", fname);

        n = sendto(sockfd, buf, (int)strlen(buf), 0, (struct sockaddr *) &clientaddr, clientlen);
        if (n < 0) {error("ERROR in sendto");}
      }
      

    }

    else if (strcmp(buf, "exit\n") == 0)
    {
      memset(buf, '\0', BUFSIZE);
      strcpy(buf, "Goodbye!\n");
      n = sendto(sockfd, buf, (int)strlen(buf), 0, (struct sockaddr *) &clientaddr, clientlen);
      if (n < 0) {error("ERROR in sendto");}
    }

    else
    {
      n = sendto(sockfd, "FAILURE", (int)strlen("FAILURE"), 0, (struct sockaddr *) &clientaddr, clientlen);
      if (n < 0) {error("ERROR in sendto");}

      char errbuf[strlen(buf)];
      memset(errbuf, '\0', strlen(buf));
      strcpy(errbuf, buf);
      errbuf[strlen(errbuf)-1] = '\0';
      char *newbuf = errbuf;
      memset(buf, '\0', BUFSIZE);
      sprintf(buf, "error: command '%s' unknown\n", newbuf);

      n = sendto(sockfd, buf, (int)strlen(buf), 0, (struct sockaddr *) &clientaddr, clientlen);
      if (n < 0) {error("ERROR in sendto");}
    }
  }
}
