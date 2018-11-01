#include <rpc/rpc.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/times.h>
#include <pthread.h>

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
    struct tms dumm;
    struct timeval t;
    int i;

    /*
     *   use 30 seconds timeout
     */
    t.tv_sec = 30;
    t.tv_usec = 0;

    rtime = times(&dumm);
    for (i = 0; i < s->count; i++) {
        status = clnt_call(s->handle, s->proc, (xdrproc_t) xdr_void,
                NULL, (xdrproc_t) xdr_void, NULL, t);

        if (status != RPC_SUCCESS) {
            clnt_perror(s->handle, "rpc error");
        }
    }

    s->avarageTime = s->count / ((double) (times(&dumm) - rtime) / (double) sysconf(_SC_CLK_TCK));
    
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
    double avarage;
    AUTH *cl_auth;

    if (argc < 4 || argc > 5) {
        printf("Usage: rpcping <host> <program> <version> [nthreads]\n");
        exit(1);
    }

    if (argc == 5) {
        nthreads = atoi(argv[4]);
    }

    states = calloc(nthreads, sizeof (struct state));
    if (!states) {
        perror("malloc");
        exit(1);
    }

    /*
     *   Create Client Handle
     */
    programm = atoi(argv[2]);
    version = atoi(argv[3]);
    cl_auth = authunix_create_default();

    while (1) {
        running = nthreads;
        for (i = 0; i < nthreads; i++) {
            pthread_t t;
            s = &states[i];
            s->handle = clnt_create(argv[1], programm, version, "tcp");

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

        avarage = 0.0;
        for (i = 0; i < nthreads; i++) {
            s = &states[i];
            avarage += s->avarageTime;
        }
        avarage = avarage / nthreads;
        fprintf(stdout, "Speed:  %2.4fs, %2.4fs in total\n", avarage, avarage * nthreads);
        fflush(stdout);
    }
    auth_destroy(cl_auth);

}
