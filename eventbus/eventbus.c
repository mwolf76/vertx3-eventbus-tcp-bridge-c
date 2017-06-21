/**
 * @file eventbus.c
 * @brief Event Bus client library implementation.
 */
#include <assert.h>
#include <netdb.h>
#include <pthread.h>
#include <signal.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <unistd.h>

#include "hash-table.h"
#include "config.h"

#include "eventbus.h"
#include "eventbus_private.h"

static const char *EVENTBUS_DEFAULT_NODE = "localhost";
static const char *EVENTBUS_DEFAULT_SERVICE = "7000";

/* message keys */
static const char *EVT_TYPE = "type";
static const char *EVT_TYPE_MESSAGE = "message";
static const char *EVT_TYPE_ERROR = "err";

static const char *EVT_ADDR = "address";
static const char *EVT_MESSAGE = "message";

/* max number of iterations to wait when terminating recv thread */
static const int RECEIVE_THREAD_TERMINATION_MAXWAIT =  5; /* x 200ms = 1s */
static const size_t RECEIVE_THREAD_DEFAULT_BUFSIZE = 4096; /* 4k */

/** -- static prototypes ------------------------------------------------------------------------------------------- */
static void delay(uint32_t millis);
static const char *frame_msg_type(eventbus_frame_t type);

static unsigned long address_hash(void *address);
static int address_equal(void *addr1, void *addr2);

static int post_message(eventbus_t *instance, eventbus_frame_t frame_type, const char *address,
                        const char *reply_address, json_t *headers, json_t *body);

static ssize_t receive(int fd, void *dest, size_t len);
static void *receive_message_loop(void *dummy);
static void shutdown_client(eventbus_t *instance);

static pthread_attr_t default_thread_attrs;
static pthread_mutexattr_t default_mutex_attrs;

/* -- getters ------------------------------------------------------------------------------------------------------ */
const char *eventbus_node(eventbus_t *instance)
{
    assert(instance);
    return instance->node;
}

const char *eventbus_service(eventbus_t *instance)
{
    assert(instance);
    return instance->service;
}

void* eventbus_user(eventbus_t *instance)
{
    assert(instance);
    return instance->user;
}

eventbus_t *eventbus_create(const char *node, const char *service, handler_t error_handler, void *user)
{
    /* allocate and initialize memory */
    eventbus_t *instance = (eventbus_t *) malloc(sizeof(eventbus_t));
    if (! instance)
        return NULL;

    /* initialize the structure */
    memset(instance, 0, sizeof(eventbus_t));
    instance->node = strdup(node ? node : EVENTBUS_DEFAULT_NODE);
    if (! instance->node) {
        instance->error = EVENTBUS_ERROR_OUT_OF_MEMORY;
        return instance;
    }

    instance->service = strdup(service ? service : EVENTBUS_DEFAULT_SERVICE);
    if (! instance->service) {
        instance->error = EVENTBUS_ERROR_OUT_OF_MEMORY;
        return instance;
    }

    instance->error_handler = error_handler;
    instance->user = user;
    instance->socket = -1;

    /* initial state */
    instance->state = EVENTBUS_STATE_IDLE;
    instance->error = EVENTBUS_ERROR_NOERROR;

    /* instance mutex setup with defaults */
    pthread_mutexattr_init(&default_mutex_attrs);
    pthread_mutex_init(&instance->object_mutex, &default_mutex_attrs);

    /* deferred (see eventbus_start) */
    instance->handlers = NULL;

    return instance;
} /* eventbus_create() */

eventbus_state_t eventbus_state(eventbus_t *instance)
{
    assert(instance);
    eventbus_state_t ret;

    ACQUIRE_LOCK(instance->object);
    ret = instance->state;
    RELEASE_LOCK(instance->object);

    return ret;
}

eventbus_error_t eventbus_error(eventbus_t *instance)
{
    assert(instance);
    eventbus_error_t ret;

    ACQUIRE_LOCK(instance->object);
    ret = instance->error;
    RELEASE_LOCK(instance->object);

    return ret;
}

