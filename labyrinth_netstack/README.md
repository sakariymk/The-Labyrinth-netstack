# Networked Labyrinth - README

## Overview

This exam implements a networked labyrinth solver. It features emulated Link Layer (L2) and Transport Layer (L4) functionalities built upon UDP, along with an Application Layer (L5) component that interacts with a server to request, solve, and return a labyrinth.

The system operates as follows:
1.  **L2 (Link Layer - `l2sap.c`):** Handles sending and receiving L2 frames as UDP datagrams between the client and a server. It manages frame size limits (1024 bytes total), L2 headers, and checksum validation (byte-wise XOR). Corrupted or oversized frames are discarded. Timeouts are handled using `select()`.
2.  **L4 (Transport Layer - `l4sap.c`):** Implements the Stop-and-Wait protocol on top of L2 for data transfer. It deals with L4 packets, sequence numbers (0/1), acknowledgments (ACKs), data truncation (max 1012 byte payload), retransmissions on timeout (1-second timeout, up to 4 retries for 5 total attempts), and termination (`L4_RESET`).
3.  **L5 (Application/Maze Solver - `maze.c`):** Contains the logic (`mazeSolve`) to find a path through a labyrinth received from the server. The client requests the maze, solves it by marking the path, sends the solution back, and terminates the connection.

## Implementation

### L2 Layer (`l2sap.c`)

* **Initialization (`l2sap_create`):** Creates a UDP socket (`socket(AF_INET, SOCK_DGRAM, 0)`). It prepares the server's address structure (`struct sockaddr_in`) using the provided IP and port, converting them to the network format (`inet_pton`, `htons`). It does *not* explicitly bind the client socket to a local port, relying on the OS to assign one implicitly upon sending/receiving.
* **Sending (`l2sap_sendto`):**
    * Checks if the payload size (`len`) plus the L2 header size (`L2Headersize`, 8 bytes) exceeds the maximum frame size (`L2Framesize`, 1024 bytes). If so, it returns an error.
    * Constructs the L2 frame in a temporary buffer:
        * Copies the `L2Header` fields (converting `len` to network byte order using `htons`). `dst_addr` is already in network order from `l2sap_create`. `mbz` is set to 0.
        * Copies the payload data after the header.
        * Calculates the checksum using `compute_checksum` (byte-wise XOR over the frame, excluding the checksum field itself, which is temporarily zeroed).
        * Places the calculated checksum into the header.
    * Sends the complete frame buffer using `sendto`.
* **Receiving (`l2sap_recvfrom_timeout`):**
    * Uses `select()` to wait for data on the socket, respecting the optional `timeout` value. If `timeout` is `NULL`, it blocks indefinitely.
    * Handles `select()` errors (ignoring `EINTR`). Returns `L2_TIMEOUT` if the timeout expires.
    * If data is ready, reads the incoming UDP datagram using `recvfrom`.
    * **Validation:**
        * Discards frames smaller than `L2Headersize`.
        * Parses the `L2Header` (converting `len` from network to host byte order using `ntohs`).
        * Validates the length field in the header against the actual received bytes.
        * Verifies the checksum: recalculates the checksum on the received frame (with the checksum field temporarily zeroed) and compares it to the received checksum.
        * If any validation fails, the frame is discarded, and the function continues waiting for a valid frame.
    * If the frame is valid, it calculates the payload length and copies up to `len` bytes of the payload into the caller's `data` buffer. It returns the number of bytes copied.
* **Checksum (`compute_checksum`):** A static helper function performing a simple byte-wise XOR sum over the provided data buffer.
* **Blocking Receive (`l2sap_recvfrom`):** A convenience function that calls `l2sap_recvfrom_timeout` with a `NULL` timeout for indefinite blocking.

### L4 Layer (`l4sap.c`)

* **Initialization (`l4sap_create`):** Allocates the `L4SAP` state structure, creates the underlying `L2SAP` instance, and initializes the Stop-and-Wait state variables (`next_seqno_send = 0`, `expected_seqno_recv = 0`).
* **State (`L4SAP` struct):** This structure (defined in `l4sap.h`) holds the pointer to the `L2SAP` instance and the current sequence numbers needed for Stop-and-Wait.
* **Sending (`l4sap_send`):**
    * Truncates application `data` if its `len` exceeds `L4Payloadsize` (1012 bytes), issuing a warning.
    * Constructs an `L4_DATA` packet containing the `L4Header` (`type=L4_DATA`, `seqno=current next_seqno_send`) and the (potentially truncated) payload.
    * Enters a retransmission loop (max `L4_MAX_RETRIES = 5` attempts):
        * Sends the L4 packet using `l2sap_sendto`.
        * Waits for a reply using `l2sap_recvfrom_timeout` with a 1-second timeout.
        * **ACK Handling:** It specifically waits for an `L4_ACK` packet. Based on the code's logic (`recv_header->ackno == (l4->next_seqno_send + 1) % 2`), it expects the `ackno` field in the received ACK to contain the sequence number of the *next* data packet the peer expects (i.e., acknowledging the reception of `l4->next_seqno_send`).
        * If the correct ACK arrives, it toggles `l4->next_seqno_send` and returns the number of bytes sent (original `payload_len`).
        * If a timeout occurs, the loop continues, triggering a retransmission.
        * If `L4_RESET` is received, it returns `L4_QUIT`.
        * Incorrect ACKs or unexpected `L4_DATA` packets are ignored while waiting.
    * If all attempts fail due to timeouts, it returns `L4_SEND_FAILED`.
