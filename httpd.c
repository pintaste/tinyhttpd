/* J. David's webserver */
/* This is a simple webserver.
 * Created November 1999 by J. David Blackstone.
 * CSE 4344 (Network concepts), Prof. Zeigler
 * University of Texas at Arlington
 */
/* This program compiles for Sparc Solaris 2.6.
 * To compile for Linux:
 *  1) Comment out the #include <pthread.h> line.
 *  2) Comment out the line that defines the variable newthread.
 *  3) Comment out the two lines that run pthread_create().
 *  4) Uncomment the line that runs accept_request().
 *  5) Remove -lsocket from the Makefile.
 */
#include <stdio.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <ctype.h>
#include <strings.h>
#include <string.h>
#include <sys/stat.h>
#include <pthread.h>
#include <sys/wait.h>
#include <stdlib.h>

// macro to check if the character is a space
#define ISspace(x) isspace((int)(x))

#define SERVER_STRING "Server: jdbhttpd/0.1.0\r\n"

// when receiving a request, then create a new thread to handle it
void accept_request(int);

void bad_request(int);

// read the file
void cat(int, FILE *);

void cannot_execute(int);

// error message
void error_die(const char *);

// execute the cgi script
void execute_cgi(int, const char *, const char *, const char *);

// implement the get_line function
int get_line(int, char *, int);

// return the HTTP header
void headers(int, const char *);

void not_found(int);

// serve the file, if the file is not GCI, then just read the file and send it to the client
void serve_file(int, const char *);

// start the tcp connection, and listen to the port
int startup(u_short *);

// if the request is not GET or POST, then print the error message
void unimplemented(int);

/**********************************************************************/
/* A request has caused a call to accept() on the server port to
 * return.  Process the request appropriately.
 * Parameters: the socket connected to the client */
/**********************************************************************/
void accept_request(int client)
{
  // socket
  char buf[1024];
  int numchars;
  char method[255];
  char url[255];
  char path[512];
  size_t i, j;
  struct stat st;
  int cgi = 0; /* becomes true if server decides this is a CGI program */
  char *query_string = NULL;
  // handle the first line of the http request
  // GET /index.html HTTP/1.1
  numchars = get_line(client, buf, sizeof(buf));
  i = 0;
  j = 0;
  // copy the method from the request to the method variable
  while (!ISspace(buf[j]) && (i < sizeof(method) - 1))
  {
    method[i] = buf[j];
    i++;
    j++;
  }
  method[i] = '\0';

  // if the method is not GET or POST, then return
  // strcasecmp is a function that compares two strings, ignoring the case
  if (strcasecmp(method, "GET") && strcasecmp(method, "POST"))
  {
    unimplemented(client);
    return;
  }

  if (strcasecmp(method, "POST") == 0)
    cgi = 1;

  i = 0;

  // skip the space in the buffer
  while (ISspace(buf[j]) && (j < sizeof(buf)))
  {
    j++;
  }

  // copy the url from the buf to the url variable
  // if the url is http://localhost:34347/index.html
  // then the http request is GET /index.html HTTP/1.1
  // so the url is /index.html
  while (!ISspace(buf[j]) && (i < sizeof(url) - 1) && (j < sizeof(buf)))
  {
    url[i] = buf[j];
    i++;
    j++;
  }
  url[i] = '\0';

  // if the method is GET, then the url may contain the query string
  if (strcasecmp(method, "GET") == 0)
  {
    query_string = url;
    while ((*query_string != '?') && (*query_string != '\0'))
      query_string++;
    if (*query_string == '?')
    {
      cgi = 1;
      *query_string = '\0';
      query_string++;
    }
  }

  // create the path
  sprintf(path, "htdocs%s", url);

  // if the path is /, then add the index.html to the path
  if (path[strlen(path) - 1] == '/')
  {
    strcat(path, "index.html");
  }

  // check if the file exists
  if (stat(path, &st) == -1)
  {
    // read the all info from the http header and discard it
    while ((numchars > 0) && strcmp("\n", buf)) /* read & discard headers */
    {
      numchars = get_line(client, buf, sizeof(buf));
    }

    not_found(client);
  }
  else
  {
    if ((st.st_mode & S_IFMT) == S_IFDIR)
    {
      strcat(path, "/index.html");
    }
    // if the file is executable, then it will consider as a cgi script
    if ((st.st_mode & S_IXUSR) ||
        (st.st_mode & S_IXGRP) ||
        (st.st_mode & S_IXOTH))
      cgi = 1;
    if (!cgi)
    {
      // serve the htdocs/index.html file
      serve_file(client, path);
    }
    else
    {
      // execute the cgi script
      execute_cgi(client, path, method, query_string);
    }
  }
  // close the client socket
  close(client);
}

