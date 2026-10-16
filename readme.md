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

none

#### Known bugs

<!-- what does not work, and when; write "none known" if there are none -->

none known

### mastermind

```bash
mastermind [--cost-cutting] [--log]
```

#### Design

<!-- how you split the code into files, and what each file is responsible for -->

The implementation is organized across seven files:
- `discovery.c`: handles UDP peer beacon broadcasts on port 8888, self-filtering, and pruning inactive players.
- `protocol_tcp.c`: manages TCP connection handshakes, challenge prompts, and the synchronous turn loop.
- `protocol_udp.c`: implements chunked reliable transmission over UDP with ACKs, retransmits, and message IDs.
- `engine.c`: validates sequence formats, checks guesses against secrets, and generates feedback scores.
- `ui.c`: draws the game board, player tables, and disconnection notifications.
- `logger.c`: appends timestamped protocol events to log.txt when running with --log.
- `main.c`: handles CLI flags, sets up the poll loop, and directs user commands from stdin.

#### Discovery

<!-- the 4-byte magic and the broadcast payload layout; the sender's ip comes from the packet source, not the payload -->

Peers broadcast UDP datagrams to port 8888 every 2 seconds using the layout MST1|<name>|<port>. The MST1 prefix drops random network noise right away. The receiving end gets the sender IP directly from the sockaddr_in structure in recvfrom, while the payload itself stays limited to the player name and listening port. To stop a node from adding itself to its own lobby, is_self compares the incoming packet IP against local interface addresses from getifaddrs alongside the listening port.

#### Message verbs

<!-- every verb, its payload, and when it is sent; must match your .h -->

The protocol runs five message exchanges across both TCP and UDP modes:
- Challenge request: carries the challenger's name as a string. Sent by the initiating player to propose a match.
- Challenge response: carries "yes" or "no". Sent by the target player to accept or decline the game.
- Guess: carries a 5-digit sequence string like "12345". Sent by the Codebreaker on each turn.
- Feedback: carries a 5-character string of 'o', 'x', and '-' marks. Sent by the Mastermind after evaluating a guess.
- Reveal secret: carries the 5-digit solution sequence. Sent by the Mastermind once the game concludes.

#### Reliable transfer over UDP (`--cost-cutting`)

<!-- the chunk struct, sequence numbering, how the total chunk count is sent, and the ack/retransmit scheme -->

In cost-cutting mode, application messages travel inside UdpChunk structs over UDP. Each chunk carries an integer msg_id, a 0-indexed seq_num, total_chunks, an is_ack flag, and a 4-byte data payload. Every chunk embeds total_chunks in its header, calculated as the message length rounded up in 4-byte increments.

Delivery uses selective ACKs. When receiving a data chunk, the node immediately returns an ACK chunk bearing the matching msg_id and seq_num. The sender polls for ACKs and retransmits any unacknowledged chunk if 100 milliseconds pass without confirmation. After 50 retries, it treats the peer as disconnected. The msg_id isolates chunks across separate transmissions so delayed packets never corrupt subsequent messages.

#### Disconnection handling

<!-- how you detect that the other player is gone -->

- TCP: The socket read loop calls recv. When the peer closes their terminal or drops the link, recv returns zero bytes on EOF or a negative value on connection reset. The game catches this immediately and marks the match aborted.
- UDP: For UDP, recv_udp_chunked runs a poll call with a 30-second timeout on each turn. If no chunk arrives within 30 seconds, or if send_udp_chunked exhausts 50 retries, the opponent is declared disconnected. In the discovery lobby, players that miss broadcasts for over 5 seconds are purged.

#### Changed defaults

<!-- defaults: broadcast every 2 s, entries expire after 5 s, retransmit after 0.1 s; write "none" if unchanged -->

none

#### Assumptions

<!-- anything you took for granted that the spec did not state -->

We assume player names never contain the pipe symbol | because we use it as the token delimiter in broadcast packets.

In UDP cost-cutting mode, if an opponent takes longer than 30 seconds to submit a guess or feedback, we assume they left the game and trigger a disconnect.

#### Known bugs

<!-- what does not work, and when; write "none known" if there are none -->

none known

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
