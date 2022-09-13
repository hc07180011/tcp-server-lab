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

#define INADDR_ANY ((in_addr_t) 0x00000000)
#define ERR_EXIT(a) do { perror(a); exit(1); } while(0)

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
    int id;
    int wait_for_write;  // used by handle_read to know if the header is read or not.
} request;

server svr;  // server
request* requestP = NULL;  // point to a list of requests
int maxfd;  // size of open file descriptor table, size of request list

const char* accept_read_header = "ACCEPT_FROM_READ";
const char* accept_write_header = "ACCEPT_FROM_WRITE";

static void init_server(unsigned short port);
// initailize a server, exit for error

static void init_request(request* reqP);
// initailize a request instance

static void free_request(request* reqP);
// free resources used by a request instance

typedef struct {
    int id;          //902001-902020
    int AZ;          
    int BNT;         
    int Moderna;     
} registerRecord;

const int id_num = 902001, id_range = 20;

typedef struct {
    char first[8];
    char second[8];
    char third[8];
} preferenceOrder;

preferenceOrder generate_preference(registerRecord target_record) {
    preferenceOrder ret;
    memset(ret.first, 0, sizeof(ret.first));

    if (target_record.AZ < 1 || target_record.AZ > 3 ||
        target_record.BNT < 1 || target_record.BNT > 3 ||
        target_record.Moderna < 1 || target_record.Moderna > 3) {
        return ret;
    }

    if (target_record.AZ < target_record.BNT && target_record.BNT < target_record.Moderna) {
        sprintf(ret.first, "AZ");
        sprintf(ret.second, "BNT");
        sprintf(ret.third, "Moderna");
    } else if (target_record.AZ < target_record.Moderna && target_record.Moderna < target_record.BNT) {
        sprintf(ret.first, "AZ");
        sprintf(ret.second, "Moderna");
        sprintf(ret.third, "BNT");
    } else if (target_record.BNT < target_record.AZ && target_record.AZ < target_record.Moderna) {
        sprintf(ret.first, "BNT");
        sprintf(ret.second, "AZ");
        sprintf(ret.third, "Moderna");
    } else if (target_record.Moderna < target_record.AZ && target_record.AZ < target_record.BNT) {
        sprintf(ret.first, "Moderna");
        sprintf(ret.second, "AZ");
        sprintf(ret.third, "BNT");
    } else if (target_record.BNT < target_record.Moderna && target_record.Moderna < target_record.AZ) {
        sprintf(ret.first, "BNT");
        sprintf(ret.second, "Moderna");
        sprintf(ret.third, "AZ");
    } else if (target_record.Moderna < target_record.BNT && target_record.BNT < target_record.AZ) {
        sprintf(ret.first, "Moderna");
        sprintf(ret.second, "BNT");
        sprintf(ret.third, "AZ");
    }
    return ret;
}

int handle_read(request* reqP) {
    int r;
    char buf[512];

    // Read in request from client
    r = read(reqP->conn_fd, buf, sizeof(buf));
    if (r < 0) return -1;
    if (r == 0) return 0;
    char* p1 = strstr(buf, "\015\012");
    if (p1 == NULL) {
       p1 = strstr(buf, "\012");
        if (p1 == NULL) {
            ERR_EXIT("this really should not happen...");
        }
    }
    size_t len = p1 - buf + 1;
    memmove(reqP->buf, buf, len);
    reqP->buf[len - 1] = '\0';
    reqP->buf_len = len-1;
    return 1;
}