/**********************************************************************/
/* Inform the client that a request it has made has a problem.
 * Parameters: client socket */
/**********************************************************************/
void bad_request(int client)
{
  char buf[1024];

  sprintf(buf, "HTTP/1.0 400 BAD REQUEST\r\n");
  send(client, buf, sizeof(buf), 0);
  sprintf(buf, "Content-type: text/html\r\n");
  send(client, buf, sizeof(buf), 0);
  sprintf(buf, "\r\n");
  send(client, buf, sizeof(buf), 0);
  sprintf(buf, "<P>Your browser sent a bad request, ");
  send(client, buf, sizeof(buf), 0);
  sprintf(buf, "such as a POST without a Content-Length.\r\n");
  send(client, buf, sizeof(buf), 0);
}

/**********************************************************************/
/* Put the entire contents of a file out on a socket.  This function
 * is named after the UNIX "cat" command, because it might have been
 * easier just to do something like pipe, fork, and exec("cat").
 * Parameters: the client socket descriptor
 *             FILE pointer for the file to cat */
/**********************************************************************/
void cat(int client, FILE *resource)
{
  char buf[1024];

  fgets(buf, sizeof(buf), resource);
  while (!feof(resource))
  {
    send(client, buf, strlen(buf), 0);
    fgets(buf, sizeof(buf), resource);
  }
}

/**********************************************************************/
/* Inform the client that a CGI script could not be executed.
 * Parameter: the client socket descriptor. */
/**********************************************************************/
void cannot_execute(int client)
{
  char buf[1024];

  sprintf(buf, "HTTP/1.0 500 Internal Server Error\r\n");
  send(client, buf, strlen(buf), 0);
  sprintf(buf, "Content-type: text/html\r\n");
  send(client, buf, strlen(buf), 0);
  sprintf(buf, "\r\n");
  send(client, buf, strlen(buf), 0);
  sprintf(buf, "<P>Error prohibited CGI execution.\r\n");
  send(client, buf, strlen(buf), 0);
}

/**********************************************************************/
/* Print out an error message with perror() (for system errors; based
 * on value of errno, which indicates system call errors) and exit the
 * program indicating an error. */
/**********************************************************************/
void error_die(const char *sc)
{
  perror(sc);
  exit(1);
}

/**********************************************************************/
/* Execute a CGI script.  Will need to set environment variables as
 * appropriate.
 * Parameters: client socket descriptor
 *             path to the CGI script */
/**********************************************************************/
void execute_cgi(int client, const char *path,
                 const char *method, const char *query_string)
{
  char buf[1024];
  // two pipes
  // one for the parent to write to the child
  // one for the child to write to the parent
  int cgi_output[2];
  int cgi_input[2];
  // process id and status
  pid_t pid;
  int status;

  int i;
  char c;

  // number of characters read
  int numchars = 1;

  // content length of the http header
  int content_length = -1;

  buf[0] = 'A';  // just to make sure it is not empty
  buf[1] = '\0'; // just to make sure it is not empty

  if (strcasecmp(method, "GET") == 0)
  {
    while ((numchars > 0) && strcmp("\n", buf))
    { /* read & discard headers */
      numchars = get_line(client, buf, sizeof(buf));
    }
  }
  else /* POST */
  {
    numchars = get_line(client, buf, sizeof(buf));
    while ((numchars > 0) && strcmp("\n", buf))
    {
      // get the content length from the http header
      // the Content-Length is fixed to 15 characters
      // so the 16th character is null terminator.
      buf[15] = '\0';
      if (strcasecmp(buf, "Content-Length:") == 0)
      {
        // convert the string to integer from the 17th character
        content_length = atoi(&(buf[16]));
      }
      numchars = get_line(client, buf, sizeof(buf));
    }
    if (content_length == -1)
    {
      bad_request(client);
      return;
    }
  }

  sprintf(buf, "HTTP/1.0 200 OK\r\n");
  send(client, buf, strlen(buf), 0);

  // create output pipe
  if (pipe(cgi_output) < 0)
  {
    cannot_execute(client);
    return;
  }

  // create input pipe
  if (pipe(cgi_input) < 0)
  {
    cannot_execute(client);
    return;
  }

  // fork a child process to execute the cgi script
  // parent process will receive the output from the child process
  if ((pid = fork()) < 0)
  {
    cannot_execute(client);
    return;
  }

  if (pid == 0) /* child: CGI script */
  {
    char meth_env[255];
    char query_env[255];
    char length_env[255];

    // child process will write to the parent process
    dup2(cgi_output[1], 1);
    // child process will read from the parent process
    dup2(cgi_input[0], 0);

    // close the unused pipes
    close(cgi_output[0]);
    close(cgi_input[1]);

    // set the environment variables
    sprintf(meth_env, "REQUEST_METHOD=%s", method);
    putenv(meth_env);
    if (strcasecmp(method, "GET") == 0)
    {
      sprintf(query_env, "QUERY_STRING=%s", query_string);
      putenv(query_env);
    }
    else
    { /* POST */
      sprintf(length_env, "CONTENT_LENGTH=%d", content_length);
      putenv(length_env);
    }

    // execute the cgi script
    execl(path, path, NULL);
    // if the execl function returns, it means it failed
    // for example, the html file is excutable, the return value will be -1
    // when the child process will exit but the parent process will still write to the child process, the error "Broken pipe" will be raised
    exit(0);
  }
  else
  { /* parent */
    // close the unused pipes
    close(cgi_output[1]);
    close(cgi_input[0]);
    if (strcasecmp(method, "POST") == 0)
      for (i = 0; i < content_length; i++)
      {
        // read the post data from the client and send to the child process via the input pipe
        recv(client, &c, 1, 0);
        write(cgi_input[1], &c, 1);
      }

    while (read(cgi_output[0], &c, 1) > 0)
    {
      // read the output from the child process and send to the client
      send(client, &c, 1, 0);
    }

    // close the pipes
    close(cgi_output[0]);
    close(cgi_input[1]);

    // wait for the child process to exit
    waitpid(pid, &status, 0);
  }
}

