#define GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <syslog.h>
#include <sys/types.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <signal.h>
#include <stdbool.h>
#include <pthread.h>
#include <semaphore.h>
#include <sys/queue.h>

#define OK                 1
#define ERROR              -1
#define USER_PORT          9000
#define MAX_CONNECTIONS    128         //Number of threads used
#define MAX_SIZE           128         //Size of character array
#define WRITE_TO_FILE      "/var/tmp/aesdsocketdata"
#define TIMESTAMP_INTERVAL 10

typedef struct threadParams_s
{
   pthread_t thread;
   char* buffer_ptr;
   int threadDone;
   int threadIdx;
   int threadSock;
   SLIST_ENTRY(threadParams_s) entries;
} slist_tparams_t;

int sockfd;
int socket_file_fd;
pthread_mutex_t mutex;
pthread_mutexattr_t mutex_attr;
static timer_t timer_1;
static struct itimerspec itime = {{1,0}, {1,0}};
static struct itimerspec last_itime;

void thread_exit(slist_tparams_t*);
void gracefull_exit(void);
void signal_handler(int);
void *conn_handler(void*);
void timestamp(void);

void thread_exit(slist_tparams_t* threadParams)
{
    syslog(LOG_INFO, "thread_exit\n");
    close(threadParams->threadSock);
    syslog(LOG_INFO, "Closed connection from socket %d\n", threadParams->threadSock);
    threadParams->threadDone = 1;
    pthread_exit((void *)0);
}

void gracefull_exit(void)
{
    close(sockfd);
    close(socket_file_fd);
    remove(WRITE_TO_FILE);
}

void signal_handler(int signal_number)
{
   // bool caught_sigint = false;
   // bool caught_sigterm = false;

    syslog(LOG_INFO, "Caught signal, exiting");
    // if (signal_number == SIGINT){
        // caught_sigint = true;
    // }
    // if (signal_number == SIGTERM){
        // caught_sigterm = true;
    // }
    gracefull_exit();
    exit(0);
}

void *conn_handler(void *threadp)
{
   ssize_t nbytes_socket;
   ssize_t nbytes_file;
   char* buffer_read_ptr;
   bool got_newline;
   long int current_buffer_size = 0;

   // typecast
   slist_tparams_t *g_threadParams = (slist_tparams_t *)threadp;

   g_threadParams->buffer_ptr = malloc(MAX_SIZE);
   if (g_threadParams->buffer_ptr == NULL)
   {
      syslog(LOG_ERR, "[%d] Error:  malloc", g_threadParams->threadIdx);
      thread_exit(g_threadParams);
      return (void *)ERROR;
   }
   current_buffer_size = MAX_SIZE;
   got_newline = false;
   do{
      buffer_read_ptr = g_threadParams->buffer_ptr + current_buffer_size - MAX_SIZE;
      nbytes_socket = recv(g_threadParams->threadSock, buffer_read_ptr, MAX_SIZE, 0);
      if (nbytes_socket < 0)
      {
         if (nbytes_socket == ERROR)
            syslog(LOG_ERR, "[%d] Error: recv", g_threadParams->threadIdx);
         else
            syslog(LOG_INFO, "[%d] Client Disconnected", g_threadParams->threadIdx);
         thread_exit(g_threadParams);
         return (void *)ERROR;
      }

      // syslog(LOG_INFO, "[%d] Received %li bytes from socket %d\n",g_threadParams->threadIdx, nbytes_socket, g_threadParams->threadSock);

      if (buffer_read_ptr[nbytes_socket-1] == '\n')
         got_newline = true;
      else
      {
         if (nbytes_socket == MAX_SIZE)
         {
            syslog(LOG_INFO, "[%d] About to reallocate memory! ", g_threadParams->threadIdx);
            syslog(LOG_INFO, "[%d] Current size = %li, ", g_threadParams->threadIdx, current_buffer_size);
            syslog(LOG_INFO, "[%d] New size = %li\n", g_threadParams->threadIdx, current_buffer_size+MAX_SIZE);
            char* bp_temp = realloc(g_threadParams->buffer_ptr, current_buffer_size+MAX_SIZE);
            if (bp_temp == NULL)
            {
               syslog(LOG_ERR, "[%d] Error: realloc", g_threadParams->threadIdx);
               thread_exit(g_threadParams);
               return (void *)ERROR;
            }
            g_threadParams->buffer_ptr = bp_temp;
            current_buffer_size += MAX_SIZE;
         }
      }
   } while(got_newline == false);

   syslog(LOG_INFO, "[%d] Found end of data symbol, closing connection.\n", g_threadParams->threadIdx);

   if(pthread_mutex_lock(&mutex) == 0)
   {
      int rc_sync = fsync(socket_file_fd);
      if (rc_sync == ERROR)
      {
         // Release Mutex lock
         pthread_mutex_unlock(&mutex);

         syslog(LOG_ERR, "[%d] Error: fsync", g_threadParams->threadIdx);
         thread_exit(g_threadParams);
         return (void *)ERROR;
      }

      nbytes_file = write(socket_file_fd, g_threadParams->buffer_ptr, nbytes_socket + current_buffer_size - MAX_SIZE);
      if (nbytes_file == ERROR)
      {
         // Release Mutex lock
         pthread_mutex_unlock(&mutex);

         syslog(LOG_ERR, "[%d] Error: write", g_threadParams->threadIdx);
         thread_exit(g_threadParams);
         return (void *)ERROR;
      }

      lseek(socket_file_fd, 0, SEEK_SET); // Go to beginning of file
      do
      {
         nbytes_file = read(socket_file_fd,
            g_threadParams->buffer_ptr, current_buffer_size); // Read in up to current_buffer_size bytes
         if (nbytes_file == ERROR)
         {
            syslog(LOG_ERR, "[%d] Couldn't read from file %s\n", g_threadParams->threadIdx, WRITE_TO_FILE);
            thread_exit(g_threadParams);
            return (void *)ERROR;
         }
         syslog(LOG_INFO, "[%d] Got %li bytes from file\n", g_threadParams->threadIdx, nbytes_file);
         send(g_threadParams->threadSock, g_threadParams->buffer_ptr,
            nbytes_file, 0); // Send back nbytes_file to socket
      } while (nbytes_file > 0);
         // Release Mutex lock
         pthread_mutex_unlock(&mutex);
   }
   else
      syslog(LOG_ERR, "[%d] Error: MUTEX\n", g_threadParams->threadIdx);

   remove(WRITE_TO_FILE);
   thread_exit(g_threadParams);

   return (void *)OK;
}

