# PLEB (Process-Local Event Bus)
⚠ This library is in early development.  The API is subject to change.

PLEB is a header-only library extending C++17 with common patterns from network programming.  It provides a concurrent global **resource tree** similar to the index of a web server, implementing Request-Reply and Publish-Subscribe messaging patterns as lightweight function calls using any data structure.

PLEB's design is based on the following observations:

1. Request-Reply (REST) and Publish-Subscribe are excellent patterns for messaging and error handling — not only across networks. but also within programs with multiple modules or threads.
2. In-process messages do not necessarily need serialization or queueing, both of which create potentially unnecessary work.
3. Global pathnames are more manageable than references or GUIDs when communicating across a complex program.

PLEB does not replace messaging frameworks like ZeroMQ and NNG; rather, it can act as a front-end.  Where traditional frameworks treat in-process communication as a special case of network sockets, PLEB allows us to do the opposite by using local resources as an interface to remote ones.

Resources have path-like names such as `output/1` .  PLEB messages are realized as function calls using `std::any` as a generic container.  PLEB is multi-threaded and (mostly*) wait-free, meaning it can be used for extremely time-sensitive concurrent applications.  While PLEB is thread-safe for purposes of setting up the resource tree and issuing messages, it imposes no locks or message queueing — the application is expected to impose its own concurrency measures.

