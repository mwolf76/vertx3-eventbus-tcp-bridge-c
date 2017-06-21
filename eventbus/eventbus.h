/**
 * @file eventbus.h
 * @brief Vert.X 3 TCP event bus bridge client library header file.
 *
 * @mainpage Vert.X 3 TCP event bus bridge client library.
 * @author Marco Pensallorto < marco DOT pensallorto AT gmail DOT com>\n
 *
 * @section License
 * Copyright 2017 Marco Pensallorto
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES WITH
 * REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT,
 * INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
 * LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR
 * OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 *
 * @section Overview
 * This library provides a C99 interface to the Vert.x 3 event bus via the TCP
 * event bus bridge. Using this library, low-level components written in C can
 * be transparently integrated in a Vert.x 3 distributed application using the
 * event bus to exchange messages with each other. This approach can be useful
 * for instance in an embedded application, in which a C program performs
 * low-level control of an actual embedded device while business logic is
 * implemented in an higher-level application component deployed on the JVM
 * using the Vert.x toolkit. For more information on the Vert.x 3 TCP event bus
 * bridge refer to http://vertx.io/docs/vertx-tcp-eventbus-bridge/java.
 *
 * To interact with the event bus you need to create an event bus client
 * instance using @ref eventbus_create. When creating the client a special
 * handler reserved for error messages from the bridge can be registered; a
 * pointer to an application-specific data structure can also be passed upon
 * creation of the client instance. This data can be retrieved later from within
 * the handlers using @ref eventbus_user.
 *
 * With a valid event bus client at hand, you need to start it with @ref
 * eventbus_start before any actual message exchange can take place. The client
 * can be stopped anytime with @ref eventbus_stop and restarted with @ref
 * eventbus_start if required by your application logic. The client can be
 * started and stopped multiple times.
 *
 * Message handlers for incoming messages can be registered and unregistered
 * using @ref eventbus_register and @ref eventbus_unregister respectively.
 * Remember that the client needs to be running for registration/deregistration
 * functions to work properly.
 *
 * @ref eventbus_state can be used to read the current state. In case of an
 * error, @ref eventbus_error can be used to determine the cause of the error.
 *
 * Messages can be sent or published to the event bus using @ref eventbus_send
 * and @ref eventbus_publish respectively. Refer to the Vert.x 3 event bus
 * documentation for more information about the semantics of send vs. publish.
 *
 * The state of the connection can be tested using @ref eventbus_ping.
 *
 * When the application is done with the client use @ref eventbus_destroy to
 * close the connection to the event bus bridge and release the resources.
 *
 * @section Download
 * Source code is available on Github:
 * https://github.com/mwolf76/vertx3-eventbus-tcp-bridge-c
 *
 */

#ifndef VERTX3_TCP_EVENTBUS_BRIDGE_CLIENT_H
#define VERTX3_TCP_EVENTBUS_BRIDGE_CLIENT_H

#include <jansson.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * The event bus client state
 */
typedef enum {
    /** Client is idle, no message exchange can take place. */
    EVENTBUS_STATE_IDLE,

    /** Connection established, client is up and running. */
    EVENTBUS_STATE_RUNNING,

    /** Client has encountered an error: use @ref eventbus_error for more
        information about the error. */
    EVENTBUS_STATE_ERROR
} eventbus_state_t;

/**
 * Error condition (or no error if all good)
 */
typedef enum {
    /** No error */
    EVENTBUS_ERROR_NOERROR,

    /** Bad address */
    EVENTBUS_ERROR_BAD_ADDRESS,

    /** Out of memory */
    EVENTBUS_ERROR_OUT_OF_MEMORY,

    /** Socket error */
    EVENTBUS_ERROR_SOCKET_ERROR,

    /** Connect error */
    EVENTBUS_ERROR_CONNECT_ERROR,

    /** Threading error */
    EVENTBUS_ERROR_THREADING_ERROR,

    /** Receiver synchronization error */
    EVENTBUS_ERROR_RECEIVE_SYNC,

    /** Receiver illegale state error */
    EVENTBUS_ERROR_ILLEGAL_STATE,

    /** Broken pipe */
    EVENTBUS_ERROR_BROKEN_PIPE
} eventbus_error_t;

typedef struct eventbus_s eventbus_t;
typedef void (*handler_t)(eventbus_t *instance, const json_t *obj);

/**
 * Creates a new event bus client. The newly created instance will be in @ref
 * EVENTBUS_STATE_IDLE state and will need to be started using @ref
 * eventbus_start before any actual operation can take place.
 *
 * @param node The event bus endpoint host (e.g. "localhost") or NULL. If NULL
 * defaults to "localhost".
 *
 * @param service The event bus endpoint service on the node or NULL. If NULL
 * defaults to "7000".
 *
 * @param error_handler The handler used to process error messages or NULL. If
 * NULL defaults to NOP.
 *
 * @param user_data Pointer to an external user-defined structure. This pointer
 * will be embedded in the newly created instance and passed to invoked handlers
 * as part of the *instance struct.
 *
 * @return Newly created eventbus client instance or NULL if an error has
 * occurred.
 */
eventbus_t *eventbus_create(const char *node,
                            const char *service,
                            handler_t error_handler,
                            void *user_data);