* **Receiving (`l4sap_recv`):**
    * Enters a loop, calling the blocking `l2sap_recvfrom` to wait for L2 frames.
    * Parses the L4 header from valid L2 payloads.
    * Handles incoming packet types:
        * `L4_RESET`: Returns `L4_QUIT`.
        * `L4_ACK`: Ignores unexpected ACKs.
        * `L4_DATA`:
            * Compares the packet's `seqno` with `l4->expected_seqno_recv`.
            * If it matches (expected packet): Copies the payload (up to `len` bytes) to the caller's `data` buffer, toggles `l4->expected_seqno_recv`, sends an `L4_ACK` back (with `ackno` set to the *new* `l4->expected_seqno_recv`, consistent with the ACK convention seen in `l4sap_send`), and returns the number of bytes copied.
            * If it doesn't match (duplicate packet): Discards the payload and resends the *previous* ACK (acknowledging the last correctly received packet again, using the *current* `l4->expected_seqno_recv` in the `ackno` field).
        * Unknown types: Ignores.
* **Termination (`l4sap_destroy`):** Sends multiple `L4_RESET` packets (best effort) to the peer via L2, destroys the underlying `L2SAP`, and frees the `L4SAP` structure.

### L5 Layer / Maze Solver (`maze.c`)

* **Functionality (`mazeSolve`):** Takes a `struct Maze` pointer, clears any previous solution marks (`mark` and `tmark` bits), checks for invalid input/bounds, and initiates the maze solving process.
* **Algorithm (`solve_recursive`):** Implements a **recursive Depth-First Search (DFS)** algorithm to find a path.
    * It uses a temporary mark bit (`tmark`) to keep track of visited cells within the current search path to avoid cycles.
    * It explores adjacent cells recursively based on the `walls` bits in the current cell (`maze->maze[index]`) indicating open directions (`up`, `down`, `left`, `right`).
    * **Path Marking:** If a recursive call successfully finds the end (`endX`, `endY`), it returns `true`. On the way back up the recursion stack, each function call that received `true` from its child call sets the permanent `mark` bit on its *own* cell (`maze->maze[index] |= mark`). This marks the path from end to start.
    * **Backtracking:** If exploring all open directions from a cell does not lead to the solution (all recursive calls return `false`), the function returns `false` without setting the `mark` bit. The `tmark` bit remains set for that cell in this implementation, effectively marking it as visited and preventing re-exploration via other paths in this specific DFS run.

## Assumptions and Choices

* **L2 Socket Binding:** The L2 client socket is not explicitly bound to a local address/port; it relies on the OS for implicit binding.
* **L4 ACK Convention:** The implementation assumes a specific Stop-and-Wait acknowledgment convention: an ACK packet with `ackno = N` acknowledges the receipt of the DATA packet with `seqno = N-1` (modulo 2) and indicates the peer is now expecting a DATA packet with `seqno = N`. This is consistently applied in both `l4sap_send` (when checking received ACKs) and `l4sap_recv` (when sending ACKs).
* **Maze Solving Algorithm:** A recursive Depth-First Search (DFS) is used, not Breadth-First Search (BFS). This finds *a* path, but not necessarily the shortest one (though for many simple mazes, it might).
* **Network Byte Order:** `htons`/`ntohs` are used for the 16-bit `len` field in the `L2Header`. It is assumed that the 32-bit `dst_addr` is already in network byte order (as returned by `inet_pton`). L4 header fields are single bytes.
* **Error Handling:** Basic error checking is present for system calls and invalid arguments. L2 checksum errors lead to silent discards. L4 timeouts lead to retransmissions up to a limit. Detailed network error recovery beyond Stop-and-Wait is not implemented.
* **Helper Functions:** Static helper functions (`compute_checksum`, `solve_recursive`) are used internally for organization.
* **Debugging Output:** `fprintf(stderr, ...)` statements are included throughout the code, useful for debugging.

## Build Instructions

The project uses CMake. To build the client executables:

```bash
# In the project's directory
mkdir build
cd build
cmake ..
make
```

In our case we used the following commands to run each of the tests (with varying loss probability). The first is to initialise the servers and the second is to run the client:

`maze-client`

```bash
./maze-server -v 8111
./maze-client 127.0.0.1 8111 40
```

`datalink-test-client`

```bash
./datalink-test-server -v 2 8111
./datalink-test-client 127.0.0.1 8111
```

`transport-test-client`

```bash
./transport-test-server -v 2 8111
./transport-test-client 127.0.0.1 8111
```