(* PLEB's resource tree uses locking operations when adding resources or looking them up, pending the integration of a suitable wait-free hash table algorithm,)



## Quick  Examples

```C++
#include <pleb.h>

class MyService
{
public:
    std::shared_ptr<pleb::service> pleb_service;
    
    void handshake(pleb::request &request) {request.reply("Howdy");}
}

void main()
{
    auto service = std::make_shared<MyService>();
    service->pleb_service = pleb::serve(
        "/service/handshake",
		service, &MyService::handshake);
    
    pleb::reply reply = pleb::GET("/service/handshake");
}
```

## How are Messages Processed?

When a Request is issued to a resource, or an Event is published, lock-free algorithms are used to identify the recipient (of a Request) or recipients (of an Event) and invoke them.  Handlers are implemented as a `std::function` accepting a reference to the message.  The value passed with the message, if any, is contained within a generic `std::any` container.

The primary thread safety mechanism used by the resource tree is `std::weak_ptr::lock()`, typically a non-blocking operation.  `std::function` and `std::any` allow us to invoke any logic we like for the cost of two virtual function calls.  On the rare occasion where this is too much overhead for some real-time operation, a request may be used at set-up time to provide a direct reference to some low-level mechanism.

## A Native Event Bus

PLEB can be used for intra-process communications similar to a message queue framework, but with essential differences that leverage the advantages of communicating within a local process:

* Events may exchange any native object, not just binary data.
  * This can avoid unnecessary serialization and deserialization.
  * This allows for RAII, reference counting, synchronization mechanisms and more.
* Events are handled by calling functors.
  * This allows for queue-free operation and immediate fulfillment of certain requests.
  * This allows exceptions to be thrown directly or via futures.
* Event handling is completely reliable as long as the origin and destination exist.
  * In many cases, this alleviates the need for loss-tolerance or idempotence.
  * Callers may cache a reference to a named resource or topic for repeated interactions.

In this sense, PLEB can be framed as an event bus API which removes implementation burden as well as resource burden in time-critical applications.  It is designed to facilitate 



## Requests, Events and other Patterns

PLEB facilitates two primary messaging patterns:  a `request` to a single service or an `event` for any number of subscribers.  Values passed to receivers are wrapped in `std::any`.  Receivers will typically support a limited set of value types, treating unsupported types as an error.

Requests and event handlers are realized by direct calls to bound `std::function` wrappers.  PLEB itself provides no intrinsic thread safety in passing events and requests; instead, it is expected that these mechanisms will be built into the registered functions themselves.  Replies to requests may be fulfilled using `std::future`, which is thread-safe.

Network microservice libraries often include additional communication patterns.  Because PLEB works within a single process, "PAIR" and "PUSH_PULL" topologies can be implemented with PLEB's requests, while SURVEYOR and STAR topologies can be implemented with PLEB's event publishing mechanism.

A surveyor-respondent pattern may be implemented by publishing an object capable of collecting responses.



## Publish-Subscribe Pattern

Publish/Subscribe logic may be implemented using PLEB's topic tree.  Topics may have an arbitrary number of subscriber functions, which will be invoked when a value is published to the topic or any of its children.

The topic tree may additionally be used to implement a surveyor pattern, by publishing a reference to some mutable object.

Publish/subscribe is synchronous; any thread safety must be managed by the published object and/or subscriber function.  (TODO: provide utilities for this)



## Request-Reply Pattern

Requests in PLEB utilize HTTP methods:

* **GET** retrieves data without modifying it.



Replies to requests may be implemented by replacing the request object in the supplied parameter.

Requests are synchronous; any thread safety must be managed by the published object, returned object and/or service function.  (TODO: provide utilities for this)

We recommend the following conventions for implementing CRUD methods:

* **CREATE** objects by making a request to a parent service with parameters for the object to be created.
* **READ** data by making a request with an empty `std::any` object.
* **UPDATE** data by making a request to the object with a new value or patch data.
* **DELETE** data by making a request to the object with a tag type.

The push-pull messaging pattern is a specialization of the request pattern where no `std::future` is provided and thus no reply is possible.  This typically yields a performance advantage, and may affect the choice of protocol used to fulfill networked requests.



## Automatic Type Conversion

PLEB uses `std::any` to exchange values of arbitrary type, but in some cases it may be desirable for producers and consumers of information to work with different representations of the data.  Usually this is because certain systems (such as networking and files) need to work with serialized data while others need to work with native objects.

PLEB provides a global lookup table mapping `std::type_index` pairs to type conversion functions.  When a callee attempts to retrieve a value whose type differs from the type provided, the type conversion corresponding to the provided type and the desired type may be invoked.  Most often this will be a serialization or marshalling function.

A secondary table is provided for patching operations, where the source type describes a set of modifications to be made to the destination type.  When no patching rule is available, PLEB falls back to type conversion rules and attempts to overwrite the destination type.

Type conversion functions are contained in a cooperative collection (see below); this means that their registrations must be strongly referenced as long as they are needed.



# Design and Idioms

## Smart Pointers

PLEB uses C++11's `shared_ptr` and `weak_ptr` to solve the most common thread safety issue with inter-thread resources and calls:  destruction.  Whenever a service or subscriber receives a call, PLEB uses `weak_ptr::lock()` to ensuring the object exists until the call is completed.  (Locking a weak pointer is a non-blocking operation.)  When binding calls to methods on an application class, PLEB expects a `weak_ptr` as well.

Normally, `shared_ptr` and `weak_ptr` require the indicated object to be allocated on the heap.  This can be inconvenient when an object would otherwise exist on the stack or as a member of another object.  For this reason, we provide a utility called `life_lock` which allows *any* object to generate weak pointers to itself, and which only blocks if these have been locked as `shared_ptr` when the object is destroyed.

## Cooperative Data Structures

PLEB is based on "cooperative data structures", in which collections are owned (strongly referenced) by the elements inside them.  For example, subscribers to a topic own a subscription which owns a topic and all its ancestors.  Services own the resource on which they serve, which owns its parent resources.

The only way to remove an element from a cooperative collection is to allow that element to expire by releasing all strong references (that is, `shared_ptr`'s) to it.  The only way to destroy a cooperative is for all strong references to the collection and its elements and sub-elements to expire.  This way, a resourc

These properties make cooperatives well-suited to coordinating producers with consumers, services with clients and publishers with subscribers.



## Trie Structure for Resources & Topics

The essential data structures used by PLEB are wait-free tries resembling directory structures.  The root node of each trie is a global variable, with child nodes identified by unique strings.  Each child node may in turn have child nodes.

Levels in the trie are defined by strings rather than single characters.  Trie nodes can be accessed using `path_view` which delimits a string by runs of forward slash characters `/`, ignoring leading and trailing slashes.  Thus, a path like `//voices/1/config` refers to the `config` node within the `1` node within the `voice` node of the resource root.

These tries follow the cooperative structure described above:  child nodes share ownership over parent nodes, and are only removed from the trie when all strong references to them expire.
