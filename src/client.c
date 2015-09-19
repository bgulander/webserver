#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netdb.h>
#include <errno.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <http_parser.h>
#include <sys/wait.h>

#include "client.h"
#include "datetime.h"
#include "parser.h"

#define MAX_SZ 8192
#define READ_SZ 4096
#define MAX_QUERY_SZ 255

const char *HTTP_RESPONSE_TEMPLATE =
"HTTP/1.1 %d %s\r\n"
"Server: webhttpd/1.0\r\n"
"Connection: close\r\n"
"Content-Length %ld\r\n"
"Date: %s\r\n";

const char *HTTP_RESPONSE_404 =
"HTTP/1.1 404 Not Found\r\n"
"Content-Type: text/html\r\n"
"\r\n"
"<HTML><TITLE>404</TITLE>\r\n"
"<BODY><P>404 - Page Not Found</P></BODY>\r\n"
"</HTML>\r\n"
"\r\n";

const char *HTTP_RESPONSE_501 =
"HTTP/1.1 501 Method Not Implemented\r\n"
"Content-Type: text/html\r\n"
"\r\n"
"<HTML><TITLE>501</TITLE>\r\n"
"<BODY><P>501 - Method Not Implemented</P></BODY>\r\n"
"</HTML>\r\n"
"\r\n";

const char *HTTP_RESPONSE_500 =
"HTTP/1.1 500 Internal Server Error\r\n"
"Content-Type: text/html\r\n"
"\r\n"
"<HTML><TITLE>500</TITLE>\r\n"
"<BODY><P>500 - Internal Server Error</P></BODY>\r\n"
"</HTML>\r\n"
"\r\n";


void get_peer_information(Client *client)
{
    socklen_t len;
    struct sockaddr_storage addr;
    char ipstr[INET6_ADDRSTRLEN];

    len = sizeof(addr);
    getpeername(client->msgsock, (struct sockaddr*)&addr, &len);

    // deal with both IPv4 and IPv6:
    if (addr.ss_family == AF_INET) {
        struct sockaddr_in *s = (struct sockaddr_in *)&addr;
        inet_ntop(AF_INET, &s->sin_addr, ipstr, sizeof ipstr);
    } else { // AF_INET6
        struct sockaddr_in6 *s = (struct sockaddr_in6 *)&addr;
        inet_ntop(AF_INET6, &s->sin6_addr, ipstr, sizeof ipstr);
    }

    strcpy(client->ipstr, ipstr);
}


char *read_file(char *path, size_t *file_size)
{
	int fd;


	// open file
	fd = open(path, O_RDONLY);
	if (fd < 0)
	{
		perror("open");
		return NULL;
	}

	// read file
	int sz;
	char *file_buffer = NULL;
	char strbuf[READ_SZ];
	*file_size = 0;

	memset(strbuf, 0, READ_SZ);
	while ((sz = read(fd, strbuf, READ_SZ)) != 0)
	{
		if (sz < 0)
		{
			perror("read");
			return NULL;
		}

		// append
		file_buffer = realloc(file_buffer, *file_size + sz);
		memcpy(file_buffer + *file_size, strbuf, sz);
		*file_size += sz;

		memset(strbuf, 0, READ_SZ);
	}
	close(fd);

	return file_buffer;
}

char *read_file_stdio(char *path, size_t *file_size)
{
	FILE *fp = NULL;


	// open file
	fp = fopen(path, "r");
	if (fp == NULL)
	{
		perror("open");
		return NULL;
	}

	// read file
	int sz;
	char *file_buffer = NULL;
	char strbuf[READ_SZ];
	*file_size = 0;

	memset(strbuf, 0, READ_SZ);
	while (fgets(strbuf, READ_SZ, fp) != NULL)
	{

		// append
        sz = strlen(strbuf);
		file_buffer = realloc(file_buffer, *file_size + sz + 1);
		memcpy(file_buffer + *file_size, strbuf, sz);
		*file_size += sz;
        file_buffer[*file_size] = 0;


		memset(strbuf, 0, READ_SZ);
	}
	fclose(fp);

	return file_buffer;
}

int read_http_request(Client *client)
{
	int sz;
	int rc;
	char buffer[MAX_SZ];


	memset(buffer, 0, MAX_SZ);

	sz = read(client->msgsock, buffer, MAX_SZ);
	printf("IP: %s\n", client->ipstr);
	printf("RECV:\n----------\n%s\n----------\n", buffer);

	// get http header
	http_header_t *header = (http_header_t *) malloc (sizeof(http_header_t));

	// parse http header
	rc = parse(header, buffer, sz);

	client->header = header;

	return sz;
}


int http_response_get(Client *client)
{
    int rc;
    int http_code = 0;
    char datetime[MAX_DATETIME_LENGTH];
    char path[MAX_SZ];
    char *file_buffer = NULL;
    size_t file_size;

    // get datetime
	memset(datetime, 0, MAX_DATETIME_LENGTH);
	rc = get_datetime(datetime);

    // get html path
    sprintf(path, "html%s", client->header->url);
    printf("HTTP_PATH: %s\n", path);

    // try to read file
    file_buffer = read_file(path, &file_size);

    // page not found
    if (file_buffer == NULL)
    {
        http_code = 404;
        file_buffer = read_file("html/404.html", &file_size);

        // NO! We should have 404 page sit somewhere
        if (file_buffer == NULL)
        {
            write(client->msgsock, HTTP_RESPONSE_404, strlen(HTTP_RESPONSE_404));
            printf("HTTP_CODE: 501\n");
            return http_code;
        }

    }

    http_code = 200;

    // response http_packet
    char buf[file_size + MAX_DATETIME_LENGTH + strlen(HTTP_RESPONSE_TEMPLATE) * 2];
    size_t readable_length;

    memset(buf, 0, sizeof(buf));

    // create http header
    sprintf(buf,
        HTTP_RESPONSE_TEMPLATE, // template
        http_code,
        get_http_code_description(http_code),
        file_size,
        datetime);

    strncat(buf, "\r\n", 2);

    readable_length = strlen(buf);

    // append binary data (possible) to the end of header
    memcpy(buf + readable_length, file_buffer, file_size);

    // append \r\n at the end
    memcpy(buf + readable_length + file_size, "\r\n", 3);

    write(client->msgsock, buf, sizeof(buf));


    // safely free file buffer
    if (file_buffer != NULL)
    {
        free(file_buffer);
    }

    return http_code;
}

