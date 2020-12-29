#include <rpc/rpc.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/times.h>
#include <pthread.h>
#include <netdb.h>
#include <arpa/inet.h>

static pthread_mutex_t rnmtx = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t rncond = PTHREAD_COND_INITIALIZER;
static volatile unsigned running;

struct state {
    unsigned long requests;
    CLIENT *handle;
    int id;
    int count;

    /* RPC */
    int proc;
    double avarageTime;
    double avarageRPS;
};

static void worker_done() {
    pthread_mutex_lock(&rnmtx);
    --running;
    pthread_mutex_unlock(&rnmtx);
    pthread_cond_broadcast(&rncond);
}

void * worker(void *arg) {

    struct state *s = arg;
    enum clnt_stat status;
    clock_t rtime;
    struct tms dummy;
    struct timeval t;
    int i;

    /*
     *   use 30 seconds timeout
     */
    t.tv_sec = 30;
    t.tv_usec = 0;

    rtime = times(&dummy);
    for (i = 0; i < s->count; i++) {
        status = clnt_call(s->handle, s->proc, (xdrproc_t) xdr_void,
                NULL, (xdrproc_t) xdr_void, NULL, t);

        if (status != RPC_SUCCESS) {
            clnt_perror(s->handle, "rpc error");
        }
    }

    s->avarageTime = ((double) (times(&dummy) - rtime) / (double) sysconf(_SC_CLK_TCK));
    s->avarageRPS = s->count / s->avarageTime;

    clnt_destroy(s->handle);
    worker_done();
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
    double avarageRPS;
    double avarageTime;
    AUTH *cl_auth;
    struct sockaddr_in serv_addr;
    int sock_fd;
    int port;
    struct hostent *hp;

    if (argc < 5 || argc > 7) {
        printf("Usage: rpcping <host> <port> <program> <version> [nthreads] [nloops]\n");
        exit(1);
    }

    if (argc == 6) {
        nthreads = atoi(argv[5]);
    }

    if (argc == 7) {
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

    sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (sock_fd < 0) {
        perror("socket");
        exit(1);
    }

    do {
        running = nthreads;
        for (i = 0; i < nthreads; i++) {
            pthread_t t;
            s = &states[i];
            s->handle = clnttcp_create(&serv_addr, programm, version, &sock_fd, 0, 0);

            if (s->handle == NULL) {
                clnt_pcreateerror("clnt failed");
                exit(2);
            }

            s->id = i;
            s->count = count;
            s->proc = 0;
            s->handle->cl_auth = cl_auth;

            pthread_create(&t, NULL, worker, s);
        }

        while (running) {
            pthread_cond_wait(&rncond, &rnmtx);
        }

        avarageRPS = 0.0;
        avarageTime = 0.0;
        for (i = 0; i < nthreads; i++) {
            s = &states[i];
            avarageRPS += s->avarageRPS;
            avarageTime += s->avarageTime;
        }
        avarageTime = avarageTime / nthreads;
        avarageRPS = avarageRPS / nthreads;

        fprintf(stdout, "Speed:  %2.2f rps in %2.2fs (%2.4f s per request), %2.2f rps in total\n",
            avarageRPS, avarageTime, avarageTime/avarageRPS, avarageRPS * nthreads);
        fflush(stdout);
    } while(--nloops > 0);
    auth_destroy(cl_auth);

}