/**********************************************************************/
/* Get a line from a socket, whether the line ends in a newline,
 * carriage return, or a CRLF combination.  Terminates the string read
 * with a null character.  If no newline indicator is found before the
 * end of the buffer, the string is terminated with a null.  If any of
 * the above three line terminators is read, the last character of the
 * string will be a linefeed and the string will be terminated with a
 * null character.
 * Parameters: the socket descriptor
 *             the buffer to save the data in
 *             the size of the buffer
 * Returns: the number of bytes stored (excluding null) */
/**********************************************************************/
int get_line(int sock, char *buf, int size)
{
  int i = 0;
  char c = '\0';
  int n;

  while ((i < size - 1) && (c != '\n'))
  {
    n = recv(sock, &c, 1, 0);
    /* DEBUG printf("%02X\n", c); */
    if (n > 0)
    {
      if (c == '\r')
      {
        // if the next character is a newline, read and discard it
        n = recv(sock, &c, 1, MSG_PEEK);
        /* DEBUG printf("%02X\n", c); */
        if ((n > 0) && (c == '\n'))
        {
          // read and discard the newline
          recv(sock, &c, 1, 0);
        }
        else
        {
          // if the next character is not a newline, replace it with a null
          c = '\n';
        }
      }
      buf[i] = c;
      i++;
    }
    else
      c = '\n';
  }
  buf[i] = '\0';

  return (i);
}

/**********************************************************************/
/* Return the informational HTTP headers about a file. */
/* Parameters: the socket to print the headers on
 *             the name of the file */
/**********************************************************************/
void headers(int client, const char *filename)
{
  char buf[1024];
  (void)filename; /* could use filename to determine file type */

  strcpy(buf, "HTTP/1.0 200 OK\r\n");
  send(client, buf, strlen(buf), 0);
  strcpy(buf, SERVER_STRING);
  send(client, buf, strlen(buf), 0);
  sprintf(buf, "Content-Type: text/html\r\n");
  send(client, buf, strlen(buf), 0);
  strcpy(buf, "\r\n");
  send(client, buf, strlen(buf), 0);
}

/**********************************************************************/
/* Give a client a 404 not found status message. */
/**********************************************************************/
void not_found(int client)
{
  char buf[1024];

  sprintf(buf, "HTTP/1.0 404 NOT FOUND\r\n");
  send(client, buf, strlen(buf), 0);
  sprintf(buf, SERVER_STRING);
  send(client, buf, strlen(buf), 0);
  sprintf(buf, "Content-Type: text/html\r\n");
  send(client, buf, strlen(buf), 0);
  sprintf(buf, "\r\n");
  send(client, buf, strlen(buf), 0);
  sprintf(buf, "<HTML><TITLE>Not Found</TITLE>\r\n");
  send(client, buf, strlen(buf), 0);
  sprintf(buf, "<BODY><P>The server could not fulfill\r\n");
  send(client, buf, strlen(buf), 0);
  sprintf(buf, "your request because the resource specified\r\n");
  send(client, buf, strlen(buf), 0);
  sprintf(buf, "is unavailable or nonexistent.\r\n");
  send(client, buf, strlen(buf), 0);
  sprintf(buf, "</BODY></HTML>\r\n");
  send(client, buf, strlen(buf), 0);
}