/**
 * Reads the current state for the event bus client
 *
 * @param instance The event bus client.
 *
 * @return current state of the instance.
 */
eventbus_state_t eventbus_state(eventbus_t *instance);

/**
 * Reads the last error for the event bus client
 *
 * @param instance The event bus client.
 *
 * @return last error signalled for the instance.
 */
eventbus_error_t eventbus_error(eventbus_t *instance);

/**
 * Starts the event bus client. It is now possible to register/unregister
 * message handlers using @ref eventbus_register and @ref eventbus_unregister
 * respectively and to send/publish messages on the event bus using @ref
 * eventbus_send and @ref eventbus_publish respectively. The client can be
 * stopped using @ref eventbus_stop and restarted multiple times, however you
 * will have to register the handlers using @ref eventbus_register after each
 * restart, i.e. handlers registered in previous sessions will no longer be
 * active.
 *
 * @param instance The event bus client.
 *
 * @return 0 in case of success, -1 otherwise. Inspect the value of @ref
 * eventbus_error in case of error.
 */
int eventbus_start(eventbus_t *instance);

/**
 * Retrieves the event bus node for this client.
 *
 * @param instance The event bus client.
 *
 * @return The event bus endpoint node for this instance (e.g. "localhost")
 */
const char *eventbus_node(eventbus_t *instance);

/**
 * Retrieves the event bus service(i.e. port) for this client.
 *
 * @param instance              The event bus client.
 *
 * @return The event bus endpoint service for this instance (e.g. "7000")
 */
const char *eventbus_service(eventbus_t *instance);

/**
 * Retrieves event bus client user data.
 *
 * @param instance The event bus client.
 *
 * @return A pointer to the user-defined data structure passed upon creation.
 */
void *eventbus_user(eventbus_t *instance);

/**
 * Retrieves event bus last errror.
 *
 * @param instance The event bus client.
 *
 * @return An error code. Refer to @ref eventbus_error_t for details about error codes.
 */
eventbus_error_t eventbus_error(eventbus_t *instance);

/**
 * Pings the event bus. Remark: the server does not respond to pings.
 *
 * @param instance The event bus client.
 *
 * @return 0 in case of success, -1 otherwise. Inspect the value of @ref
 * eventbus_error in case of error.
  */
int eventbus_ping(eventbus_t *instance);

/**
 * Sends a message to a specific address on the event bus.
 *
 * @param instance The event bus client.
 *
 * @param address The destination address for the new message.
 *
 * @param reply_address The destination address for the reply to this message or
 * NULL.
 *
 * @param headers Message headers or NULL.
 *
 * @param body Message body or NULL.
 *
 * @return 0 in case of success, -1 otherwise. Inspect the value of @ref
 * eventbus_error in case of error.
 *
 */
int eventbus_send(eventbus_t *instance,
                  const char *address,
                  const char *reply_address,
                  json_t *headers,
                  json_t *body);

/**
 * Publishes a message to a specific address on the event bus.
 *
 * @param instance The event bus client.
 *
 * @param address The destination address for the new message.
 *
 * @param reply_address (optional) The destination address for the reply to this
 * message.
 *
 * @param headers Message headers or NULL.
 *
 * @param body Message body or NULL.
 *
 * @return 0 in case of success, -1 otherwise. Inspect the value of @ref
 * eventbus_error in case of error.
 */
int eventbus_publish(eventbus_t *instance,
                     const char *address,
                     json_t *headers,
                     json_t *body);

/**
 * Registers an handler on the event bus at a given address.
 *
 * @param instance The event bus client.
 *
 * @param address The address onto which to register the handler.
 *
 * @param handler The handler to be registered. The handler will be invoked to
 * service incoming messages sent to the address.
 *
 * @return 0 in case of success, -1 otherwise. Inspect the value of @ref
 * eventbus_error in case of error.
 */
int eventbus_register(eventbus_t *instance,
                      const char *address,
                      handler_t handler);

/**
 * Unregisters the handler on the event bus at a given address.
 *
 * @param instance The event bus client.
 *
 * @param address The address onto which to register the handler.
 *
 * @return 0 in case of success, -1 otherwise. Inspect the value of @ref
 * eventbus_error in case of error.
 */
int eventbus_unregister(eventbus_t *instance,
                        const char *address);

/**
 * Stops the event bus client. Recovers from a previously encountered error, if
 * any. Stopping the client prevents it from receiving messages from the bridge
 * and makes it uncapable of sending/publishing messages on the bridge. The
 * client can be restarted using @ref eventbus_start.
 *
 * @param instance The event bus client.
 *
 * @return 0 in case of success, -1 otherwise. Inspect the value of @ref
 * eventbus_error in case of error.
 */
int eventbus_stop(eventbus_t *instance);

/**
 * Destroys the event bus client. If the instance is still running when this
 * function is called, @ref eventbus_stop will automatically be called on it
 * first.
 *
 * @param instance The event bus client.
 *
 * @return 0 in case of success, -1 otherwise. Inspect the value of @ref
 * eventbus_error in case of error.
 */
int eventbus_destroy(eventbus_t *instance);

#ifdef __cplusplus
}
#endif

#endif /* VERTX3_TCP_EVENTBUS_BRIDGE_CLIENT_H */
