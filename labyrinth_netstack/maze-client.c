#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>

#include "l4sap.h"
#include "maze.h"

#define MAZE_HEADER_LEN (6*sizeof(uint32_t))

static int maxi( int a, int b )
{
    if( a > b ) return a;
    return b;
}

void usage( const char* name )
{
    fprintf( stderr, "Usage: %s <serverip> <port> <maze-seed>\n"
                     "       serverip - IPv4 address of the server in dotted decimal notation\n"
                     "       port     - The server's port\n"
                     "       maze-seed - random number generator seed\n", name );
    exit( -1 );
}

int main( int argc, char *argv[] )
{
    if( argc != 4 ) usage( argv[0] );

    L4SAP* l4 = l4sap_create( argv[1], atoi(argv[2]) );
    if( !l4 )
    {
        fprintf( stderr, "%s: Failed to create server\n", __FUNCTION__ );
        return -1;
    }

    long maze_seed = strtol( argv[3], NULL, 10 );

    char buffer[1024];
    snprintf( buffer, 1024, "MAZE %ld", maze_seed );

    fprintf( stderr, "%s: Client sends: %s\n", __FUNCTION__, buffer );

    int retval = l4sap_send( l4, (uint8_t*)buffer, strlen(buffer)+1 );
    if( retval < 0 )
    {
        fprintf( stderr, "%s: Failed to send data\n", __FUNCTION__ );
    }

    retval = l4sap_recv( l4, (uint8_t*)buffer, 1024 );
    if( retval < 0 )
    {
        fprintf( stderr, "%s: Failed to receive data (error)\n", __FUNCTION__ );
    }
    else if( retval == 0 )
    {
        fprintf( stderr, "%s: Failed to receive data (timeout)\n", __FUNCTION__ );
    }
    else
    {
        fprintf( stderr, "%s: Received a message of length %d\n", __FUNCTION__, retval );

        if( retval < 8 )
        {
            fprintf( stderr, "%s: Message too small, cannot contain a Maze\n", __FUNCTION__ );
        }
        else
        {
            Maze* maze = (Maze*)malloc( sizeof(Maze) );
            if( maze == NULL )
            {
                fprintf( stderr, "%s: Could not allocate a Maze structure\n", __FUNCTION__ );
            }

            uint32_t* header = (uint32_t*)buffer;
            maze->edgeLen = ntohl( header[0] );
            maze->size    = ntohl( header[1] );
            if( retval != maze->size + MAZE_HEADER_LEN )
            {
                fprintf( stderr, "%s: Message size should be %d, but it is %d, not processing\n",
                         __FUNCTION__, (int)(maze->size + MAZE_HEADER_LEN), retval );
            }
            else
            {
                maze->startX = ntohl( header[2] );
                maze->startY = ntohl( header[3] );
                maze->endX   = ntohl( header[4] );
                maze->endY   = ntohl( header[5] );
                maze->maze   = (char*)malloc( maze->size );
                if( maze->maze == NULL )
                {
                    fprintf( stderr, "%s: Could not allocate a Maze data\n", __FUNCTION__ );
                }
                else
                {
                    memcpy( maze->maze, &buffer[MAZE_HEADER_LEN], maze->size );

                    mazePlot( maze );

                    mazeSolve( maze );

                    mazePlot( maze );


                    uint32_t* header = (uint32_t*)buffer;
                    header[0] = htonl( maze->edgeLen );
                    header[1] = htonl( maze->size );
                    header[2] = htonl( maze->startX );
                    header[3] = htonl( maze->startY );
                    header[4] = htonl( maze->endX );
                    header[5] = htonl( maze->endY );
                    memcpy( &buffer[MAZE_HEADER_LEN], maze->maze, maze->size );

                    l4sap_send( l4, (uint8_t*)buffer, maze->size + MAZE_HEADER_LEN );
                    free(maze->maze);
                }
            }
            free(maze);
        }
    }

    l4sap_send( l4, (uint8_t*)"QUIT", 5 );

    l4sap_destroy( l4 );
}