int eventbus_start(eventbus_t *instance)
{
    assert(instance);
    int ret = 0;

    ACQUIRE_LOCK(instance->object);
    if (instance->state == EVENTBUS_STATE_IDLE) {
        struct addrinfo hints;
        struct addrinfo *result, *rp;

        /* Obtain address(es) matching host/port */
        memset(&hints, 0, sizeof(struct addrinfo));
        hints.ai_socktype = SOCK_STREAM; /* Stream socket */
        hints.ai_protocol = IPPROTO_TCP; /* TCP protocol */

        if (getaddrinfo(instance->node, instance->service, &hints, &result)) {
            instance->error = EVENTBUS_ERROR_BAD_ADDRESS;
            instance->state = EVENTBUS_STATE_ERROR;

            /* name resolution error */
            ret = -1;
        } else {
            /* getaddrinfo() returns a list of address structures. Try each address
               until we successfully connect(2). If socket(2) (or connect(2)) fails,
               we (close the socket and) try the next address. */
            for (rp = result; rp != NULL; rp = rp->ai_next) {
                assert(rp->ai_socktype == SOCK_STREAM);
                assert(rp->ai_protocol == IPPROTO_TCP);

                instance->socket = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
                if (instance->socket == -1)
                    continue;

                if (connect(instance->socket, rp->ai_addr, rp->ai_addrlen) != -1)
                    break; /* running */

                close(instance->socket);
            }

            if (! rp) {
                instance->error = EVENTBUS_ERROR_CONNECT_ERROR;
                instance->state = EVENTBUS_STATE_ERROR;

                /* No address succeeded */
                ret = -1;
            }

            /* No longer needed */
            freeaddrinfo(result);

            if (! ret) {
                /* Postponed from create: no free for values, they're function pointers. */
                assert(! instance->handlers);
                instance->handlers = hash_table_new(&address_hash, &address_equal);
                hash_table_register_free_functions(instance->handlers,
                                                   (HashTableKeyFreeFunc) free,
                                                   (HashTableValueFreeFunc) NULL);

                instance->receive_bufsize = RECEIVE_THREAD_DEFAULT_BUFSIZE; /* will grow as required */
                if (! pthread_create(&instance->receive_thread, NULL, &receive_message_loop, instance)) {
                    instance->state = EVENTBUS_STATE_RUNNING;
                } else {
                    instance->error = EVENTBUS_ERROR_THREADING_ERROR;
                    instance->state = EVENTBUS_STATE_ERROR;
                }
            }
        }
    } else ret = -1;
    RELEASE_LOCK(instance->object);

    return ret;
} /* eventbus_start() */

int eventbus_stop(eventbus_t *instance)
{
    assert(instance);

    ACQUIRE_LOCK(instance->object);
    shutdown_client(instance);
    RELEASE_LOCK(instance->object);

    return 0;
} /* eventbus_stop() */

int eventbus_destroy(eventbus_t *instance)
{
    assert(instance);

    ACQUIRE_LOCK(instance->object);

    /* does nothing if already idle */
    shutdown_client(instance);

    free(instance->service);
    instance->service = NULL;

    free(instance->node);
    instance->node = NULL;

    RELEASE_LOCK(instance->object);

    free(instance);

    return 0;
} /* eventbus_destroy() */

int eventbus_ping(eventbus_t *instance)
{
    assert(instance);

    ACQUIRE_LOCK(instance->object);
    int ret = post_message(instance, FRAME_PING, NULL, NULL, NULL, NULL);
    RELEASE_LOCK(instance->object);

    return ret;
}

int eventbus_send(eventbus_t *instance, const char *address, const char *reply_address,
                   json_t *headers, json_t *body)
{
    assert(instance);

    ACQUIRE_LOCK(instance->object);
    int ret = post_message(instance, FRAME_SEND, address, reply_address, headers, body);
    RELEASE_LOCK(instance->object);

    return ret;
}

int eventbus_publish(eventbus_t *instance, const char *address,
                     json_t *headers, json_t *body)
{
    assert(instance);

    ACQUIRE_LOCK(instance->object);
    /* reply_address is unused in PUBLISH messages */
    int ret = post_message(instance, FRAME_PUBLISH, address, NULL, headers, body);
    RELEASE_LOCK(instance->object);

    return ret;
}

/* returns 0 if handler was successfully registered, -1 otherwise */
int eventbus_register(eventbus_t *instance, const char *address, handler_t handler)
{
    assert(instance);
    int ret = 0;

    ACQUIRE_LOCK(instance->object);
    if (instance->state == EVENTBUS_STATE_RUNNING) {
        handler_t lookup = hash_table_lookup(instance->handlers, (void *) address);
        if (! lookup) {
            /* keys inserted in the hash table must be privately owned */
            hash_table_insert(instance->handlers, strdup(address), handler);
            ret = post_message(instance, FRAME_REGISTER, address, NULL, NULL, NULL);
        } else ret = -1;
    } else ret = -1;
    RELEASE_LOCK(instance->object);

    return ret;
} /* eventbus_register_handler() */

