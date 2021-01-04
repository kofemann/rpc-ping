#include <rpc/rpc.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/times.h>
#include <pthread.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <semaphore.h>
#include <netinet/tcp.h>

struct state {
    unsigned long requests;
    int sock_fd;
    CLIENT *handle;
    int id;
    int count;
    pthread_t thread;
    sem_t go;

    /* RPC */
    int proc;
};

void * worker(void *arg) {

    struct state *s = arg;
    enum clnt_stat status;
    struct timeval t;
    int i;

    /*
     *   use 30 seconds timeout
     */
    t.tv_sec = 30;
    t.tv_usec = 0;

    if (sem_wait(&s->go) != 0) {
        perror("sem_wait");
        exit(1);
    }

    for (i = 0; i < s->count; i++) {
        status = clnt_call(s->handle, s->proc, (xdrproc_t) xdr_void,
                NULL, (xdrproc_t) xdr_void, NULL, t);

        if (status != RPC_SUCCESS) {
            clnt_perror(s->handle, "rpc error");
        }
    }

    return NULL;
}

int main(int argc, char *argv[]) {

    struct state *states, *s;

    int count = 100000;
    int i;
    int programm;
    int version;
    int nthreads = 1;
    int nloops = 1;
    int no_delay = 1;
    double duration;
    AUTH *cl_auth;
    struct sockaddr_in serv_addr;
    int port;
    struct hostent *hp;
    clock_t rtime;
    struct tms dummy;

    if (argc < 5 || argc > 7) {
        printf("Usage: rpcping <host> <port> <program> <version> [nthreads] [nloops]\n");
        exit(1);
    }

    if (argc > 5) {
        nthreads = atoi(argv[5]);
    }

    if (argc > 6) {
        nloops = atoi(argv[6]);
    }

    states = calloc(nthreads, sizeof (struct state));
    if (!states) {
        perror("malloc");
        exit(1);
    }

    /*
     *   Create Client Handle
     */
    programm = atoi(argv[3]);
    version = atoi(argv[4]);
    port = atoi(argv[2]);
    cl_auth = authunix_create_default();

    memset((char *) &serv_addr, 0, sizeof(serv_addr));

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);

    // try by hostname first
    hp = (struct hostent *) gethostbyname(argv[1]);
    if (hp) {
        memcpy( &serv_addr.sin_addr.s_addr, hp->h_addr_list[0], hp->h_length);
    } else {
        if ((serv_addr.sin_addr.s_addr = inet_addr(argv[1])) < 0) {
            perror("resolve");
            exit(1);
        }
    }

    do {
        sem_t go;
        if (sem_init(&go, 0, nthreads) != 0) {
            perror("sem_init");
            exit(1);
        }

        for (i = 0; i < nthreads; i++) {

            s = &states[i];

            s->sock_fd = socket(AF_INET, SOCK_STREAM, 0);
            if (s->sock_fd < 0) {
                perror("socket");
                exit(1);
            }

            if (setsockopt(s->sock_fd, IPPROTO_TCP, TCP_NODELAY, &no_delay, sizeof(no_delay)) != 0) {
                perror("setsockopt");
                exit(1);
            }
            s->handle = clnttcp_create(&serv_addr, programm, version, &s->sock_fd, 0, 0);

            if (s->handle == NULL) {
                clnt_pcreateerror("clnt failed");
                exit(2);
            }

            s->id = i;
            s->count = count;
            s->proc = 0;
            s->handle->cl_auth = cl_auth;
            s->go = go;

            pthread_create(&s->thread, NULL, worker, s);
            if (sem_post(&go) != 0) {
                perror("sem_post");
                exit(1);
            }
        }

        rtime = times(&dummy);

        for (i = 0; i < nthreads; i++) {
            pthread_join(states[i].thread, NULL);
        }

        duration = ((double) (times(&dummy) - rtime) / (double) sysconf(_SC_CLK_TCK));

        fprintf(stdout, "Speed:  %2.2f rps in %2.2fs (%2.4f ms per request), %d in total\n",
            (double)count*nthreads / duration, duration, duration*1000/count*nthreads, count * nthreads);
        fflush(stdout);

        for (i = 0; i < nthreads; i++) {
            clnt_destroy(states[i].handle);
            close(states[i].sock_fd);
        }

    } while(--nloops > 0);
    auth_destroy(cl_auth);

}
