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

#define BUFSIZE 4096

//#define DEBUG
#define COLOR
/*
 * error - wrapper for perror
 */
void error(char *msg, int ex) 
{
  perror(msg);
  if (ex)
  {
    exit(1);
  }
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
  char *bufargs[3];
  char *bufpos;
  char *bufarg;
  int argcount;
  char *hostaddrp; /* dotted decimal host addr string */
  int optval; /* flag value for setsockopt */
  int n; /* message byte size */
  DIR *currdir;
  struct dirent *dirptr;
  struct timeval t;
  struct timeval t1;

  
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
  if (sockfd < 0) {error("ERROR opening socket", 1);}
  /* setsockopt: Handy debugging trick that lets 
   * us rerun the server immediately after we kill it; 
   * otherwise we have to wait about 20 secs. 
   * Eliminates "ERROR on binding: Address already in use" error. 
   */
  optval = 1;
  t.tv_sec = 10;
  t.tv_usec = 0;
  t1.tv_sec = 0;
  t1.tv_usec = 0;
  setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, (const void *)&optval , sizeof(int));
  setsockopt(sockfd, SOL_SOCKET, SO_SNDTIMEO, &t, sizeof(t));
  setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &t1, sizeof(t1));
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
  if (bind(sockfd, (struct sockaddr *) &serveraddr, sizeof(serveraddr)) < 0) {error("ERROR on binding", 1);}
  
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
    if (n < 0) {error("ERROR in recvfrom", 0);}
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
    if (hostp == NULL) {error("ERROR on gethostbyaddr", 0);}
    hostaddrp = inet_ntoa(clientaddr.sin_addr);
    if (hostaddrp == NULL) {error("ERROR on inet_ntoa\n", 0);}

    #ifdef DEBUG
      printf("\nserver received datagram from %s (%s)\n", hostp->h_name, hostaddrp);
      printf("server received %d/%d bytes: %s\n", (int)strlen(buf), n, buf);
    #endif

    /* 
     * handle client requests below
     */
    if (strcmp(buf, "ls\n") == 0)
    {
      memset(buf, '\0', BUFSIZE);
      if ((currdir = opendir(".")) == NULL)
      {
        strcpy(buf, "error: server couldn't open current directory\n");
      }
      else
      {
        while((dirptr = readdir(currdir)) != NULL)
        {
          #ifdef COLOR
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

          #else
            strcat(buf, dirptr->d_name);
            strcat(buf, "\n"); 
          #endif
        }
        closedir(currdir);
      }
      n = sendto(sockfd, buf, (int)strlen(buf), 0, (struct sockaddr *) &clientaddr, clientlen);
      if (n < 0) {error("ERROR in sendto", 0);}
    }

    else if ((strcmp(bufargs[0], "delete") == 0) && argcount == 2)
    {
      n = sendto(sockfd, "DELETE", (int)strlen("DELETE"), 0, (struct sockaddr *) &clientaddr, clientlen);
      if (n < 0) {error("ERROR in sendto", 0);}
      *(bufargs[1]+(strlen(bufargs[1]))-1) = '\0';
      char *fname = bufargs[1];
      FILE *fp;
      if ((fp = fopen(fname, "r")) != NULL)
      {
        fclose(fp);
        if (remove(fname) == 0)
        {
          memset(buf, '\0', BUFSIZE);
          sprintf(buf, "success: file '%s' deleted from server\n", fname);
          n = sendto(sockfd, buf, (int)strlen(buf), 0, (struct sockaddr *) &clientaddr, clientlen);
          if (n < 0) {error("ERROR in sendto", 0);}
        }
        else
        {
          memset(buf, '\0', BUFSIZE);
          sprintf(buf, "error: could not delete file '%s'\n", fname);
          n = sendto(sockfd, buf, (int)strlen(buf), 0, (struct sockaddr *) &clientaddr, clientlen);
          if (n < 0) {error("ERROR in sendto", 0);}
        }
      }
      else
      {
        memset(buf, '\0', BUFSIZE);
        sprintf(buf, "error: file '%s' not found\n", fname);
        n = sendto(sockfd, buf, (int)strlen(buf), 0, (struct sockaddr *) &clientaddr, clientlen);
        if (n < 0) {error("ERROR in sendto", 0);}
      }
    }

    else if ((strcmp(bufargs[0], "put") == 0) && argcount == 2)
    {
      t1.tv_sec = 10;
      char *fname;
      if(strstr(buf, "/"))
      {
        fname = strrchr(buf, '/');
      }
      else
      {
        fname = strrchr(buf, ' ');
      }
      fname = (fname+1);
      fname[strlen(fname)-1] = '\0';
      FILE *fp;
      if (((fp = fopen(fname, "wb")) != NULL))
      {
        n = sendto(sockfd, "PUTINIT", (int)strlen("PUTINIT"), 0, (struct sockaddr *) &clientaddr, clientlen);
        if (n < 0) 
        {
          error("ERROR in sendto", 0);
        }
        while(strcmp(buf, "PUTSUCCESS") != 0)
        {
          memset(buf, '\0', BUFSIZE);
          n = recvfrom(sockfd, buf, BUFSIZE, 0, (struct sockaddr *) &clientaddr, &clientlen);
          if (n < 0) 
          {
            error("ERROR in recvfrom", 0);
          }

          if (strstr(buf, "PUT") && (bufarg = strstr(buf, " ")) != NULL)
          {
            bufarg = (bufarg+1);
            int bytes_to_copy = atoi(bufarg);
            memset(buf, '\0', BUFSIZE);
            n = recvfrom(sockfd, buf, bytes_to_copy, 0, (struct sockaddr *) &clientaddr, &clientlen);
            if (n < 0) 
            {
              error("ERROR in recvfrom", 0);
            }

            fwrite(buf, 1, bytes_to_copy, fp);

            n = sendto(sockfd, "SUCCESS", (int)strlen("SUCCESS"), 0, (struct sockaddr *) &clientaddr, clientlen);
            if (n < 0) 
            {
              error("ERROR in sendto", 0);
            }
          }
        }
        fclose(fp);
      }
      else
      {
        sprintf(buf, "PUTFAILURE\nerror: could not create file'%s'\n", fname);
        n = sendto(sockfd, buf, (int)strlen(buf), 0, (struct sockaddr *) &clientaddr, clientlen);
        if (n < 0) 
        {
          error("ERROR in sendto", 0);
        }
      }
      t1.tv_sec = 0;
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

      if ((fp = fopen(fname, "rb")) != NULL)
      {
        fseek(fp, 0, SEEK_END);
        fpos = ftell(fp);
        fseek(fp, 0, SEEK_SET);

        #ifdef DEBUG
          printf("server sending '%s' (%d bytes) to client (%s)\n", fname, fpos, hostaddrp);
        #endif

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

          char success[50];
          memset(buf, '\0', BUFSIZE);
          memset(success, '\0', 32);

          sprintf(success, "GETINIT %d", fsend);

          n = sendto(sockfd, success, (int)strlen(success), 0, (struct sockaddr *) &clientaddr, clientlen);
          if (n < 0) 
          {
            error("ERROR in sendto", 0);
          }

          #ifdef DEBUG
            printf("progress: %d/%d bytes\n", fsum, fpos);
          #endif

          fread(buf, 1, fsend, fp); 

          n = sendto(sockfd, buf, fsend, 0, (struct sockaddr *) &clientaddr, clientlen);
          if (n < 0) {error("ERROR in sendto", 0);}

          memset(buf, '\0', BUFSIZE);
          n = recvfrom(sockfd, buf, BUFSIZE, 0, (struct sockaddr *) &clientaddr, &clientlen);
          if (n < 0) 
          {
            error("ERROR in recvfrom", 0);
          }

          if (strcmp(buf, "FAILED") == 0)
          {
            #ifdef DEBUG
              printf("'GET' STOPPED BY CLIENT\n");
            #endif
            break;
          }
        }
        if (strcmp(buf, "FAILED") == 0)
        {
          continue;
        }
        n = sendto(sockfd, "GETSUCCESS", (int)strlen("GETSUCCESS"), 0, (struct sockaddr *) &clientaddr, clientlen);
        if (n < 0) 
        {
          error("ERROR in sendto", 0);
        }

        fclose(fp);
      }
      else
      {
        n = sendto(sockfd, "GETFAILURE", (int)strlen("GETFAILURE"), 0, (struct sockaddr *) &clientaddr, clientlen);
        if (n < 0) {error("ERROR in sendto", 0);}

        sprintf(buf, "error: file '%s' not found\n", fname);

        n = sendto(sockfd, buf, (int)strlen(buf), 0, (struct sockaddr *) &clientaddr, clientlen);
        if (n < 0) {error("ERROR in sendto", 0);}
      }
      

    }

    else if (strcmp(buf, "exit\n") == 0)
    {
      memset(buf, '\0', BUFSIZE);
      strcpy(buf, "Goodbye!\n");
      n = sendto(sockfd, buf, (int)strlen(buf), 0, (struct sockaddr *) &clientaddr, clientlen);
      if (n < 0) {error("ERROR in sendto", 0);}
    }

    else
    {
      n = sendto(sockfd, "FAILURE", (int)strlen("FAILURE"), 0, (struct sockaddr *) &clientaddr, clientlen);
      if (n < 0) {error("ERROR in sendto", 0);}

      char errbuf[strlen(buf)];
      memset(errbuf, '\0', strlen(buf));
      strcpy(errbuf, buf);
      errbuf[strlen(errbuf)-1] = '\0';
      char *newbuf = errbuf;
      memset(buf, '\0', BUFSIZE);
      sprintf(buf, "error: command '%s' unknown\n", newbuf);

      n = sendto(sockfd, buf, (int)strlen(buf), 0, (struct sockaddr *) &clientaddr, clientlen);
      if (n < 0) {error("ERROR in sendto", 0);}
    }
  }
}
