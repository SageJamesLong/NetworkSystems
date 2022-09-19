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

#define BUFSIZE 4096
//#define DEBUG

/* 
 * error - wrapper for perror
 */
void error(char *msg, int ex) {
  perror(msg);
  if (ex)
  {
    exit(0);
  }
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
  struct timeval t;
  struct timeval t1;
  t.tv_sec = 10;
  t.tv_usec = 0;
  t1.tv_sec = 10;
  t1.tv_usec = 0;
  
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
  if (sockfd < 0) {error("ERROR opening socket", 1);}

  /* gethostbyname: get the server's DNS entry */

  setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &t, sizeof(t));
  setsockopt(sockfd, SOL_SOCKET, SO_SNDTIMEO, &t1, sizeof(t1));

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

    if (strstr(buf, "put") != NULL)
    {
      int argcount;
      char *bufarg;
      char *bufpos;
      char *bufargs[2];
      bufpos = temp;
      argcount = 0;
      while ((bufarg = strtok_r(bufpos, " ", &bufpos)))
      {
        if (argcount <= 1)
        {
          bufargs[argcount] = bufarg;
        }
        argcount++;
      }
      if (argcount > 2)
      {
        printf("error: too many arguments\n");
      }
      else if (argcount <= 1)
      {
        printf("error: filename not specified\n");
      }
      else
      {
        FILE *fp;
        *(bufargs[1]+(strlen(bufargs[1]))-1) = '\0';
        char *fname = bufargs[1];

        if (((fp = fopen(fname, "rb")) != NULL))
        {
          n = sendto(sockfd, buf, strlen(buf), 0, (struct sockaddr *)&serveraddr, serverlen);
          if (n < 0) 
          {
            error("ERROR in sendto", 0);
          }
          bzero(buf, BUFSIZE);
          n = recvfrom(sockfd, buf, BUFSIZE, 0, (struct sockaddr *)&serveraddr, &serverlen);
          if (n < 0) 
          {
            error("ERROR in recvfrom", 0);
          }
          int fpos = 0;
          int ftop = 0;
          int fsend = 0;
          int fsum = 0;

          fseek(fp, 0, SEEK_END);
          fpos = ftell(fp);
          fseek(fp, 0, SEEK_SET);

          #ifdef DEBUG
            printf("client sending '%s' (%d bytes)\n", fname, fpos);
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

            char success[100];
            memset(buf, '\0', BUFSIZE);
            memset(success, '\0', 100);

            sprintf(success, "PUT %d", fsend);
            n = sendto(sockfd, success, (int)strlen(success), 0, (struct sockaddr *)&serveraddr, serverlen);
            if (n < 0) 
            {
              error("ERROR in sendto", 0);
            }

            #ifdef DEBUG
              printf("progress: %d/%d bytes\n", fsum, fpos);
            #endif

            fread(buf, 1, fsend, fp);

            n = sendto(sockfd, buf, fsend, 0, (struct sockaddr *)&serveraddr, serverlen);
            if (n < 0) 
            {
              error("ERROR in sendto", 0);
            }
            memset(buf, '\0', BUFSIZE);
            n = recvfrom(sockfd, buf, BUFSIZE, 0, (struct sockaddr *)&serveraddr, &serverlen);
            if (n < 0) 
            {
              error("ERROR in recvfrom", 0);
            }
            if (strcmp(buf, "SUCCESS") != 0)
            {
              #ifdef DEBUG
                printf("'GET' STOPPED BY SERVER\n");
              #endif
              fclose(fp);
              break;
            }
          }
          n = sendto(sockfd, "PUTSUCCESS", strlen("PUTSUCCESS"), 0, (struct sockaddr *)&serveraddr, serverlen);
          if (n < 0) 
          {
            error("ERROR in sendto", 0);
          }
          fclose(fp);
        }
        else
        {
          printf("error: file '%s' not found\n", fname);
        }
      }
    }
    else
    {
      n = sendto(sockfd, buf, strlen(buf), 0, (struct sockaddr *)&serveraddr, serverlen);
      if (n < 0) {error("ERROR in sendto", 0);}
      bzero(buf, BUFSIZE);

      n = recvfrom(sockfd, buf, BUFSIZE, 0, (struct sockaddr *)&serveraddr, &serverlen);
      if (n < 0) {error("ERROR in recvfrom", 0);}

      if (strcmp(buf, "FAILURE") == 0 || strcmp(buf, "GETFAILURE") == 0)
      {
        bzero(buf, BUFSIZE);
        n = recvfrom(sockfd, buf, BUFSIZE, 0, (struct sockaddr *)&serveraddr, &serverlen);
        if (n < 0) {error("ERROR in recvfrom", 0);}
        printf("%s", buf);

        bzero(buf, BUFSIZE);
        printf("Enter Command: ");
        fgets(buf, BUFSIZE, stdin);
        continue;
      }
      else if (strstr(buf, "GETINIT") != NULL)
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
        if ((fp = fopen(fname, "wb")) != NULL)
        {
          while(strcmp(buf, "GETSUCCESS") != 0)
          {
            if (strstr(buf, "GETINIT") && (bufarg = strstr(buf, " ")) != NULL)
            {
              bufarg = (bufarg+1);
              bytes_to_copy = atoi(bufarg);
              bzero(buf, BUFSIZE);
              n = recvfrom(sockfd, buf, BUFSIZE, 0, (struct sockaddr *)&serveraddr, &serverlen);
              if (n < 0) 
              {
                error("ERROR in recvfrom", 0);
              }

              fwrite(buf, 1, bytes_to_copy, fp);

              n = sendto(sockfd, "SUCCESS", strlen("SUCCESS"), 0, (struct sockaddr *)&serveraddr, serverlen);
              if (n < 0) 
              {
                error("ERROR in sendto", 0);
              }

              bzero(buf, BUFSIZE);
              n = recvfrom(sockfd, buf, BUFSIZE, 0, (struct sockaddr *)&serveraddr, &serverlen);
              if (n < 0) 
              {
                error("ERROR in recvfrom", 0);
              }
            }
          }
          fclose(fp);
        }
        else
        {
          printf("error: failed to create file '%s'\n", fname);
          bzero(buf, BUFSIZE);
          n = recvfrom(sockfd, buf, BUFSIZE, 0, (struct sockaddr *)&serveraddr, &serverlen);
          if (n < 0) {error("ERROR in recvfrom", 0);}
          n = sendto(sockfd, "FAILED", strlen("FAILED"), 0, (struct sockaddr *)&serveraddr, serverlen);
          if (n < 0) 
          {
            error("ERROR in sendto", 0);
          }
          bzero(buf, BUFSIZE);
          printf("Enter Command: ");
          fgets(buf, BUFSIZE, stdin);
          continue;
        }
      } 
      else if (strcmp(buf, "DELETE") == 0)
      {
        bzero(buf, BUFSIZE);
        n = recvfrom(sockfd, buf, BUFSIZE, 0, (struct sockaddr *)&serveraddr, &serverlen);
        if (n < 0) {error("ERROR in recvfrom", 0);}
        printf("%s", buf);
      }
      else
      {
        if (strcmp(temp, "exit\n") == 0)
        {
          printf("%s", buf);
          break;
        }

        if (strcmp(temp, "ls\n") == 0)
        {
          printf("%s", buf);
        }
      }
    }

    bzero(buf, BUFSIZE);
    printf("Enter Command: ");
    fgets(buf, BUFSIZE, stdin);

  }
  return 0;
}
