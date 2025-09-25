#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "maze.h"

void mazePlot( const struct Maze* maze )
{
    int gridLen = maze->edgeLen * 2 + 1;

    char* grid = malloc( gridLen * gridLen );
    for( int y=0; y<gridLen; y++ )
        for( int x=0; x<gridLen; x++ )
            grid[y*gridLen+x] = 'X';

    for( int row=0; row<maze->edgeLen; row++ )
    {
        for( int col=0; col<maze->edgeLen; col++ )
        {
            grid[ (row*2+1) * gridLen + (col*2+1) ] = ' ';
            char val = maze->maze[ row*maze->edgeLen + col ];
            if( val & left  ) grid[ (row*2+1+0) * gridLen + (col*2+1-1) ] = ' ';
            if( val & right ) grid[ (row*2+1+0) * gridLen + (col*2+1+1) ] = ' ';
            if( val & up    ) grid[ (row*2+1-1) * gridLen + (col*2+1+0) ] = ' ';
            if( val & down  ) grid[ (row*2+1+1) * gridLen + (col*2+1+0) ] = ' ';

            if( val & mark  )
                grid[ (row*2+1) * gridLen + (col*2+1) ] = 'o';
        }
    }

    int col = maze->startX;
    int row = maze->startY;
    grid[ (row*2+1) * gridLen + (col*2+1) ] = 'A';
    col = maze->endX;
    row = maze->endY;
    grid[ (row*2+1) * gridLen + (col*2+1) ] = 'B';

    for( int y=0; y<gridLen; y++ )
    {
        for( int x=0; x<gridLen; x++ )
        {
            printf( "%c", grid[y*gridLen+x] );
        }
        printf( "\n" );
    }
    printf( "\n" );

    free( grid );
}

