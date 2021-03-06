/* file: echosrv.c

   Bare-bones single-threaded TCP server. Listens for connections
   on "ephemeral" socket, assigned dynamically by the OS.

   This started out with an example in W. Richard Stevens' book
   "Advanced Programming in the Unix Environment".  I have
   modified it quite a bit, including changes to make use of my
   own re-entrant version of functions in echolib.

   NOTE: See comments starting with "NOTE:" for indications of
   places where code needs to be added to make this multithreaded.
   Remove those comments from your solution before turning it in,
   and replace them by comments explaining the parts you have
   changed.

   Ted Baker
   February 2015

 */

#include "config.h"
#include <pthread.h>
/* not needed now, but will be needed in multi-threaded version */
#include "echolib.h"
#include "checks.h"
#include <sys/epoll.h>

#define MAXCLIENT 20
#define MAXQUESIZE 1024
#define Q_LEN 1024

pthread_mutex_t cnt_mtx, queue_mutex;
int client_cnt=0;

struct queue{
  int number;
  int *ret;
};

struct queue q[MAXQUESIZE];
int que_idx=0;

int q_count = 0;
int tail = 0; int head = 0;
pthread_cond_t cond;
pthread_mutex_t queue_mutex;
pthread_mutex_t event_mutex;
pthread_mutex_t pool_mutex;
int pool_count = 0;

void* signal_thread(void* args){
  while(1){
    pthread_mutex_lock(&queue_mutex);
    pthread_mutex_lock(&pool_mutex);
    if(q_count > 0 && pool_count > 0){
      pool_count -=1;
      q_count -= 1;
      pthread_mutex_unlock(&pool_mutex);
      pthread_mutex_unlock(&queue_mutex);
      pthread_cond_signal(&cond);
    }else{
      pthread_mutex_unlock(&pool_mutex);
      pthread_mutex_unlock(&queue_mutex);
    }
    
  }
}


void* pool_thread(void* args){
  while(1){
    /*
    pthread_mutex_lock(&queue_mutex);
    pool_count++;
    pthread_cond_wait(&cond, &queue_mutex);
    pthread_mutex_unlock(&queue_mutex);
    */

    pthread_mutex_lock(&pool_mutex);
    pool_count++;
    pthread_cond_wait(&cond, &pool_mutex);
    pthread_mutex_unlock(&pool_mutex);

    //read data
    pthread_mutex_lock(&queue_mutex);
    int data = q[head].number;
    int* ret = q[head].ret;
    head = (head+1)%Q_LEN;
    pthread_mutex_unlock(&queue_mutex);

    int tmp_ret = 1;
    //prime
    if(data<=1)
      tmp_ret = 0;
    for(int i=2; i*i<=data; i++)
    {
      if(data%i == 0){
        tmp_ret = 0;
        break;
      }
    }
    usleep(5000000);
    // write ret.
    //pthread_mutex_lock(&queue_mutex);
    *ret = tmp_ret;
    //pthread_mutex_unlock(&queue_mutex);
  }
}

/* set up socket to use in listening for connections */
void
open_listening_socket (int *listenfd) {
  struct sockaddr_in servaddr;
  const int server_port = 0; /* use ephemeral port number */
  socklen_t namelen;
  memset (&servaddr, 0, sizeof(struct sockaddr_in));
  servaddr.sin_family = AF_INET;
  /* htons translates host byte order to network byte order; ntohs
     translates network byte order to host byte order */
  servaddr.sin_addr.s_addr = htonl (INADDR_ANY);
  servaddr.sin_port = htons (server_port);
  /* create the socket */
  CHECK (*listenfd = socket(AF_INET, SOCK_STREAM, 0))
  /* bind it to the ephemeral port number */
  CHECK (bind (*listenfd, (struct sockaddr *) &servaddr, sizeof (servaddr)));
  /* extract the ephemeral port number, and put it out */
  namelen = sizeof (servaddr);
  CHECK (getsockname (*listenfd, (struct sockaddr *) &servaddr, &namelen));
  fprintf (stderr, "server using port %d\n", ntohs(servaddr.sin_port));
}

