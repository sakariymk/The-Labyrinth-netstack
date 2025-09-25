# The Labyrinth netstack

A network system implemented in C that builds up parts of the OSI model (L2, L4, L5) – and finishes with an application that solves a maze.  
This project was developed as part of a take-home exam at the University of Oslo, and demonstrates **network programming, protocol implementation, and algorithmic problem solving**.  

---

## 🌐 Overview  

The system is divided into three layers:  

- **L2 – Data Link**  
  Sends and receives frames over UDP with headers and checksums. Valid frames are passed up, invalid ones are discarded.  

- **L4 – Transport**  
  Implements a basic *stop-and-wait* protocol with sequence numbers and acknowledgements. No segmentation, no full-duplex – just enough to provide reliable delivery.  

- **L5 – Application (Maze Client)**  
  The client requests a maze from the server, finds a path from start to goal, returns the solution, and then terminates.  

---

## 🏗️ Architecture  

Communication flow:  

1. **L2** wraps data into frames with headers and checksum.  
2. **L4** sends packets using stop-and-wait, ensuring acknowledgements.  
3. **L5** (Maze Client) runs on top of L4 and handles maze logic.  

On the server side, test servers are provided (`datalink-test-server`, `transport-test-server`, `maze-server`) to validate the implementation.  

---

## 📦 Components  

- `l2sap.c / l2sap.h` – Data Link layer (frames, checksums)  
- `l4sap.c / l4sap.h` – Transport layer (stop-and-wait, seq/ACK handling)  
- `maze.c / maze.h` – Maze solver (pathfinding algorithm)  
- `maze-client.c` – Client connecting to the maze server  
- `maze-plot.c` – Simple visualization of mazes in the terminal  

---

## ⚡ Building  

The project uses **CMake**:  

```bash
git clone https://github.com/<username>/labyrinth.git
cd labyrinth
mkdir build && cd build
cmake ..
make