void timestamp(void)
{
   ssize_t nbytes_file;
   time_t rawtime;
   char ts_string[] = "timestamp:";
   char time_string[] = "0000/00/00 00:00:00";
   char result[31];
   int rc_sync;

   // Create Timestamp string
   time(&rawtime);
   strftime(time_string, sizeof(time_string), "%Y/%m/%d %H:%M:%S", localtime(&rawtime));
   strcpy(result, ts_string);    // Copy str1 into result
   strcat(result, time_string);  // Concatenate str2 to result
   strcat(result, "\n");         // Add a newline to result

   // Write Timestamp
  if(pthread_mutex_lock(&mutex) == 0)
   {
      rc_sync = fsync(socket_file_fd);
      if (rc_sync == ERROR)
      {
         // Release Mutex lock
         pthread_mutex_unlock(&mutex);
         // Log error
         syslog(LOG_ERR, "Error: fsync");
      }

      nbytes_file = write(socket_file_fd, result, sizeof(result));
      syslog(LOG_INFO, "%s", result);
      if (nbytes_file == ERROR)
      {
         // Release Mutex lock
         pthread_mutex_unlock(&mutex);
         // Log error
         syslog(LOG_ERR, "Error: write");
      }
      // Release Mutex lock
      pthread_mutex_unlock(&mutex);
   }
   else
      syslog(LOG_ERR, "Error: MUTEX\n");
}