/* handler for SIGINT, the signal conventionally generated by the
   control-C key at a Unix console, to allow us to shut down
   gently rather than having the entire process killed abruptly. */ 
void
siginthandler (int sig, siginfo_t *info, void *ignored) {
  shutting_down = 1;
  printf("Server terminated\n");
}

void
install_siginthandler () {
  struct sigaction act;
  /* get current action for SIGINT */
  CHECK (sigaction (SIGINT, NULL, &act));
  /* add our handler */
  act.sa_sigaction = siginthandler;
  /* update action for SIGINT */
  CHECK (sigaction (SIGINT, &act, NULL));
}

struct epoll_event events[100];
struct epoll_event read_ev;
struct epoll_event quit_ev;
int epoll_fd_read = 0, epoll_fd_quit=0, read_wait=0, quit_wait=0;
int main_fd = 0;

void* scan_socket(void* arg)
{
  connection_t conn;
  connection_init (&conn);
  while(1)
  {
start:
    //Check client quit event
    pthread_mutex_lock(&event_mutex);
    quit_wait = epoll_wait(epoll_fd_quit, events, 100,10);
    pthread_mutex_unlock(&event_mutex);
    for(int i=0;i<quit_wait;i++)
    {
quit:
      pthread_mutex_lock(&cnt_mtx);
      client_cnt--;
      pthread_mutex_unlock(&cnt_mtx);
      printf("socket num %d disconnected\n", events[i].data.fd);
      printf("Current member : %d\n", client_cnt);

      epoll_ctl(epoll_fd_read, EPOLL_CTL_DEL, events[i].data.fd, &quit_ev);
      epoll_ctl(epoll_fd_quit, EPOLL_CTL_DEL, events[i].data.fd, &read_ev);
      CHECK (close (events[i].data.fd));
    }
    //Check client send buffer
    pthread_mutex_lock(&event_mutex);
    read_wait = epoll_wait(epoll_fd_read, events, 100,10);
    pthread_mutex_unlock(&event_mutex);
    for(int i=0;i<read_wait;i++)
    {
      if(events[i].data.fd == main_fd)  goto start;
      printf("fd %d sent msg\n", events[i].data.fd);
      ssize_t  n, result;
      int *ret, *temp_num;
      char line[MAXLINE];
      char buf[40];
      conn.sockfd = events[i].data.fd;

      if ((n = readline (&conn, line, MAXLINE)) == 0) goto quit;
      printf("msg : %s\n", line);
      /* connection closed by other end */

      //Parse data and
      //Add data to queue

      //parsing and queue register
      int finish=0;
      //Set element size
      int num_cnt = atoi(strtok(line, " "));
      ret = (int*)malloc(sizeof(int)*num_cnt);
      temp_num = (int*)malloc(sizeof(int)*num_cnt);
        
      for(int j=0;j<num_cnt;j++)
      {
        pthread_mutex_lock(&queue_mutex);
        ret[j] = -1;
        temp_num[j] = atoi(strtok(NULL, " "));
        q[que_idx].number = temp_num[j];
        q[que_idx].ret=&ret[j];
        que_idx = (que_idx + 1)%Q_LEN;
        q_count = q_count + 1;
        pthread_mutex_unlock(&queue_mutex);
      }     
      

      //Wait until whole number calculated
      printf("number of results : %d\n\n", num_cnt);
      while(1)
      {
        finish=0;
        for(int k=0;k<num_cnt;k++)
        {
          if(ret[k]==-1)
            finish++;
        }
        if(finish==0)
          goto print;
      }
print:  ;
      //After calculation
      printf("Wait end\n");
      char *is_prime;
      for(int i=0;i<num_cnt;i++)
      {
        line[0]='\0';
        sprintf(buf,"%d is ", temp_num[i]);
        if(ret[i]==1){
          is_prime = "prime number\n";
        }else{
          is_prime = "not prime number\n";
        }
        strcat(line, buf);
        strcat(line, is_prime);
        n= strlen(line);
        result = writen (&conn, line, n);
      }
      if (result != n) {
        perror ("writen failed");
        goto quit;
      }
    }
  }
}



