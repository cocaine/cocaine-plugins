== What is it? ==
This plugin provides a service, which can be used:
  * To store applications configurations
  * To store any management data used by cocaine
  * To provide consistent publish-subscribe mechanism
  * To be used as distributed locking service
== API ==
Service defines following methods:
  * create(string path, variant value) -> error || bool.
   Creates node with specified value if it does not exist. Returns error otherwise.
  * put(string path, variant value, uint version) -> error || (bool, variant value, uint version).
   Puts value to specified node path. Returns tuple of bool(indicates if put was performed by this call), value(current value), version(current version).
  * subscribe(string path) -> stream of (variant value, uint version) || error.
   Subscribes for specified path. Streams updates(including initial value) to rx. Updates are guaranteed to be sent in version ascending order.
  * children_subscribe(string path) -> stream_of (list of string) || error
   Subscription for node children. Streams list of node names(including starting list) - children of specified node.
  * del(string path, uint version) -> error || bool.
   Deletes a node if version matches. Error otherwise.
  * increment(string path, variant value) -> error || (variant value, uint version).
   Increments node's value by specified value and returns resulting value. If one of operand is non-numeric returns error, if one operand is floating point returns floating point, otherwise returns int. Value can be negative.
  * lock(string path) -> void
   Creates lock on specified path. Path SHOULD NOT be used for other purposes. If lock is  aquired by other app blocks until lock is released.

After any call close method is defined, which can be used to cancel subscription or release lock or just close channel.