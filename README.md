# vertx3-eventbus-tcp-bridge-c
Vert.X 3 TCP event bus bridge client library

This library provides a C99 interface to the Vert.x 3 event bus via the TCP
event bus bridge. Using this library, low-level components written in C can
be transparently integrated in a Vert.x 3 distributed application using the
event bus to exchange messages with each other. This approach can be useful
for instance in an embedded application, in which a C program performs
low-level control of an actual embedded device while business logic is
implemented in an higher-level application component deployed on the JVM
using the Vert.x toolkit. For more information on the Vert.x 3 TCP event bus
bridge refer to http://vertx.io/docs/vertx-tcp-eventbus-bridge/java.
