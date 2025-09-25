#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>

#include "l2sap.h"

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

    struct L2SAP* l2 = l2sap_create( argv[1], atoi(argv[2]) );
    if( !l2 ) {
        fprintf( stderr, "Failed to create server\n" );
        return -1;
    }

    for( int i=0; i<25; i++ )
    {
        fprintf( stderr, "\n%s: Round %d\n\n", __FUNCTION__, i );

        char buffer[4096];
        snprintf( buffer, 1024, "message %d from client to server.", i );

        int len = maxi( strlen(buffer)+1, 4*(2<<i) );
        if( len > 4096 ) len = strlen(buffer) + 1;

        fprintf( stderr, "%s: Client sends: '%s' and %d bytes\n", __FUNCTION__, buffer, len );

        int error = l2sap_sendto( l2, (uint8_t*)buffer, len );
        if( error < 0 ) {
            fprintf( stderr, "Failed to send data\n" );
            continue;
        }

        struct timeval tv;
        tv.tv_usec = 0;
        tv.tv_sec = 1;

        len = l2sap_recvfrom_timeout( l2, (uint8_t*)buffer, 1024, &tv );
        if( len < 0 )
        {
            fprintf( stderr, "Receiving data failed.\n" );
        }
        else if( len == 0 )
        {
            fprintf( stderr, "Server did not respond in 1 second.\n" );
        }
        else
        {
            printf("Server responded: %s\n", buffer );
        }
    }

    l2sap_destroy( l2 );
}

