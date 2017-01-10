Cache
=====

Cache plugin provides an interface to the underlying LRU (least recently used) cache allowing the cloud users to put/get
string mapped to string items.

## Configuration

To configure this service add the following lines into the *services* section.

```json
"cache": {
    "type": "cache",
    "args": {
        "max-size": 10000
    }
}
```

This will configure the cache service with maximum size of `10'000` **items** (not bytes). The default size is
`1'000'000`.

## API

The service provides the following methods

#### Put

Attempts to put the given `value` under the specified `key` into the cache.

##### Signature

```cpp
auto put(const std::string& key, const std::string& value) -> void;
```

##### Protocol
The `option_of<void>` - means that it returns either an empty response on success or an error tuple on fail.

#### Get

Attempts to get the value that is probably lies under the given `key` from the cache.

Returns a tuple of boolean flag with string. The `true` flag means that the value was found in the cache, `false`
otherwise.

##### Signature

```cpp
auto get(const std::string& key) -> std::tuple<bool, std::string>;
```

##### Protocol
The `option_of<T> where T: std::tuple<bool, std::string>` - means that the method returns either an filled response on
success or an error tuple on fail.
