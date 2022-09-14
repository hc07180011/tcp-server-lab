#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <arpa/inet.h>
#include <netinet/in.h>

#include "logger.h"
#include "db.h"

#ifndef INADDR_ANY
#define INADDR_ANY ((in_addr_t) 0x00000000)
#endif
#define ERR_EXIT(a) do { perror(a); exit(1); } while(0)

#define ACK "ack"
#define RST "rst"

typedef struct {
    char hostname[512];  // server's hostname
    unsigned short port;  // port to listen
    int listen_fd;  // fd to wait for a new connection
} server;

typedef struct {
    char host[512];  // client's host
    int conn_fd;  // fd to talk with client
    char buf[512];  // data sent by/to client
    size_t buf_len;  // bytes used by buf
    // you don't need to change this.
    int id; // delete?
    int user_id;
    int wait_for_write;  // used by handle_read to know if the header is read or not.
} request;

server svr;  // server
request* requestP = NULL;  // point to a list of requests
int maxfd;  // size of open file descriptor table, size of request list

char logging_mes[1024];

static void init_server(unsigned short port);
// initailize a server, exit for error

static void init_request(request* reqP);
// initailize a request instance

static void free_request(request* reqP);
// free resources used by a request instance

int handle_read(request* reqP) {
    int r;
    char buf[512];
    memset(buf, 0, sizeof(buf));

    // Read in request from client
    r = read(reqP->conn_fd, buf, sizeof(buf));
    sprintf(logging_mes, "[Recv] %d: `%s`", r, buf);
    logging(DEBUG, logging_mes);

    if (r < 0) {
        sprintf(logging_mes, "`handle_read` error, ERRNO=%d", errno);
        logging(ERROR, logging_mes);
        ERR_EXIT("fd");
        return -1;
    }
    if (r == 0) {
        return 0; // Closing implemented in main()
    }

    memmove(reqP->buf, buf, strlen(buf));
    reqP->buf[strlen(buf)] = '\0';
    reqP->buf_len = strlen(buf);
    return 1;
}

