# PLEB (Process-Local Event Bus)
PLEB is a header-only C++ library for high-performance message and event handling within a multi-threaded process.  It provides functionality comparable to message queue libraries but with essential differences that leverage the nature of communications within a single, multi-threaded process:

* Native objects may be exchanged rather than serialized data.
  * These objects may have RAII functionality, reference counting, synchronization mechanisms and other side-effects.
* Events are handled by calling functors.
  * This allows for queueless or even synchronous interoperation between agents.
  * This allows exceptions to be thrown directly or via futures when 
* Event handling is completely reliable as long as the origin and destination exist.
  * In many cases, this alleviates the need for loss-tolerance or idempotence.
  * Callers may cache a reference to a named resource or topic for repeated interactions.

In this sense, PLEB can be framed as an event bus API which removes implementation burden as well as resource burden in time-critical applications.  It is designed to facilitate 



## Event Processing

Events in PLEB are implemented by passing `std::any` objects to receivers (services or subscribers).  Receivers will typically support a limited set of parameter types, throwing an exception for unsupported types.  Receiver functions are always called synchronously, although it is common for services and subscribers to implement some kind of asynchronous function.

When making a request, an exception may be thrown if the corresponding service does not exist, and no "catch-all" service is provided by a parent path.



## Publish-Subscribe Pattern

Publish/Subscribe logic may be implemented using PLEB's topic tree.  Topics may have an arbitrary number of subscriber functions, which will be invoked when a value is published to the topic or any of its children.

The topic tree may additionally be used to implement a surveyor pattern, by publishing a reference to some mutable object.

Publish/subscribe is synchronous; any thread safety must be managed by the published object and/or subscriber function.  (TODO: provide utilities for this)



## Request-Reply Pattern

It is desirable to implement REST operations with PLEB's request mechanism

Replies to requests may be implemented by replacing the request object in the supplied parameter.

Requests are synchronous; any thread safety must be managed by the published object, returned object and/or service function.  (TODO: provide utilities for this)

We recommend the following conventions for implementing CRUD methods:

* **CREATE** objects by making a request to a parent service with parameters for the object to be created.
* **READ** data by making a request with an empty `std::any` object.
* **UPDATE** data by making a request to the object with a new value or patch data.
* **DELETE** data by making a request to the object with a tag type.



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
