# PLEB (Process-Local Event Bus)
⚠ This library is in early development.  The API is subject to change.

PLEB is a header-only C++17 library that provides a **resource tree** for microservices within a process.  It is multi-threaded and (mostly*) wait-free, meaning it can be used for highly time-sensitive concurrent applications.  The resource tree is (usually) global and indexed by names similar to file paths.  PLEB does not include any protocols for communicating with other processes or machines.  Instead, it is designed to act as an interface between application code and external communications.

Resources allow us to **publish** events to any number of subscribers, to make a **request** to a single service, or both.  These communications are are realized as function calls.  While PLEB is thread-safe for purposes of setting up and making these calls, it imposes no locks or message queueing — instead, the application is expected to impose its own concurrency measures.

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
    
    pleb::reply reply = pleb::request("/service/handshake");
}
```



## Smart Pointers

PLEB uses C++11's `shared_ptr` and `weak_ptr` to solve the most common thread safety issue with inter-thread function calls:  the 

Normally, `shared_ptr` and `weak_ptr` require the indicated object to be allocated on the heap.  This can be inconvenient when an object would otherwise exist on the stack or as a member of another object.  For this reason, we provide a utility called `life_lock` which allows *any* object to generate weak pointers to itself, and which only blocks if these have been locked as `shared_ptr` when the object is destroyed.


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

**NOT YET IMPLEMENTED**

PLEB uses `std::any` to exchange values of arbitrary type, but in some cases it may be desirable for producers and consumers of information to work with different representations of the data.  Usually this is because certain systems (such as networking and files) need to work with serialized data while others need to work with native objects.

PLEB provides a lookup table mapping `std::type_index` pairs to type conversion functions.  When a callee attempts to retrieve a value whose type differs from the type provided, the type conversion corresponding to the provided type and the desired type will be invoked.  Most often this will be a marshalling or unmarshalling function of some kind.

A secondary table is provided for patching operations, where the source type describes a set of modifications to be made to the destination type.  When no patching rule is available, PLEB falls back to type conversion rules and attempts to overwrite the destination type.

Type conversion functions are contained in a cooperative collection (see below); this means that their registrations must be strongly referenced as long as they are needed.



## Cooperative Data Structures

PLEB is based on wait-free "cooperative data structures", in which collections are owned by their elements.  For example, subscribers to a topic strongly reference a subscription which strongly references a topic and all its ancestors.  Services providing a resource own that resource which owns its parent resources.

The only way to remove an element from a cooperative collection is to allow that element to expire by releasing all strong references (that is, `shared_ptr`'s) to it.  The only way to destroy a cooperative is for all strong references to the collection and its elements to expire.

These properties make cooperatives well-suited to coordinating producers with consumers, services with clients and publishers with subscribers.



## Trie Structure for Resources & Topics

The essential data structures used by PLEB are wait-free tries resembling directory structures.  The root node of each trie is a global variable, with child nodes identified by unique strings.  Each child node may in turn have child nodes.

Levels in the trie are defined by strings rather than single characters.  Trie nodes can be accessed using `path_view` which delimits a string by runs of forward slash characters `/`, ignoring leading and trailing slashes.  Thus, a path like `//voices/1/config` refers to the `config` node within the `1` node within the `voice` node of the resource root.

These tries follow the cooperative structure described above:  child nodes share ownership over parent nodes, and are only removed from the trie when all strong references to them expire.
