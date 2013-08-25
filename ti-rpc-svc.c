#include <stdio.h>
#include <rpc/rpc.h>
#include <errno.h>

#define PROGNUM 100018
#define VERSNUM 1

void null_dispatch(struct svc_req *request, SVCXPRT *svc) {

    (void) svc_sendreply((SVCXPRT *) svc,
            (xdrproc_t) xdr_void,
            (char *) NULL);
} 

int main(int argc, char *argv[]) {

    int num_svc;
    SVCXPRT     *transp;
    
    transp = svctcp_create (RPC_ANYSOCK, BUFSIZ, BUFSIZ);

    svc_unregister(PROGNUM, VERSNUM);


    num_svc = svc_register(transp, PROGNUM, VERSNUM, null_dispatch, IPPROTO_TCP);

    /* check for errors calling svc_create() */
    if (!num_svc) {
        perror("svc_create");
        return 1;
    }

        
    svc_run();

    /*
     * never reached
     */
   
    svc_unregister(PROGNUM, VERSNUM);

   

}
