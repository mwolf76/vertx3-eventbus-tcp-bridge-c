#include <assert.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <jansson.h>
#include <unistd.h>

#include <time.h>
#include <sys/time.h>

#include "eventbus.h"

#define MAX_TIMESTAMP_LENGTH (100)
#define MAX_TIMESTAMP_PREFIX (40)

/* a useful logging macro :-) */
#define LOG(stream, msg, ...)                                         \
    do {                                                              \
        char timestamp[MAX_TIMESTAMP_LENGTH];                         \
                                                                      \
        time_t time_;                                                 \
        struct tm *ltime;                                             \
        static struct timeval t_;                                     \
        static struct timezone tz;                                    \
                                                                      \
        time(&time_);                                                   \
        ltime = (struct tm *) localtime(&time_);                        \
        gettimeofday(&t_, &tz);                                         \
        strftime(timestamp, MAX_TIMESTAMP_PREFIX,                       \
                 "%Y-%m-%d %H:%M:%S", ltime);                           \
        sprintf(timestamp, "%s.%d", timestamp, (int)t_.tv_usec);        \
        fprintf(stderr, "%s at %s:%d :: " msg "\n",                     \
                timestamp, __FILE__, __LINE__, ##__VA_ARGS__);          \
    } while (0)

/* example user-defined struct used to pass arbitrary data to handlers*/
typedef struct {
    FILE *stream;
    uint32_t counter;
} user_t ;

static void data_handler(eventbus_t *instance, const json_t *obj)
{
    assert(instance);

    /* retrieve user data */
    user_t *data = (user_t *) eventbus_user(instance);

    char *s = json_dumps(obj, 0);
    LOG(data->stream, "Got message from server: %s", s);

    json_t* body = json_object();
    const char *reply_address = json_string_value(json_object_get(obj,
                                                                  "replyAddress"));

    json_object_set_new(body, "status", json_string("ACK"));
    eventbus_send(instance, reply_address, NULL, NULL, body);

    ++ data->counter;
    free(s);
}

static void error_handler(eventbus_t *instance, const json_t *obj)
{
    assert(instance);

    /* retrieve user data */
    user_t *data = (user_t *) eventbus_user(instance);

    char *s = json_dumps(obj, 0);
    fprintf(data->stream, s);
    fprintf(data->stream, "\n");

    ++ data->counter;
    free(s);
}

int main()
{
    int rc;

    /* user defined data */
    user_t user = {
        stdout, 0
    };

    /* create client instance */
    eventbus_t *instance = eventbus_create(NULL, NULL, error_handler, &user);
    if (! instance) {
        LOG(stderr, "Could not create instance");
        exit(1);
    }
    LOG(stderr, "Eventbus client instance created");

    /* start the client */
    rc = eventbus_start(instance);
    if (rc) {
        LOG(stderr, "Could not start event bus client");
        exit(1);
    }
    LOG(stderr, "Eventbus client instance started");

    /* register msg handlers */
    rc = eventbus_register(instance, "data", data_handler);
    if (rc) {
        LOG(stderr, "Could not start register message handler");
        exit(1);
    }
    LOG(stderr, "Registered data handler");

    /* Mind your own business ... */
    for (int i = 0; i < 10; ++ i) {
        sleep(1);
        rc = eventbus_ping(instance);
        if (rc) {
            LOG(stderr, "Could not ping");
            exit(1);
        }
        LOG(stderr, "Ping...");

        /* send messages to the event bus */
        json_t* obj = json_object();
        json_object_set_new(obj, "value", json_integer(42));
        rc = eventbus_send(instance, "temperature", NULL, NULL, obj);
        if (rc) {
            LOG(stderr, "Could not send message to address cinderella");
            exit(1);
        }
    }

    /* Stop the client */
    rc = eventbus_stop(instance);
    if (rc) {
        LOG(stderr, "Could not stop event bus client");
        exit(1);
    }
    LOG(stderr, "Event bus client stopped");

    rc = eventbus_destroy(instance);
    if (rc) {
        LOG(stderr, "Could not destroy event bus client");
        exit(1);
    }
    LOG(stderr, "Event bus client destroyed");

    LOG(stderr, "%d messages processed.", user.counter);
    return 0;
}
