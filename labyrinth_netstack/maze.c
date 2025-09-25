#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>

#include "maze.h"

// Funksjon deklarasjon
static bool solve_recursive(struct Maze* maze, int x, int y);

void mazeSolve(struct Maze* maze)
{
    if (!maze || !maze->maze) { // Maze eller maze pekeren er null
        fprintf(stderr, "mazeSolve: Invalid maze pointer provided.\n");
        return;
    }
    if (maze->size == 0 || maze->edgeLen == 0) { // Stoerelse paa maze er 0 (ingenting aa loese)
        fprintf(stderr, "mazeSolve: Maze has zero size or edgeLen.\n");
        return;
    }
    if (maze->startX >= maze->edgeLen || maze->startY >= maze->edgeLen ||
        maze->endX >= maze->edgeLen || maze->endY >= maze->edgeLen) { // Om A eller B (start eller slutt) er utenfor koordinatene til mazen
        fprintf(stderr, "mazeSolve: Start or End coordinates are out of bounds.\n");
        return;
    }

    fprintf(stderr, "mazeSolve: Clearing previous marks...\n");
    for (uint32_t i = 0; i < maze->size; ++i) { // For stoerelse paa maze
        maze->maze[i] &= ~(mark | tmark); // XOR operasjon paa mark og tmark for aa bytte markeringer
    }

    fprintf(stderr, "mazeSolve: Starting DFS from (%u, %u) to (%u, %u)...\n",
            maze->startX, maze->startY, maze->endX, maze->endY);

    bool path_found = solve_recursive(maze, maze->startX, maze->startY); // Kall til rekursiv funksjon

    if (path_found) {
        fprintf(stderr, "mazeSolve: Path found and marked.\n");
    } else {
        fprintf(stderr, "mazeSolve: No path found.\n");
    }
}

static bool solve_recursive(struct Maze* maze, int x, int y)
{
    if (x < 0 || x >= maze->edgeLen || y < 0 || y >= maze->edgeLen) { // Om start koordinater er out of bounds saa finnes det ingen vei
        return false;
    }

    int index = y * maze->edgeLen + x; // Beregn indeks til cellen

    if (maze->maze[index] & tmark) { // Hvis cellen allerede er markert
        return false;
    }

    maze->maze[index] |= tmark; // Merker cellen som besoekt

    if (x == maze->endX && y == maze->endY) { // Om vi har kommet til slutt koordinatene
        maze->maze[index] |= mark;  // Marker stien fra A til B
        return true;
    }

    char walls = maze->maze[index]; // Setter vegger

    // Finn sti utifra rettningene opp, ned, venstre, hoeyre
    bool path_found = false;
    if ((walls & up) && solve_recursive(maze, x, y - 1)) { // Sjekker om det er mulig aa gaa opp og rekursivt proever aa finne en vei
        maze->maze[index] |= mark; // Hvis solve gir true (sti funnet) marker cellen som del av stien
        path_found = true;
    }
    if ((walls & down) && solve_recursive(maze, x, y + 1)) { // Sjekker om det er mulig aa gaa ned og rekursivt proever aa finne en vei
        maze->maze[index] |= mark; // Hvis solve gir true (sti funnet) marker cellen som del av stien
        path_found = true;
    }
    if ((walls & left) && solve_recursive(maze, x - 1, y)) { // Sjekker om det er mulig aa gaa til venstre og rekursivt proever aa finne en vei
        maze->maze[index] |= mark; // Hvis solve gir true (sti funnet) marker cellen som del av stien
        path_found = true;
    }
    if ((walls & right) && solve_recursive(maze, x + 1, y)) { // Sjekker om det er mulig aa gaa til hoeyre og rekursivt proever aa finne en vei
        maze->maze[index] |= mark; // Hvis solve gir true (sti funnet) marker cellen som del av stien
        path_found = true;
    }

    // Gaa tilbake om ingen sti var funnet og fjern markering av cellen
    if (!path_found) {
        maze->maze[index] &= ~tmark;  // Fjern markering av cellen
    }

    return path_found;
}
