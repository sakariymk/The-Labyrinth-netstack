#ifndef MAZE_H
#define MAZE_H

#include <inttypes.h>

#define left   ( 0x1 << 1 )
#define right  ( 0x1 << 2 )
#define up     ( 0x1 << 3 )
#define down   ( 0x1 << 4 )

#define tmark  ( 0x1 << 5 )
#define mark   ( 0x1 << 6 )

typedef struct Maze Maze;

struct Maze
{
    /* number of squares in horizontal or vertical direction */
    uint32_t edgeLen;

    /* total number of squares */
    uint32_t size;

    /* Start your path search at this coordinate */
    uint32_t startX;
    uint32_t startY;

    /* End your path search at this coordinate */
    uint32_t endX;
    uint32_t endY;

    /* Grid space that should be allocated dynamically on the
     * head, one byte per square.
     * Squares are arranged in column-major order, meaning that
     * the square (x,y) is accessed as _grid[y*_edgeLen+x].
     * There is no special character that terminates a line or
     * the grid.
     * Each square contains the directions in which the maze's
     * neighbouring cells can be reached directly. For example,
     * if _grid[2*_edgeLen+11] contains the value left|right,
     * there is not wall between grid[2*_edgeLen+10],
     * grid[2*_edgeLen+11] and grid[2*_edgeLen+12], but there
     * are walls between grid[1*_edgeLen+10], grid[2*_edgeLen+11]
     * and grid[3*_edgeLen+11].
     */
    char* maze;
};

/* Take a maze data structure and plot it to the screen.
 */
void mazePlot( const struct Maze* maze );

/* This function takes a maze data structure. It will search
 * for a path through the maze from (startX,startY) to (endX,endY)
 * and mark the path by adding the bit "mark" on the direct
 * path from start to end. That means that cells that are in dead
 * ends must not be marked with this bit.
 * It is very likely that you must introduce additional bits to
 * find a correct path from start to end. The bit called "tmark"
 * is such a bit that you could use.
 * The strategy for finding the path is yours. A typical approach
 * would be DFS and recursion, but the choice is yours.
 */
void mazeSolve( struct Maze* maze );

#endif