int
main (int argc, char *argv[]) {
  int connfd, listenfd;
  socklen_t clilen;
  struct sockaddr_in cliaddr;
  int thread_cnt=4, c, accept_cnt=1;

  pthread_t sig_thread;

  connection_t conn;
  connection_init (&conn);

  while((c = getopt(argc, argv, "n:a:")) != -1){
    switch(c)
    {
      case 'n':
        thread_cnt = atoi(optarg);
        break;
      case 'a':
        accept_cnt = atoi(optarg);
        break;
      case '?':
        printf("Thread option error\n");
        printf("%s -n <number of threads>\n", argv[0]);
        break;
    }
  } 
  pthread_mutex_init(&cnt_mtx, NULL);
  pthread_mutex_init(&queue_mutex, NULL);
  pthread_mutex_init(&event_mutex, NULL);
  pthread_mutex_init(&pool_mutex, NULL);

  printf("Thread pool size : %d\n", thread_cnt);

  pthread_t worker_thread[thread_cnt];
  pthread_t accept_thread[accept_cnt];

  for(int i=0;i<thread_cnt;i++)
  {
    pthread_create(&worker_thread[i], NULL, pool_thread, NULL);
  }
  pthread_create(&sig_thread, NULL, signal_thread, NULL);

  /* NOTE: To make this multi-threaded, You may need insert
     additional initialization code here, but you will not need to
     modify anything below here, though you are permitted to
     change anything in this file if you feel it is necessary for
     your design */

  install_siginthandler();
  open_listening_socket (&listenfd);
  CHECK (listen (listenfd, 4));
  printf("listenfd : %d\n", listenfd);
  main_fd = listenfd;
  /* allow up to 4 queued connection requests before refusing */

  epoll_fd_read = epoll_create(100);
  epoll_fd_quit = epoll_create(100);

  quit_ev.events = EPOLLRDHUP;
  quit_ev.data.fd = listenfd;
  read_ev.events = EPOLLIN;
  read_ev.data.fd = listenfd;
  //add server event
  epoll_ctl(epoll_fd_read, EPOLL_CTL_ADD, listenfd, &read_ev);
  epoll_ctl(epoll_fd_quit, EPOLL_CTL_ADD, listenfd, &quit_ev);
  printf("Number of accept thread : %d\n", accept_cnt);
  for(int i=0;i<accept_cnt;i++)
  {
    pthread_create(&accept_thread[i], NULL, scan_socket, NULL);  
  }

  while (! shutting_down) {
    errno = 0;
    clilen = sizeof (cliaddr); /* length of address can vary, by protocol */
    printf("Waiting for next connections....\n\n");

    if ((connfd = accept (listenfd, (struct sockaddr *) &cliaddr, &clilen)) < 0) {
      if (errno != EINTR) ERR_QUIT ("accept"); 
      /* otherwise try again, unless we are shutting down */
    } else {
      pthread_mutex_lock(&pool_mutex);
      if(!pool_count) {
        printf("pool %d\n", pool_count);
        pthread_mutex_unlock(&pool_mutex);
        conn.sockfd = connfd;
        char* sorry = "Sorry Server is busy\n\0";
        writen (&conn, sorry, strlen(sorry));
        printf("%d refused\n", connfd);
        close(connfd);
        continue;
      }else{
        pthread_mutex_unlock(&pool_mutex);
      }

      printf("socker number %d Connected\n", connfd);
      pthread_mutex_lock(&cnt_mtx);
      client_cnt++;
      pthread_mutex_unlock(&cnt_mtx);
      printf("Current number of client : %d\n", client_cnt);
      //Add event to events list
      quit_ev.events = EPOLLRDHUP;
      quit_ev.data.fd = connfd;
      epoll_ctl(epoll_fd_quit, EPOLL_CTL_ADD, connfd, &quit_ev);
      read_ev.events = EPOLLIN;
      read_ev.data.fd = connfd;
      epoll_ctl(epoll_fd_read, EPOLL_CTL_ADD, connfd, &read_ev);
    }
  }
  CHECK (close (listenfd));
  pthread_mutex_destroy(&cnt_mtx);
  pthread_mutex_destroy(&queue_mutex);
  return 0;
}