/* returns 0 if handler was successfully unregistered, -1 otherwise */
int eventbus_unregister(eventbus_t *instance, const char *address)
{
    assert(instance);
    int ret = 0;

    ACQUIRE_LOCK(instance->object);
    if (instance->state == EVENTBUS_STATE_RUNNING) {
        handler_t lookup = hash_table_lookup(instance->handlers, (void *) address);
        if (lookup) {
            /* keys inserted in the hash table must be privately owned */
            hash_table_remove(instance->handlers, (void *) address);
            ret = post_message(instance, FRAME_UNREGISTER, address, NULL, NULL, NULL);
        } else ret = -1;
    } else ret = -1;
    RELEASE_LOCK(instance->object);

    return ret;
} /* eventbus_unregister_handler() */

/** -- internals --------------------------------------------------------------------------------------------------- */
static void delay(uint32_t millis)
{
    uint32_t total = 1000 * millis;
    struct timeval timeout = {
        total / 1000000L,
        total % 1000000L
    };

    /* just waiting for timeout (cfr. select man page) */
    select(0, NULL, NULL, NULL, &timeout);
}

static const char *frame_msg_type(eventbus_frame_t type)
{
    static const char *FRAME_PING_STR = "ping";
    static const char *FRAME_SEND_STR = "send";
    static const char *FRAME_PUBLISH_STR = "publish";
    static const char *FRAME_REGISTER_STR = "register";
    static const char *FRAME_UNREGISTER_STR = "unregister";

    if (type == FRAME_PING)
        return FRAME_PING_STR;

    if (type == FRAME_SEND)
        return FRAME_SEND_STR;

    if (type == FRAME_PUBLISH)
        return FRAME_PUBLISH_STR;

    if (type == FRAME_REGISTER)
        return FRAME_REGISTER_STR;

    if (type == FRAME_UNREGISTER)
        return FRAME_UNREGISTER_STR;

    assert(0); /* unreachable */
}

static unsigned long address_hash(void *address_)
{
    char *address = (char *) address_;
    unsigned long hash = 5381;
    int c;

    while (c = *address ++)
        hash = ((hash << 5) + hash) + c; /* hash * 33 + c */

    return hash;
}

static int address_equal(void *addr1_, void *addr2_)
{
    char *addr1 = (char *) addr1_;
    char *addr2 = (char *) addr2_;
    return ! strcmp(addr1, addr2);
}

static int post_message(eventbus_t *instance, eventbus_frame_t frame_type, const char *address,
                        const char *reply_address, json_t *headers, json_t *body)
{
    int ret = 0;
    assert(instance);

    if (instance->state == EVENTBUS_STATE_RUNNING) {
        json_t* out = json_object();

        json_object_set_new(out, "type",
                            json_string(frame_msg_type(frame_type)));

        if (address)
            json_object_set_new(out, "address",
                                json_string(address));

        if (reply_address)
            json_object_set_new(out, "reply_address",
                                json_string(reply_address));

        if (headers)
            json_object_set_new(out, "headers", headers);

        if (body)
            json_object_set_new(out, "body", body);

        /* dumps directly to buffer using stack space, requires jansson >= 2.10 */
        size_t size = json_dumpb(out, NULL, 0, 0);
        char stackbuf[size];

        size_t written = json_dumpb(out, stackbuf, size, 0);
        assert(written == size);

        uint32_t htonl_size = htonl(size);

        sigset_t sigpipe_mask;
        sigemptyset(&sigpipe_mask);
        sigaddset(&sigpipe_mask, SIGPIPE);
        sigset_t saved_mask;

        int rc = pthread_sigmask(SIG_BLOCK, &sigpipe_mask, &saved_mask);
        assert(! rc);

        write(instance->socket, &htonl_size, sizeof(uint32_t));
        write(instance->socket, stackbuf, size);

        struct timespec zerotime = {0};
        rc = sigtimedwait(&sigpipe_mask, 0, &zerotime);
        if (rc == SIGPIPE) {
            instance->state = EVENTBUS_STATE_ERROR;
            instance->error = EVENTBUS_ERROR_BROKEN_PIPE;
            ret = -1;
        }

        rc = pthread_sigmask(SIG_SETMASK, &saved_mask, 0);
        assert(! rc);

        json_decref(out);
    } else ret = -1;

    return ret;
}

