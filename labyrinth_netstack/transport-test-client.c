#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>

#include "l4sap.h"

static int maxi( int a, int b )
{
    if( a > b ) return a;
    return b;
}

void usage( const char* name )
{
    fprintf( stderr, "Usage: %s <serverip> <port>\n"
                     "       serverip - IPv4 address of the server in dotted decimal notation\n"
                     "       port     - The server's port\n" , name );
    exit( -1 );
}


int main( int argc, char *argv[] )
{
    if( argc != 3 ) usage( argv[0] );

    L4SAP* l4 = l4sap_create( argv[1], atoi(argv[2]) );
    if( !l4 )
    {
        fprintf( stderr, "%s: Failed to create server\n", __FUNCTION__ );
        return -1;
    }

    for( int i=0; i<20; i++ )
    {
        fprintf( stderr, "\n%s: Round %d\n\n", __FUNCTION__, i );

        char buffer[1024];
        snprintf( buffer, 1024, "This is message %d from the client to the server.", i );

        int len = maxi( strlen(buffer)+1, 4*(2<<i) );

        fprintf( stderr, "%s: Client sends: '%s' and %d bytes\n", __FUNCTION__, buffer, len );

        int retval = l4sap_send( l4, (uint8_t*)buffer, len );
        if( retval == L4_SEND_FAILED )
        {
            fprintf( stderr, "%s: Send failed. Giving up.\n", __FUNCTION__ );
            l4sap_destroy( l4 );
            exit( -1 );
        }
        if( retval == L4_QUIT )
        {
            fprintf( stderr, "%s: Quit due to retrans failure.\n", __FUNCTION__ );
            l4sap_destroy( l4 );
            exit( -1 );
        }

        if( retval < 0 )
        {
            fprintf( stderr, "%s: Failed to send data\n", __FUNCTION__ );
            continue;
        }
        fprintf( stderr, "%s: l4sap_send returned with code %d\n", __FUNCTION__, retval );

        fprintf( stderr, "%s: waiting for data from server.\n", __FUNCTION__ );
        retval = l4sap_recv( l4, (uint8_t*)buffer, len );
        if( retval == L4_QUIT )
        {
            fprintf( stderr, "%s: Quit due to retrans failure.\n", __FUNCTION__ );
            l4sap_destroy( l4 );
            exit( -1 );
        }
        else if( retval < 0 )
        {
            fprintf( stderr, "%s: Failed to receive data (error)\n", __FUNCTION__ );
        }
        else if( retval == L4_TIMEOUT )
        {
            fprintf( stderr, "%s: Failed to receive data (timeout)\n", __FUNCTION__ );
        }
        else
        {
            fprintf( stderr, "%s: Received %d bytes\n", __FUNCTION__, retval );
            fprintf( stderr, "%s: Message is '%s'\n", __FUNCTION__, buffer );
        }
    }

    l4sap_send( l4, (uint8_t*)"QUIT", 5 );

    l4sap_destroy( l4 );
}