int main(int argc, char** argv) {

    // Parse args.
    if (argc != 2) {
        sprintf(logging_mes, "usage: %s [port]", argv[0]);
        logging(ERROR, logging_mes);
        exit(1);
    }

    int clilen;
    struct sockaddr_in cliaddr;  // used by accept()

    int i;
    int select_ret, pread_ret;
    int query_uid, query_result;

    int conn_fd;  // fd for a new connection with client
    int file_fd;  // fd for file that we open for reading

    char buf[512];

    fd_set rfds;
    struct timeval tv;

    int max_conn_num = 20;
    int read_lock_cnt[max_conn_num], write_lock_cnt[max_conn_num];
    memset(read_lock_cnt, 0, sizeof(read_lock_cnt));
    memset(write_lock_cnt, 0, sizeof(write_lock_cnt));

    int db_fd = open("online.db", O_RDWR | O_CREAT, (mode_t)0755);

    struct flock fl;

    // file_fd = open("registerRecord", O_RDWR);
    // if (file_fd < 0) {
    //     sprintf(logging_mes, "open registerRecord error");
    //     logging(ERROR, logging_mes);
    //     ERR_EXIT("open");
    // }

    // Initialize server
    init_server((unsigned short) atoi(argv[1]));

    // Loop for handling connections
    sprintf(logging_mes, "starting on %.80s, port %d, fd %d, maxconn %d...", svr.hostname, svr.port, svr.listen_fd, maxfd);
    logging(INFO, logging_mes);

    FD_ZERO(&rfds);
    FD_SET(svr.listen_fd, &rfds);

    maxfd = svr.listen_fd;

    fd_set new_rfds;

    while (1) {
        tv.tv_sec = 0;
        tv.tv_usec = 1000;

        new_rfds = rfds;

        select_ret = select(maxfd + 1, &new_rfds, NULL, NULL, &tv);
        if (select_ret == -1) {
            ERR_EXIT("select");
        } else if (select_ret == 0) {
            continue;
        }

        for (i = 0; i <= maxfd; i++) {
            // iterate all fds
            if (FD_ISSET(i, &new_rfds)) {
                // check which fd is triggered
                if (i == svr.listen_fd) {
                    // new connection
                    clilen = sizeof(cliaddr);
                    conn_fd = accept(svr.listen_fd, (struct sockaddr*)&cliaddr, (socklen_t*)&clilen);
                    if (conn_fd < 0) {
                        if (errno == EINTR || errno == EAGAIN) continue;  // try again
                        if (errno == ENFILE) {
                            (void) sprintf(logging_mes, "out of file descriptor table ... (maxconn %d)", maxfd);
                            logging(INFO, logging_mes);
                            continue;
                        }
                        ERR_EXIT("accept");
                    }

                    FD_SET(conn_fd, &rfds);
                    if (conn_fd > maxfd) {
                        maxfd = conn_fd;
                    }

                    requestP[conn_fd].conn_fd = conn_fd;

                    strcpy(requestP[conn_fd].host, inet_ntoa(cliaddr.sin_addr));
                    
                    sprintf(logging_mes, "getting a new request... fd %d from %s", conn_fd, requestP[conn_fd].host);
                    logging(INFO, logging_mes);
                    
                    // Set buf to be the welcoming message
                    sprintf(requestP[conn_fd].buf, ACK);
                    // Pass the message to socket
                    sprintf(logging_mes, "[Send] `%s`", requestP[conn_fd].buf);
                    logging(DEBUG, logging_mes);
                    if (write(requestP[conn_fd].conn_fd, requestP[conn_fd].buf, strlen(requestP[conn_fd].buf)) == -1) {
                        ERR_EXIT("write");
                    }
                    memset(requestP[conn_fd].buf, 0, sizeof(requestP[conn_fd].buf));

                } else {
                    // existing connection

                    conn_fd = i;

                    int ret = handle_read(&requestP[conn_fd]); // parse data from client to requestP[conn_fd].buf
                    if (ret == 0) {
                        sprintf(
                            logging_mes,
                            "Receive zero-length message, closing socket (fd=%d,uid=%d)...",
                            conn_fd, requestP[conn_fd].user_id
                        );
                        logging(INFO, logging_mes);

                        /* !!! TODO: Implement offline */
                        put_online_status(db_fd, requestP[conn_fd].user_id, 0); // update status

                        // close
                        FD_CLR(conn_fd, &rfds);
                        close(requestP[conn_fd].conn_fd);
                        free_request(&requestP[conn_fd]);

                        continue;
                    }
                    if (ret < 0) {
                        sprintf(
                            logging_mes,
                            "bad request from %s (fd=%d|%d,uid=%d) ",
                            requestP[conn_fd].host, conn_fd, requestP[conn_fd].conn_fd, requestP[conn_fd].user_id
                        );
                        logging(INFO, logging_mes);

                        // already closed, send RST
                        sprintf(requestP[conn_fd].buf, RST);
                        write(requestP[conn_fd].conn_fd, requestP[conn_fd].buf, strlen(requestP[conn_fd].buf));

                        continue;
                    }

                    /*** ===== Core logic here ===== ***/

                    printf("Got `%s`\n", requestP[conn_fd].buf);

                    if (!strncmp(requestP[conn_fd].buf, "put", 3)) {
                        requestP[conn_fd].user_id = atoi(requestP[conn_fd].buf + 4);
                        printf("user id: %d\n", requestP[conn_fd].user_id);

                        put_online_status(db_fd, requestP[conn_fd].user_id, 1); // update status

                        sprintf(requestP[conn_fd].buf, ACK); // dump outputs

                    } else if (!strncmp(requestP[conn_fd].buf, "get", 3)) {
                        query_uid = atoi(requestP[conn_fd].buf + 4);
                        printf("query uid: %d\n", query_uid);

                        query_result = (int)get_online_status(db_fd, query_uid); // get status

                        sprintf(requestP[conn_fd].buf, "%s %d", ACK, query_result); // dump outputs

                    } else {
                        sprintf(logging_mes, "Unknown format: `%s`", requestP[conn_fd].buf);
                        logging(WARNING, logging_mes);

                        sprintf(requestP[conn_fd].buf, RST); // dump outputs
                    }

                    /************************************/

                    sprintf(logging_mes, "[Send] `%s`", requestP[conn_fd].buf);
                    logging(DEBUG, logging_mes);
                    if (write(requestP[conn_fd].conn_fd, requestP[conn_fd].buf, strlen(requestP[conn_fd].buf)) == -1) {
                        ERR_EXIT("write");
                    }
                    memset(requestP[conn_fd].buf, 0, sizeof(requestP[conn_fd].buf));
                }
            }
        }
    }

    free(requestP);
    return 0;
}

// ================================

static void init_request(request* reqP) {
    reqP->conn_fd = -1;
    reqP->buf_len = 0;
    reqP->id = 0;
    reqP->user_id = -1;
}

static void free_request(request* reqP) {
    /*if (reqP->filename != NULL) {
        free(reqP->filename);
        reqP->filename = NULL;
    }*/
    init_request(reqP);
}

static void init_server(unsigned short port) {
    struct sockaddr_in servaddr;
    int tmp;

    gethostname(svr.hostname, sizeof(svr.hostname));
    svr.port = port;

    svr.listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (svr.listen_fd < 0) ERR_EXIT("socket");

    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(port);
    tmp = 1;
    if (setsockopt(svr.listen_fd, SOL_SOCKET, SO_REUSEADDR, (void*)&tmp, sizeof(tmp)) < 0) {
        ERR_EXIT("setsockopt");
    }
    if (bind(svr.listen_fd, (struct sockaddr*)&servaddr, sizeof(servaddr)) < 0) {
        ERR_EXIT("bind");
    }
    if (listen(svr.listen_fd, 1024) < 0) {
        ERR_EXIT("listen");
    }

    // Get file descripter table size and initialize request table
    /*
    永久设定:
    修过或在/etc/security/limits.conf添加：
    * soft nofile 100000
    * hard nofile 100000
    */
    maxfd = getdtablesize(); // macOS: 61440
    requestP = (request*) malloc(sizeof(request) * maxfd);
    if (requestP == NULL) {
        ERR_EXIT("out of memory allocating all requests");
    }
    for (int i = 0; i < maxfd; i++) {
        init_request(&requestP[i]);
    }
    requestP[svr.listen_fd].conn_fd = svr.listen_fd;
    strcpy(requestP[svr.listen_fd].host, svr.hostname);

    return;
}