/* receives full fragment of requested len or nothing at all */
ssize_t receive(int fd, void *dest, size_t len)
{
    ssize_t received = recv(fd, dest, len, MSG_DONTWAIT | MSG_PEEK);

    if (received == -1)
        return -1;

    if ((size_t) received != len)
        return 0;

    received = recv(fd, dest, len, MSG_WAITALL);
    assert(received == len); /* here we got full message */

    return received;
}

static void* receive_message_loop(void *ctx)
{
    eventbus_t *instance = (eventbus_t *) ctx;
    assert(instance);

    size_t frame_len;
    char *frame_buf = malloc(instance->receive_bufsize); /* initial bufsize, may change overtime */

    json_t *obj;
    json_error_t error;
    int rc;

    /* recv loop */
    while (1) {
        switch(instance->receive_fsm) {
        case RECV_LENGTH:
            /* Waiting for frame length */
            rc = receive(instance->socket, &frame_len, sizeof(uint32_t));
            if (0 < rc) {
                if (rc == sizeof(uint32_t)) {
                    frame_len = ntohl(frame_len);

                    int changed = 0;
                    while (frame_len >= instance->receive_bufsize) {
                        instance->receive_bufsize *= 2;
                        changed = 1;
                    }

                    if (changed)
                        frame_buf = realloc(frame_buf, instance->receive_bufsize);

                    instance->receive_fsm = RECV_BODY;
                } else {
                    instance->receive_fsm = RECV_ERROR;
                }
            }
            break; /* RECV_LENGTH */

        case RECV_BODY:
            /* Waiting for frame body */
            assert(frame_len < instance->receive_bufsize);

            rc = receive(instance->socket, frame_buf, frame_len);
            if (0 < rc) {
                instance->receive_fsm = (rc == frame_len)
                    ? RECV_PROCESS
                    : RECV_ERROR ;
            }
            break; /* RECV_BODY */

        case RECV_PROCESS:
            /* Got all message */
            frame_buf[frame_len] = '\0';
            obj = json_loads(frame_buf, 0, &error);
            if (obj) {
                const char *type = json_string_value(json_object_get(obj, EVT_TYPE));
                if (! strcmp(type, EVT_TYPE_MESSAGE)) {
                    const char *address = json_string_value(json_object_get(obj, EVT_ADDR));

                    /* dispatch to handler */
                    ACQUIRE_LOCK(instance->object);
                    handler_t lookup = hash_table_lookup(instance->handlers, (void *) address);
                    RELEASE_LOCK(instance->object);

                    if (lookup)
                        lookup(instance, obj);

                } else if (! strcmp(type, EVT_TYPE_ERROR)) {
                    if (instance->error_handler)
                        instance->error_handler(instance, obj);
                }

                json_decref(obj);
                instance->receive_fsm = RECV_LENGTH;
            }
            break;

        case RECV_ERROR:
            /* Uh oh ... we're in trouble here */
            ACQUIRE_LOCK(instance->object);
            instance->error = EVENTBUS_ERROR_RECEIVE_SYNC;
            instance->state = EVENTBUS_STATE_ERROR;
            RELEASE_LOCK(instance->object);

            goto leave;
            break;

        default:
            assert(0); /* unreachable */
        }

        /* shutdown required? */
        if (instance->shutdown) {
            instance->shutdown = 0; /* ACK */
            break;
        }

        delay(20);
    } /* while() */

 leave:
    free(frame_buf);
    return NULL;
}

static void shutdown_client(eventbus_t *instance)
{
    if (instance->state == EVENTBUS_STATE_RUNNING) {
        int maxwait = RECEIVE_THREAD_TERMINATION_MAXWAIT;
        instance->shutdown = 1; /* require shutdown */
        while (maxwait --) {
            delay(200);
            if (! instance->shutdown) /* shutdown completed */
                break;
        }

        /* recv thread is done, free resources */
        pthread_detach(instance->receive_thread);

        /* clear handlers table */
        hash_table_free(instance->handlers);
        instance->handlers = NULL;
    }

    if (0 <= instance->socket)
        close(instance->socket);

    instance->state = EVENTBUS_STATE_IDLE;
}

