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
#include <sys/wait.h>
#include <errno.h>

#include "client.h"
#include "datetime.h"
#include "parser.h"
#include "config.h"
#include "response.h"
#include "http_parser.h"
#include "cgi.h"


char *execute_python(char *path, size_t *output_size, Configuration *config, Client *client)
{
    int                 input_fd[2];
    int                 output_fd[2];
    int                 pid;
    int                 status;
    char                *output_buffer = NULL;
    int                 content_length = 0;
    int                 method;

    // HTTP get or post
    method = client->header->method;

    // get content length
    for (int i = 0; i < client->header->num_fields; i++)
    {
        if (strcasecmp(client->header->fields[i], "Content-Length") == 0)
        {
            content_length = atoi(client->header->values[i]);
            break;
        }
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
        char dir_env[MAX_QUERY_SZ];

        // get current working directory
        char default_dir[MAX_VALUE_LEN];
        memset(default_dir, 0, MAX_VALUE_LEN);
        config_get_value(config, "default_dir", default_dir);

        sprintf(method_env, "REQUEST_METHOD=%s", http_method_str(client->header->method));
        putenv(method_env);

        sprintf(query_env, "QUERY_STRING=%s", client->header->url);
        putenv(query_env);

        sprintf(length_env, "CONTENT_LENGTH=%d", content_length);
        putenv(length_env);

        sprintf(dir_env, "WORKING_DIR=%s", default_dir);
        putenv(dir_env);


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
        ssize_t sz;

        // close pipe
        close(output_fd[1]);
        close(input_fd[0]);

        // write form data to CGI
        // write(input_fd[1], client->header->body, strlen(client->header->body));
        if (method == 3) // only post will write data to it
        {
            write(input_fd[1], client->payload, strlen(client->payload));
        }


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
