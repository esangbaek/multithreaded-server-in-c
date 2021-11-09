/* file: echocli.c

   Bare-bones TCP client with commmand-line argument to specify
   port number to use to connect to server.  Server hostname is
   specified by environment variable "SERVERHOST".

   This started out with an example in W. Richard Stevens' book
   "Advanced Programming in the Unix Environment".  I have
   modified it quite a bit, including changes to make use of my
   own re-entrant version of functions in echolib.
   
   Ted Baker
   February 2015

 */

#include "config.h"
#include "echolib.h"
#include "checks.h"
#include <sys/time.h>
#include <pthread.h>

#define USECPSEC 1000000ULL

int sent=0, received=0;
u_int64_t difft;

u_int64_t dtime_usec(u_int64_t start)
{
  struct timeval tv;
  gettimeofday(&tv, 0);
  return ((tv.tv_sec*USECPSEC)+tv.tv_usec)-start;
}

/* the main service loop of the client; assumes sockfd is a
   connected socket */
void
client_work (int sockfd) {
  connection_t conn;
  u_int64_t difft;
  char *p;
  char sendline[MAXLINE], recvline[MAXLINE];
  connection_init (&conn);
  conn.sockfd = sockfd;
  while ((p = fgets (sendline, sizeof (sendline), stdin))) {
    difft = dtime_usec(0);
    CHECK (writen (&conn, sendline, strlen (sendline)));

    if (readline (&conn, recvline, sizeof (recvline)) <= 0)
      ERR_QUIT ("str_cli: server terminated connection prematurely");
    difft = dtime_usec(difft);
    printf("Send <-> Receive time: %f\n",difft/(float)USECPSEC);
    fprintf (stdout, "%s", recvline); /* rely that line contains "/n" */
    fflush (stdout);
  }
  /* null pointer returned by fgets indicates EOF */
}

/* fetch server port number from main program argument list */
int
get_server_port (int argc, char **argv) {
  int val;
  char * endptr;
  if (argc != 2) goto fail;
  errno = 0;
  val = (int) strtol (argv [1], &endptr, 10);
  if (*endptr) goto fail;
  if ((val < 0) || (val > 0xffff)) goto fail;
#ifdef DEBUG
  fprintf (stderr, "port number = %d\n", val);
#endif
  return val;
fail:
   fprintf (stderr, "usage: echosrv [port number]\n");
   exit (-1);
}

/* set up IP address of host, using DNS lookup based on SERVERHOST
   environment variable, and port number provided in main program
   argument list. */
void
set_server_address (struct sockaddr_in *servaddr, int argc, char **argv) {
  struct hostent *hosts;
  char *server;
  const int server_port = get_server_port (argc, argv);
  if ( !(server = getenv ("SERVERHOST"))) {
    QUIT ("usage: SERVERHOST undefined.  Set it to name of server host, and export it.");
  }
  memset (servaddr, 0, sizeof(struct sockaddr_in));
  servaddr->sin_family = AF_INET;
  servaddr->sin_port = htons (server_port);
  if ( !(hosts = gethostbyname (server))) {
    ERR_QUIT ("usage: gethostbyname call failed");
  }
  servaddr->sin_addr = *(struct in_addr *) (hosts->h_addr_list[0]);
}

void* send_msg(void* arg)
{
  int sockfd = *((int*)arg);
  connection_t conn;
  char *p;
  char sendline[MAXLINE], buf[MAXLINE];
  connection_init (&conn);
  conn.sockfd = sockfd;

  while ((p = fgets (sendline, sizeof (sendline), stdin))) {
    strcpy(buf, sendline);
    sent =  atoi(strtok(buf, " "));
    difft = dtime_usec(0);
    CHECK (writen (&conn, sendline, strlen (sendline)));
  }
  return 0;
}

void* recv_msg(void* arg)
{
  int sockfd = *((int*)arg);
  connection_t conn;
  
  char recvline[MAXLINE];
  connection_init (&conn);
  conn.sockfd = sockfd;

  while (1) {
    if (readline (&conn, recvline, sizeof (recvline)) <= 0)
      ERR_QUIT ("str_cli: server terminated connection prematurely");
    
    fprintf (stdout, "%s", recvline); /* rely that line contains "/n" */
    fflush (stdout);
    received++;
    if(sent == received)
    {
      difft = dtime_usec(difft);
      printf("Send <-> Receive time: %f\n",difft/(float)USECPSEC);
      sent=received=0;
    }
  }
  return 0;
}

int
main (int argc, char **argv) {
   int sockfd;
   struct sockaddr_in servaddr;
   struct timeval start, stop;
   pthread_t send_thd, recv_thd;


   /* time how long we have to wait for a connection */
   CHECK (gettimeofday (&start, NULL));
   set_server_address (&servaddr, argc, argv);
   if ( (sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0){
    ERR_QUIT ("usage: socket call failed");
   }
   CHECK (connect(sockfd, (struct sockaddr *) &servaddr, sizeof(servaddr)));
   CHECK (gettimeofday (&stop, NULL));
   fprintf (stderr, "connection wait time = %ld microseconds\n",
            (stop.tv_sec - start.tv_sec)*1000000 + (stop.tv_usec - start.tv_usec));

   pthread_create(&send_thd, NULL, send_msg, (void*)&sockfd);
   pthread_create(&recv_thd, NULL, recv_msg, (void*)&sockfd);
   //client_work (sockfd);
   pthread_join(send_thd, NULL);
   pthread_join(recv_thd, NULL);
   exit (0);
}
