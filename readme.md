# Mini Project 2

Mini-project 2 from CS3.301 Operating Systems and Networks, Monsoon 2026

## networking

### Build

```bash
cd networking
make all      # builds tempest/tempest and mastermind/mastermind
make clean
```

### tempest

```bash
tempest <city>
tempest <city> --raw
```

#### Design

<!-- how you split the code into files, how you read the response body, and how you detect an invalid location -->

The code is split across four files:
- `cli.c`: handles argument parsing and flags, throwing errors right away if the city name is missing or invalid.
- `http.c`: URL-encodes the city, formats the GET request, and splits raw responses into header and body sections.
- `net_socket.c`: resolves wttr.is via getaddrinfo, sets up non-blocking sockets, and runs poll loops for send and receive.
- `main.c`: drives the overall flow and ignores SIGPIPE so a sudden disconnect does not crash the binary.

Reading the response body: We start with an 8 KB buffer on the heap and pull data using recv inside a poll loop. Whenever headroom drops under 1 KB, we double the buffer size with realloc. Since the request asks for Connection: close, the server shuts down the socket right after sending everything. Recv hits EOF by returning zero bytes. We then find \r\n\r\n to separate the headers from the actual body text.

Detecting an invalid location: wttr.is usually gives a 404 or 500 status line when a city does not exist, but occasionally it returns 200 OK while printing location not found inside the page. We check for both cases. If the status is not 200 or the body contains that error string, we treat the location as invalid. When --raw is enabled, we dump the raw headers and body to stdout first, then print tempest: invalid location to stderr and exit with code 1.

#### Changed defaults

<!-- default: request timeout 10 s; write "none" if unchanged -->

The 10-second timeout budget resets for each phase: connection, request transmission, and response reception. Each loop calculates its remaining time using clock_gettime with CLOCK_MONOTONIC and terminates with tempest: request timed out if poll expires.

#### Assumptions

<!-- anything you took for granted that the spec did not state -->

wttr.is is reachable over plain HTTP on port 80 and closes the connection once it finishes sending the response. City names fit in memory as standard strings.

#### Known bugs

<!-- what does not work, and when; write "none known" if there are none -->

none known

### mastermind

```bash
mastermind [--cost-cutting] [--log]
```

#### Design

<!-- how you split the code into files, and what each file is responsible for -->

#### Discovery

<!-- the 4-byte magic and the broadcast payload layout; the sender's ip comes from the packet source, not the payload -->

#### Message verbs

<!-- every verb, its payload, and when it is sent; must match your .h -->

#### Reliable transfer over UDP (`--cost-cutting`)

<!-- the chunk struct, sequence numbering, how the total chunk count is sent, and the ack/retransmit scheme -->

#### Disconnection handling

<!-- how you detect that the other player is gone -->

- TCP:
- UDP:

#### Changed defaults

<!-- defaults: broadcast every 2 s, entries expire after 5 s, retransmit after 0.1 s; write "none" if unchanged -->

#### Assumptions

<!-- anything you took for granted that the spec did not state -->

#### Known bugs

<!-- what does not work, and when; write "none known" if there are none -->

## xv6

### Build

```bash
cd xv6
make qemu     # boot xv6; Ctrl-A X to quit
make clean
```

### Run

```bash
# in the xv6 shell
cowtest
alarmtest
usertests -q

# or from outside
./test-xv6.py cowtest
./test-xv6.py alarmtest
./test-xv6.py -q usertests
```

### Copy-on-write fork

<!-- where the reference counts live and how they are locked, which pte bit marks a page cow, and how a write fault duplicates a page -->

### Alarms

<!-- the fields you added to struct proc, what state sigreturn restores, and how you stop a handler being re-entered -->

### Assumptions

<!-- anything you took for granted that the spec did not state -->

### Known bugs

<!-- what does not work, and which test shows it; write "none known" if there are none -->
