# ConsistentHashing

Simulating a multi-instance cache router, with consistent hashing.

An in-memory cache cluster with pluggable routing, served over HTTP with a
browser UI that visualises where keys land. Switch between a modulo router and a
consistent-hashing ring and watch how many keys get remapped when the node set
changes.

[Video Walkthrough](https://youtu.be/e7afhZCe9Ko)

## Requirements

- CMake 4.0 or newer
- A C++23 compiler
- Network access on the first configure (dependencies are fetched by CMake)

Dependencies are pulled in automatically: cpp-httplib 0.18.0, nlohmann/json
3.11.3, and GoogleTest 1.15.2. MurmurHash3 is vendored in `external/`.

## Build

```sh
cmake -S . -B build
cmake --build build
```

## Run

The web root is resolved relative to the working directory, so start the server
from the repository root:

```sh
./build/src/server/cacherouter_server
```

Then open http://localhost:8080.

Options:

| Flag            | Default | Notes                                  |
| --------------- | ------- | -------------------------------------- |
| `--port=N`      | `8080`  | Listen port                            |
| `--debug=LEVEL` | `off`   | One of `trace`, `debug`, `info`, `off` |

Event logs go to stderr. Severity follows volume: `trace` covers cache hits and
routing decisions, `debug` adds misses, inserts and updates, and `info` reports
evictions only.

```sh
./build/src/server/cacherouter_server --port=9000 --debug=info
```

The cluster starts with ten nodes (`node-A` through `node-J`), each with a
capacity of 50 entries, an LRU eviction policy, and 100 virtual nodes on the
ring.

## Demonstration (Default Cache Configuration)

1. Start with consistent routing. Start simulating traffic, wait around 30 seconds for the cache to be saturated (Hit rate > 90\%)
2. Add a new node, see how many keys is remapped and how much the hit rate goes down.
3. Stop the simulation, reset to default, switch the routing algorithm to simple routing.
4. Repeat step 1 and 2 and see the differences.

Expected behaviour:

- The hit rate reach >90\% after 30 seconds
- Consistent routing: only 10\% of the key space get remapped, the hit rate only slightly drop on topology change.
- Simple routing: arount 90\% of the key space get remapped, the hit rate only significantly drop on topology change.

## Test

Run the test binary directly:

```sh
./build/test/cacherouter_tests
```

It is self-contained and reports each failing assertion with its file and line.
Useful flags while working:

```sh
./build/test/cacherouter_tests --gtest_filter='HashRing.*'   # one suite
./build/test/cacherouter_tests --gtest_brief=1               # failures only
./build/test/cacherouter_tests --gtest_repeat=100            # hunt flakiness
./build/test/cacherouter_tests --gtest_output=xml:results.xml
```

CTest can also drive the same tests. `gtest_discover_tests` registers every test
as its own entry, so each one runs in a separate process and a crash costs only
that test rather than the whole run:

```sh
ctest --test-dir build --output-on-failure
ctest --test-dir build -j8          # in parallel
ctest --test-dir build -R HashRing  # by name
```

Pass `--output-on-failure`, or CTest reports only which test failed and not why.

Covers the eviction policy, the hash ring, both routers, the cluster, and the
event log.

## HTTP API

| Method | Path              | Purpose                                        |
| ------ | ----------------- | ---------------------------------------------- |
| GET    | `/api/nodes`      | List nodes with capacity and current usage     |
| POST   | `/api/nodes`      | Add a node                                     |
| DELETE | `/api/nodes/:id`  | Remove a node                                  |
| GET    | `/api/cache/:key` | Read a key                                     |
| PUT    | `/api/cache/:key` | Write a key                                    |
| GET    | `/api/router`     | Active router, available routers, and topology |
| POST   | `/api/router`     | Switch router                                  |
| POST   | `/api/reset`      | Clear data, clear nodes, or restore defaults   |

Add a node:

```sh
curl -X POST localhost:8080/api/nodes -H 'Content-Type: application/json' \
  -d '{"node-id":"node-K","capacity":50,"policy":"lru","virtual_nodes":100}'
```

`virtual_nodes` is optional and defaults to 100. It is fixed once the node is
created.

Switch routers. This clears all cached entries, so both routers are compared
from a cold start:

```sh
curl -X POST localhost:8080/api/router -d '{"name":"simple"}'
```

Reset, where `scope` is `data` (drop entries, keep nodes), `all` (remove every
node), or `default` (restore the starting cluster):

```sh
curl -X POST localhost:8080/api/reset -d '{"scope":"default"}'
```

Key hashes are 64-bit and are returned as JSON strings. A JSON number would lose
the low bits to double precision, which is exactly what the modulo router routes
on.

`/api/router` returns a topology shaped for the active router: a list of ring
points for `consistent`, an ordered bucket list for `simple` where the index is
`hash % node_count`.

## Layout

```
include/cacherouter/   Public headers
src/cacherouter/       Core library (cacherouter::core)
src/server/            HTTP server and API handlers
web/                   Frontend, served as static files
test/                  GoogleTest suite
external/murmur3/      Vendored hash function
```

The core library has no HTTP or JSON dependency. MurmurHash3 is linked privately,
so consumers get `cacherouter::hash` without inheriting the vendored headers.

## Routers

`simple` picks a node with `hash(key) % node_count`. It is easy to reason about
and remaps most of the keyspace whenever the node count changes.

`consistent` places each node at many points on a 64-bit ring and walks clockwise
from the key's hash. Adding a node to an N-node cluster moves roughly `1/(N+1)`
of the keys instead of nearly all of them. The UI reports the measured figure
after every topology change.

## Limitations

- State is in memory only and is lost on restart.