int main(int argc, char** argv) {

    // Parse args.
    if (argc != 2) {
        fprintf(stderr, "usage: %s [port]\n", argv[0]);
        exit(1);
    }

    int clilen;
    struct sockaddr_in cliaddr;  // used by accept()

    int i;
    int select_ret, pread_ret;

    int conn_fd;  // fd for a new connection with client
    int file_fd;  // fd for file that we open for reading

    char buf[512];

    registerRecord register_record;

    fd_set rfds;
    struct timeval tv;

    int max_conn_num = 20;
    int read_lock_cnt[max_conn_num], write_lock_cnt[max_conn_num];
    memset(read_lock_cnt, 0, sizeof(read_lock_cnt));
    memset(write_lock_cnt, 0, sizeof(write_lock_cnt));

    struct flock fl;

    file_fd = open("registerRecord", O_RDWR);
    if (file_fd < 0) {
        fprintf(stderr, "open registerRecord error");
        ERR_EXIT("open");
    }

    // Initialize server
    init_server((unsigned short) atoi(argv[1]));

    // Loop for handling connections
    fprintf(stderr, "\nstarting on %.80s, port %d, fd %d, maxconn %d...\n", svr.hostname, svr.port, svr.listen_fd, maxfd);

    FD_ZERO(&rfds);
    FD_SET(svr.listen_fd, &rfds);

    maxfd = svr.listen_fd;

    fd_set new_rfds;

    while (1) {
        // TODO: Add IO multiplexing

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
            if (FD_ISSET(i, &new_rfds)) {
                if (i == svr.listen_fd) {
                    // Check new connection
                    clilen = sizeof(cliaddr);
                    conn_fd = accept(svr.listen_fd, (struct sockaddr*)&cliaddr, (socklen_t*)&clilen);
                    if (conn_fd < 0) {
                        if (errno == EINTR || errno == EAGAIN) continue;  // try again
                        if (errno == ENFILE) {
                            (void) fprintf(stderr, "out of file descriptor table ... (maxconn %d)\n", maxfd);
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
                    
                    fprintf(stderr, "getting a new request... fd %d from %s\n", conn_fd, requestP[conn_fd].host);
                    
                    // Set buf to be the welcoming message
                    sprintf(requestP[conn_fd].buf, "Please enter your id (to check your preference order):\n");
                    
                    // Pass the message to socket
                    fprintf(stderr, "[Return] %s", requestP[conn_fd].buf);
                    sprintf(buf, "%s", requestP[conn_fd].buf); 
                    
                    if (write(requestP[conn_fd].conn_fd, buf, strlen(buf)) == -1) {
                        ERR_EXIT("write");
                    }
                } else {
                    // Receive some commands from the client

                    conn_fd = i;

                    int ret = handle_read(&requestP[conn_fd]); // parse data from client to requestP[conn_fd].buf
                    fprintf(stderr, "ret = %d\n", ret);
                    if (ret < 0) {
                        fprintf(stderr, "bad request from %s\n", requestP[conn_fd].host);
                        continue;
                    }
                    
// TODO: handle requests from clients
#ifdef READ_SERVER
                    requestP[conn_fd].id = atoi(requestP[conn_fd].buf) - id_num;
                    if (requestP[conn_fd].id < 0 || requestP[conn_fd].id > (id_range - 1)) {
                        sprintf(requestP[conn_fd].buf, "[Error] Operation failed. Please try again.\n");
                    } else {
                        fl.l_type = F_RDLCK;
                        fl.l_whence = SEEK_SET;
                        fl.l_start = sizeof(registerRecord) * requestP[conn_fd].id;
                        fl.l_len = sizeof(registerRecord);

                        if (write_lock_cnt[requestP[conn_fd].id]) {
                            sprintf(requestP[conn_fd].buf, "Locked.\n");
                        } else if (fcntl(file_fd, F_SETLK, &fl) == -1) {
                            sprintf(requestP[conn_fd].buf, "Locked.\n");
                        } else {
                            // Start to read the record

                            read_lock_cnt[requestP[conn_fd].id]++;

                            pread_ret = pread(file_fd, &register_record, sizeof(registerRecord), sizeof(registerRecord) * requestP[conn_fd].id);
                            if (pread_ret == -1) {
                                sprintf(requestP[conn_fd].buf, "[Error] Operation failed. Please try again.\n");
                            } else {
                                preferenceOrder order = generate_preference(register_record);
                                sprintf(requestP[conn_fd].buf, "Your preference order is %s > %s > %s.\n", order.first, order.second, order.third);
                            }

                            read_lock_cnt[requestP[conn_fd].id]--;
                            if (!read_lock_cnt[requestP[conn_fd].id]) {
                                fl.l_type = F_UNLCK;
                                fl.l_whence = SEEK_SET;
                                fl.l_start = sizeof(registerRecord) * requestP[conn_fd].id;
                                fl.l_len = sizeof(registerRecord);
                                fcntl(file_fd, F_SETLK, &fl);
                            }
                        }
                    }
#elif defined WRITE_SERVER
                    if (!requestP[conn_fd].wait_for_write) {
                        // Still waiting for id selection
                        requestP[conn_fd].id = atoi(requestP[conn_fd].buf) - id_num;
                        if (requestP[conn_fd].id < 0 || requestP[conn_fd].id > (id_range - 1)) {
                            sprintf(requestP[conn_fd].buf, "[Error] Operation failed. Please try again.\n");
                        } else {
                            fl.l_type = F_WRLCK;
                            fl.l_whence = SEEK_SET;
                            fl.l_start = sizeof(registerRecord) * requestP[conn_fd].id;
                            fl.l_len = sizeof(registerRecord);

                            if (read_lock_cnt[requestP[conn_fd].id] || write_lock_cnt[requestP[conn_fd].id]) {
                                sprintf(requestP[conn_fd].buf, "Locked.\n");
                            } else if (fcntl(file_fd, F_SETLK, &fl) == -1) {
                                sprintf(requestP[conn_fd].buf, "Locked.\n");
                            } else {
                                pread_ret = pread(file_fd, &register_record, sizeof(registerRecord), sizeof(registerRecord) * requestP[conn_fd].id);
                                if (pread_ret == -1) {
                                    sprintf(requestP[conn_fd].buf, "[Error] Operation failed. Please try again.\n");
                                } else {
                                    preferenceOrder order = generate_preference(register_record);
                                    sprintf(requestP[conn_fd].buf, "Your preference order is %s > %s > %s.\nPlease input your preference order respectively(AZ,BNT,Moderna):\n", order.first, order.second, order.third);
                                }
                                write_lock_cnt[requestP[conn_fd].id]++;
                                requestP[conn_fd].wait_for_write++;
                                // Might be able to simplify
                                fprintf(stderr, "[Return] \"%s\"\n", requestP[conn_fd].buf);
                                if (write(requestP[conn_fd].conn_fd, requestP[conn_fd].buf, strlen(requestP[conn_fd].buf)) == -1) {
                                    ERR_EXIT("write");
                                }
                                continue;
                            }
                        }
                    } else {
                        // Receiving a modification request

                        int az_order, bnt_order, moderna_order;
                        sscanf(requestP[conn_fd].buf, "%d %d %d", &az_order, &bnt_order, &moderna_order);

                        registerRecord new_record = register_record;
                        new_record.AZ = az_order;
                        new_record.BNT = bnt_order;
                        new_record.Moderna = moderna_order;

                        preferenceOrder order = generate_preference(new_record);
                        if (!strlen(order.first)) {
                            sprintf(requestP[conn_fd].buf, "[Error] Operation failed. Please try again.\n");
                        } else {
                            if (pwrite(file_fd, &new_record, sizeof(registerRecord), sizeof(registerRecord) * requestP[conn_fd].id) == -1) {
                                ERR_EXIT("pwrite");
                            }
                            sprintf(requestP[conn_fd].buf, "Preference order for %d modified successed, new preference order is %s > %s > %s.\n", (requestP[conn_fd].id + id_num), order.first, order.second, order.third);
                        }

                        write_lock_cnt[requestP[conn_fd].id]--;
                        requestP[conn_fd].wait_for_write--;
                        fl.l_type = F_UNLCK;
                        fl.l_whence = SEEK_SET;
                        fl.l_start = sizeof(registerRecord) * requestP[conn_fd].id;
                        fl.l_len = sizeof(registerRecord);
                        fcntl(file_fd, F_SETLK, &fl);
                    }
#endif

// #ifdef READ_SERVER      
//                     //sprintf(buf, "%s : %s", accept_read_header, requestP[conn_fd].buf);  
// #elif defined WRITE_SERVER
//                     //sprintf(buf, "%s : %s", accept_write_header, requestP[conn_fd].buf); 
// #endif
                    fprintf(stderr, "[Return] \"%s\"\n", requestP[conn_fd].buf);
                    if (write(requestP[conn_fd].conn_fd, requestP[conn_fd].buf, strlen(requestP[conn_fd].buf)) == -1) {
                        ERR_EXIT("write");
                    }
                    memset(requestP[conn_fd].buf, 0, sizeof(requestP[conn_fd].buf));

                    FD_CLR(conn_fd, &rfds);

                    close(requestP[conn_fd].conn_fd);
                    free_request(&requestP[conn_fd]);
                }
            }
        }
    }

    free(requestP);
    return 0;
}

// ======================================================================================================
// You don't need to know how the following codes are working
#include <fcntl.h>

static void init_request(request* reqP) {
    reqP->conn_fd = -1;
    reqP->buf_len = 0;
    reqP->id = 0;
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
    maxfd = getdtablesize();
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
