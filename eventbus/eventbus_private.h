/**
 * @file eventbus_private
 * @brief Vert.X 3 TCP eventbus bridge client library private header file.
 */

#ifndef VERTX3_TCP_EVENTBUS_BRIDGE_CLIENT_PRIVATE_H
#define VERTX3_TCP_EVENTBUS_BRIDGE_CLIENT_PRIVATE_H

/* -- critical sections management --------------------------------------------------------------------------------- */
#define ACQUIRE_LOCK(obj) do {                                \
        pthread_mutex_lock(& obj##_mutex);                    \
    } while (0)

#define RELEASE_LOCK(obj) do {                  \
        pthread_mutex_unlock(& obj##_mutex);    \
    } while(0)


/* -- private enums ------------------------------------------------------------------------------------------------ */
typedef enum {
    FRAME_PING,
    FRAME_SEND,
    FRAME_PUBLISH,
    FRAME_REGISTER,
    FRAME_UNREGISTER
} eventbus_frame_t;

typedef enum {
    RECV_LENGTH,
    RECV_BODY,
    RECV_PROCESS,
    RECV_ERROR
} eventbus_receive_fsm_t;

/* -- the event bus client data structure -------------------------------------------------------------------------- */
struct eventbus_s {
    /** The endpoint host name or IP (e.g. "localhost") */
    char *node;

    /** The endpoint service (e.g. "7000") */
    char *service;

    /** Invoked when an error message from the bridge is received. */
    handler_t error_handler;

    /** Opaque pointer to some user-defined data structure passed upon construction. */
    void *user;

    /** The client state. */
    eventbus_state_t state;

    /** Receiver thread handle. */
    pthread_t receive_thread;

    /** TCP socket handle. */
    int socket;

    /** Current size of the receive buffer. */
    size_t receive_bufsize;

    /** Current state of the receive FSM. */
    eventbus_receive_fsm_t receive_fsm;

    /** Last encountered error. */
    eventbus_error_t error;

    /** Registered handlers lookup table. */
    HashTable *handlers;

    /** Instance mutex. */
    pthread_mutex_t object_mutex;

    /** Reserved. */
    volatile int shutdown;
};

#endif /* VERTX3_TCP_EVENTBUS_BRIDGE_CLIENT_PRIVATE_H */