int main(int argc, char* argv[])
{
   bool is_daemon = false;
   int conn_curr;
   int conn_count;
   int rc = 0;
   int stdin_fd;
   int stdouterr_fd;
   struct sigaction act;
   struct sockaddr_in serv_addr;
   slist_tparams_t *conn_threadParams = NULL;
   slist_tparams_t *each_threadParams = NULL;

   // initialize the mutex
   pthread_mutexattr_init(&mutex_attr);
   pthread_mutex_init(&mutex, &mutex_attr);

   // initialize the SLIST
   SLIST_HEAD(list_params_head, threadParams_s) tparams_head;
   SLIST_INIT(&tparams_head);

   // Input arguments
   if (argc == 2)
   {
      if (strncmp(argv[1], "-d", 20) == 0)
         is_daemon = true;
   }

   //Open SYSLOG
   openlog(NULL, LOG_NDELAY, LOG_USER);

   //SIG handler
   memset(&act, 0, sizeof(act));
   act.sa_handler = signal_handler;
   if (sigaction(SIGINT, &act, NULL) == ERROR){
      syslog(LOG_ERR, "Error: sigaction-SIGINT");
      return ERROR;
   }
   if (sigaction(SIGTERM, &act, NULL) == ERROR){
      syslog(LOG_ERR, "Error: sigaction-SIGTERM");
      return ERROR;
   }

   //Setup socket
   sockfd = socket(AF_INET, SOCK_STREAM, 0);
   fcntl(sockfd, F_SETFL, O_NONBLOCK); // Change the socket into non-blocking state
   if (sockfd == ERROR){
      syslog(LOG_ERR, "Error: socket");
      return ERROR;
   }
   if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int)) < 0)
   {
      syslog(LOG_ERR, "Error: setsockopt");
      return ERROR;
   }
   serv_addr.sin_family = AF_INET;
   serv_addr.sin_addr.s_addr = INADDR_ANY;
   serv_addr.sin_port = htons(USER_PORT);
   if (bind(sockfd, (struct sockaddr*) &serv_addr, sizeof(serv_addr)) == ERROR)
   {
      syslog(LOG_ERR, "Error: Bind");
      return ERROR;
   }

   //Check if daemon
   if(is_daemon)
   {
      pid_t pid = fork();
      if (pid == ERROR)
      {
         syslog(LOG_ERR, "Error: fork");
         close(sockfd);
         return ERROR;
      }
      if (pid != 0)
         exit(0);

      pid_t sessionid = setsid();
      if (sessionid == ERROR)
      {
         syslog(LOG_ERR, "Error: setsid");
         close(sockfd);
         return ERROR;
      }
      if (chdir("/") == ERROR)
      {
         syslog(LOG_ERR, "Error: chdir");
         close(sockfd);
         return ERROR;
      }

      stdin_fd = open("/dev/null", O_RDONLY);
      stdouterr_fd = open("/dev/null", O_WRONLY);
      dup2(stdin_fd, STDIN_FILENO);
      dup2(stdouterr_fd, STDOUT_FILENO);
      dup2(stdouterr_fd, STDERR_FILENO);
      close(stdin_fd);
      close(stdouterr_fd);
   }

   // Create Timestamp interval time
   itime.it_interval.tv_sec = TIMESTAMP_INTERVAL;
   itime.it_interval.tv_nsec = 0;
   itime.it_value.tv_sec = TIMESTAMP_INTERVAL;
   itime.it_value.tv_nsec = 0;
   // set up to signal SIGALRM if timer expires
   timer_create(CLOCK_REALTIME, NULL, &timer_1);
   timer_settime(timer_1, 0, &itime, &last_itime);
   signal(SIGALRM, (void(*)()) timestamp); //Call Timestamp every 10s

   //
   if (listen(sockfd, MAX_CONNECTIONS) == ERROR)
   {
      syslog(LOG_ERR, "Error: Max number of connections reached.");
      close(sockfd);
      return ERROR;
   }

   socket_file_fd = open(WRITE_TO_FILE, O_RDWR | O_CREAT | O_APPEND, 0644);
   if (socket_file_fd == ERROR)
   {
      syslog(LOG_ERR, "Error: File");
      return ERROR;
   }

   conn_count = 0;
   while(1)
   {
      struct sockaddr_in connect_address;
      socklen_t addrlen = sizeof(connect_address);
      conn_curr = accept(sockfd, (struct sockaddr*) &connect_address, &addrlen);

      if(conn_curr == -1)
      {
         if(errno == EWOULDBLOCK)
         {
            //Check if SLIST isnt empty
            if(!SLIST_EMPTY(&tparams_head))
            {
               //Check is any thread is done
               each_threadParams = malloc(sizeof(slist_tparams_t));
               SLIST_FOREACH(each_threadParams, &tparams_head, entries){
                  if(each_threadParams->threadDone == 1)
                  {
                     if(pthread_join(each_threadParams->thread, NULL) == 0)
                     {
                        syslog(LOG_INFO, "Closed thread: %d\n", each_threadParams->threadIdx);
                        free(each_threadParams->buffer_ptr);
                        each_threadParams->buffer_ptr = NULL;
                        each_threadParams->threadDone = 0;
                     }
                     else
                        syslog(LOG_ERR, "Error: pthread_join from thread: %d\n", each_threadParams->threadIdx);
                     SLIST_REMOVE(&tparams_head, each_threadParams, threadParams_s, entries);
                  }
               }
               free(each_threadParams);
            }
         }
         else
         {
            syslog(LOG_ERR, "Error: Socket Accept");
            return ERROR;
         }
      }
      else
      {
         conn_threadParams = malloc(sizeof(slist_tparams_t));
         // Copy to struct
         conn_threadParams->threadIdx = conn_count;
         conn_threadParams->threadDone = 0;
         conn_threadParams->threadSock = conn_curr;

         // Debug SYSLOG
         // syslog(LOG_INFO, "Accepted connection from: %s\n", inet_ntoa(connect_address.sin_addr));
         // syslog(LOG_INFO, "Thread ID: %d\n", conn_count);
         // syslog(LOG_INFO, "Socket descriptor: %d\n", conn_threadParams->threadSock);

         // Append to SLIST
         SLIST_INSERT_HEAD(&tparams_head, conn_threadParams, entries);

         // Create Thread
         rc = pthread_create(
            &(SLIST_FIRST(&tparams_head)->thread)  , // pointer to thread descriptor
            NULL                                   , // attributes
            conn_handler                           , // thread function entry point
            (void *)SLIST_FIRST(&tparams_head)       // parameters to pass in
         );
         if(rc < 0)
         {
            syslog(LOG_ERR, "Error: pthread_create");
            return ERROR;
         }
         else
         {
            syslog(LOG_INFO, "pthread_create successful for thread: %d\n", conn_threadParams->threadIdx);
         }
         conn_count++;
      }
   }

   gracefull_exit();
   syslog(LOG_INFO, "exit successful");
   return OK;
}