char *execute_cgi(char *path, size_t *output_size, Client *client)
{
    int input_fd[2];
    int output_fd[2];
    int pid;
    int status;
    char *output_buffer = NULL;
    int content_length = -1;

    // get content length
    for (int i = 0; i < client->header->num_fields; i++)
    {
        if (strcasecmp(client->header->fields[i], "Content-Length") == 0)
        {
            content_length = atoi(client->header->values[i]);
            break;
        }
    }
    if (content_length == -1)
    {
        fprintf(stderr, "Unable to http request header error: not a valid Content-Length");
        return NULL;
    }

    // open pipe for executing cgi script
    if (pipe(input_fd) < 0 || pipe(output_fd) < 0)
    {
        perror("pipe");
        return NULL;
    }

    pid = fork();
    if (pid < 0)
    {
        perror("fork");
        return NULL;
    }
    else if (pid == 0)
    {
        // child code
        char method_env[MAX_QUERY_SZ];
        char query_env[MAX_QUERY_SZ];
        char length_env[MAX_QUERY_SZ];

        // copy query
        // TODO: GET and POST are able to execute cgi script. I need to extract query
        // start with ?
        sprintf(method_env, "REQUEST_METHOD=%s", http_method_str(client->header->method));
        putenv(method_env);

        
        sprintf(query_env, "QUERY_STRING=%s", "");
        putenv(query_env);

        sprintf(length_env, "CONTENT_LENGTH=%d", content_length);
        putenv(length_env);


        // redirect pipe
        dup2(output_fd[1], STDOUT_FILENO);
        dup2(input_fd[0], STDIN_FILENO);
        close(output_fd[0]);
        close(input_fd[1]);

        execl(path, path, NULL);
        perror("execl");
        exit(-1);
    }
    else
    {
        // parent code
        char strbuf[READ_SZ];
    	*output_size = 0;
        int sz;

        // close pipe
        close(output_fd[1]);
        close(input_fd[0]);

        // write form data to CGI
        write(input_fd[1], client->header->body, strlen(client->header->body));

        // wait for child to complete
        waitpid(pid, &status, 0);

    	memset(strbuf, 0, READ_SZ);
    	while ((sz = read(output_fd[0], strbuf, READ_SZ)) != 0)
    	{
    		if (sz < 0)
    		{
    			perror("read");
    			return NULL;
    		}

    		// append
    		output_buffer = realloc(output_buffer, *output_size + sz);
    		memcpy(output_buffer + *output_size, strbuf, sz);
    		*output_size += sz;

    		memset(strbuf, 0, READ_SZ);
    	}
    	close(output_fd[0]);
        close(input_fd[1]);
    }

    return output_buffer;

}
int http_response_post(Client *client)
{
    int rc;
    int http_code = 0;
    char datetime[MAX_DATETIME_LENGTH];
    char path[MAX_SZ];
    char *output_buffer = NULL;
    size_t output_size;

    // get datetime
	memset(datetime, 0, MAX_DATETIME_LENGTH);
	rc = get_datetime(datetime);

    // get cgi path
    sprintf(path, "html%s", client->header->url);
    printf("CGI_PATH: %s\n", path);

    // execute cgi script
    output_buffer = execute_cgi(path, &output_size, client);

    // failed to execute
    if (output_buffer == NULL)
    {
        http_code = 500;
        write(client->msgsock, HTTP_RESPONSE_500, strlen(HTTP_RESPONSE_500));
        printf("HTTP_CODE: 500\n");
        return http_code;
    }

    http_code = 200;

    // response http_packet
    char buf[output_size + MAX_DATETIME_LENGTH + strlen(HTTP_RESPONSE_TEMPLATE) * 2];
    size_t readable_length;

    memset(buf, 0, sizeof(buf));

    // create http header
    sprintf(buf,
        HTTP_RESPONSE_TEMPLATE, // template
        http_code,
        get_http_code_description(http_code),
        output_size,
        datetime);

    // header may not yet complete so we dont need to terminate header
    //strncat(buf, "\r\n", 2);

    readable_length = strlen(buf);

    // append binary data (possible) to the end of header
    memcpy(buf + readable_length, output_buffer, output_size);

    // append \r\n at the end
    memcpy(buf + readable_length + output_size, "\r\n", 3);

    write(client->msgsock, buf, sizeof(buf));


    // safely free file buffer
    if (output_buffer != NULL)
    {
        free(output_buffer);
    }

    return http_code;
}

int http_response_default(Client *client)
{
    int http_code = 0;

    http_code = 501;
    write(client->msgsock, HTTP_RESPONSE_501, strlen(HTTP_RESPONSE_501));
    printf("501 Method Not Implemented\n");
    return http_code;
}

int make_http_response(Client *client)
{
	// serve file
    switch (client->header->method)
    {

    	case 1: // 1 is GET
    	{
            http_response_get(client);
            break;
        }

        case 3: // 3 is POST
        {
            http_response_post(client);
            break;
        }

        default: // unimplemented
        {
            http_response_default(client);
            break;
        }

	}

	return 0;
}