/**********************************************************************/
/* Send a regular file to the client.  Use headers, and report
 * errors to client if they occur.
 * Parameters: a pointer to a file structure produced from the socket
 *              file descriptor
 *             the name of the file to serve */
/**********************************************************************/
void serve_file(int client, const char *filename)
{
  FILE *resource = NULL;
  int numchars = 1;
  char buf[1024];

  buf[0] = 'A'; // just to have a non-null first character
  buf[1] = '\0';// to ensure the while loop runs at least once
  while ((numchars > 0) && strcmp("\n", buf)) /* read & discard headers */{
    numchars = get_line(client, buf, sizeof(buf));
  }
  resource = fopen(filename, "r");
  if (resource == NULL)
  {
    not_found(client);
  }
  else
  {
    // send the HTTP headers
    headers(client, filename);
    // send the content of the file to the client
    cat(client, resource);
  }
  fclose(resource);
}

/**********************************************************************/
/* This function starts the process of listening for web connections
 * on a specified port.  If the port is 0, then dynamically allocate a
 * port and modify the original port variable to reflect the actual
 * port.
 * Parameters: pointer to variable containing the port to connect on
 * Returns: the socket */
/**********************************************************************/
int startup(u_short *port)
{
  /*
   Notes
   htons:converts the u_short int from network byte order to host byte order.
   htonl: converts the u_long int from network byte order to host byte order.
   INADDR_ANY: an IP address that is used when we don't want to bind a socket to any specific IP.
   netwrok byte order: always big endian in TCP/IP
   host byte order: little endian in x86, big endian in PowerPC
  */
  int httpd = 0;
  struct sockaddr_in name;

  httpd = socket(PF_INET, SOCK_STREAM, 0);  // using IPv4 TCP
  if (httpd == -1)                          // if socket() fails
    error_die("socket");
  memset(&name, 0, sizeof(name));           // zero out the struct
  name.sin_family = AF_INET;                // using IPv4
  name.sin_port = htons(*port);             // set port
  name.sin_addr.s_addr = htonl(INADDR_ANY); // set IP address
  if (bind(httpd, (struct sockaddr *)&name, sizeof(name)) < 0)
    error_die("bind");

  // if port is not specified, dynamically allocate a port
  if (*port == 0) /* if dynamically allocating a port */
  {
    int namelen = sizeof(name);
    if (getsockname(httpd, (struct sockaddr *)&name, &namelen) == -1)
      error_die("getsockname");
    *port = ntohs(name.sin_port);
  }
  // listen for connections, amax number of pending connections is 5
  if (listen(httpd, 5) < 0)
    error_die("listen");
  return (httpd);
}

/**********************************************************************/
/* Inform the client that the requested web method has not been
 * implemented.
 * Parameter: the client socket */
/**********************************************************************/
void unimplemented(int client)
{
  char buf[1024];

  sprintf(buf, "HTTP/1.0 501 Method Not Implemented\r\n");
  send(client, buf, strlen(buf), 0);
  sprintf(buf, SERVER_STRING);
  send(client, buf, strlen(buf), 0);
  sprintf(buf, "Content-Type: text/html\r\n");
  send(client, buf, strlen(buf), 0);
  sprintf(buf, "\r\n");
  send(client, buf, strlen(buf), 0);
  sprintf(buf, "<HTML><HEAD><TITLE>Method Not Implemented\r\n");
  send(client, buf, strlen(buf), 0);
  sprintf(buf, "</TITLE></HEAD>\r\n");
  send(client, buf, strlen(buf), 0);
  sprintf(buf, "<BODY><P>HTTP request method not supported.\r\n");
  send(client, buf, strlen(buf), 0);
  sprintf(buf, "</BODY></HTML>\r\n");
  send(client, buf, strlen(buf), 0);
}

/**********************************************************************/

int main(void)
{
  int server_sock = -1;
  u_short port = 0;
  int client_sock = -1;
  struct sockaddr_in client_name;
  int client_name_len = sizeof(client_name);
  pthread_t newthread;

  // build the server socket and listen for connections
  server_sock = startup(&port);
  printf("httpd running on port %d\n", port);

  while (1)
  {
    // accept function blocks until a client connects to the server
    client_sock = accept(server_sock,
                         (struct sockaddr *)&client_name,
                         &client_name_len);
    if (client_sock == -1)
      error_die("accept");

    accept_request(client_sock); // process the request
    
    // create a new thread to run accept_request function
    if (pthread_create(&newthread, NULL, accept_request, client_sock) != 0)
      perror("pthread_create");
  }

  close(server_sock);

  return (0);
}
