/*
 * sockect.c
 *   Listener and talker socket
*/

#include <stdio.h>
#include <stdlib.h>
#include <stropts.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <linux/netdevice.h>

#include "status.h"
#include "socket.h"

#define BROADCAST_PORT    50042
#define DATA_PORT         50043

#define MSGPERIOD         30000 // 30ms
#define MSGBUFSIZE        300
#define MAXINPIPE         (3*MSGBUFSIZE/10)
#define TIMEPIPESIZE	  MAXINPIPE + 10

#define IPSTRSIZE         80

char myIP[IPSTRSIZE];
int cnc = -1;
int bRun = 1;

long ackCount = 0;
long cmdCount = 0;
long errCount = 0;
long repeatCount = 0;

int outSize= 0;
char outBuffer[MSGBUFSIZE];

int timePipeInIdx, timePipeOutIdx;
unsigned long timePipe[ TIMEPIPESIZE ];
unsigned long totalInPipe;

pthread_mutex_t mutexBuffer = PTHREAD_MUTEX_INITIALIZER;

unsigned long getDurationOfCommandsInPipe( )
{
  /*
  unsigned long t = 0;
  int i = timePipeOutIdx;
  while( i != timePipeInIdx )
  {
    t += timePipe[i++];
    if( i>=TIMEPIPESIZE ) i=0;
  }
  return t;
  */
  return totalInPipe;
}

tStatus sendCommand( char* cmd, unsigned long d )
{
  int timeout;
  int cl = strlen( cmd );
  int bl;

  if( cnc < 0 ) return retCncNotConnected;
  if( errCount ) return retCncError;
  if( cmd == NULL ) return retInvalidParam;
  if( cl > MSGBUFSIZE ) return retInvalidParam;

  // Don't push more than 5 seconds worth of commands in the pipe
  timeout = 60 * 1000000 / MSGPERIOD;
  while(( getDurationOfCommandsInPipe( ) > 5000 ) && timeout-- ) usleep( MSGPERIOD );

  // If the command can't fit in the current buffer
  if(( strlen( outBuffer ) + cl + 1 ) >= MSGBUFSIZE )
  {
    timeout = 60 * 1000000 / MSGPERIOD;
    // Wait for the buffer to be empty (1 min timeout)
    // printf( "outBuffer is full...\n" );
    while( outBuffer[0] != 0 && timeout-- > 0 ) usleep( MSGPERIOD );
    if( timeout <= 0 )
    {
      printf( "Timeout on TX\n" );
      return retCncCommError;
    }
  }

  pthread_mutex_lock(&mutexBuffer);

  timePipe[ timePipeInIdx++ ] = d;
  totalInPipe += d;
  if( timePipeInIdx >= TIMEPIPESIZE ) timePipeInIdx = 0;

  bl = strlen( outBuffer );
  // Add the new command to the buffer
  strcpy( outBuffer + bl, cmd );
  cmdCount++;
  outSize += cl;
  pthread_mutex_unlock(&mutexBuffer);
  
  //printf( "Send:%s", cmd );

  return retSuccess;
}


void* receiverThread( void* arg )
{
  int nbytes;
  char msgbuf[MSGBUFSIZE];
  unsigned long tmp;

  while( bRun )
  {
    if((nbytes = read( cnc, msgbuf, sizeof( msgbuf ))) < 0 ) {
      perror("read");
      return NULL;
    }
    tmp = 0;
    while( nbytes )
    {
      switch( msgbuf[--nbytes] )
      {
      case 'O' : ackCount++;
         tmp += timePipe[ timePipeOutIdx++ ];  
         if( timePipeOutIdx >= TIMEPIPESIZE ) timePipeOutIdx = 0;
        break;
      case 'E' : errCount++; break;
      case 'T' : errCount++; break;
      }
    }
    pthread_mutex_lock(&mutexBuffer);
    totalInPipe -= tmp;    
    pthread_mutex_unlock(&mutexBuffer);
  }
  return NULL;
}


void* senderThread( void* arg )
{
  struct sockaddr_in addr;
  int fd, nbytes;
  unsigned int addrlen;
  char msgbuf[MSGBUFSIZE];
  char srcIP[IPSTRSIZE];
  pthread_t thread;
  unsigned long idleCount = 0;
  unsigned long inPipe = 0;


  if ((fd=socket(AF_INET,SOCK_DGRAM,0)) < 0) {
    perror("socket");
    return NULL;
  }

  /* set up destination address */
  memset(&addr,0,sizeof(addr));
  addr.sin_family=AF_INET;
  addr.sin_addr.s_addr=htonl(INADDR_ANY); /* N.B.: differs from sender */
  addr.sin_port=htons(BROADCAST_PORT);
     
  /* bind to receive address */
  if (bind(fd,(struct sockaddr *) &addr,sizeof(addr)) < 0) {
    perror("bind");
    return NULL;
  }
     
  addrlen=sizeof(addr);
  if ((nbytes=recvfrom(fd,msgbuf,MSGBUFSIZE,0,(struct sockaddr *) &addr,&addrlen)) < 0 ) {
    perror("recvfrom");
    return NULL;
  }

  // Zero terminate the string
  msgbuf[nbytes]=0;

  inet_ntop(AF_INET, &addr.sin_addr, srcIP, sizeof(srcIP));

  printf( "\nFound: %s at IP:%s.\n", msgbuf, srcIP );

  if ((cnc=socket(AF_INET,SOCK_STREAM,0)) < 0) {
    perror("socket");
    return NULL;
  }

  /* Just change the port and connect. */
  addr.sin_port=htons(DATA_PORT);

  if( connect( cnc, (struct sockaddr*)&addr, sizeof(addr)) < 0 )
  {
    perror("connect");
    return NULL;
  }

  strcpy( msgbuf, "RST" );

  if( write( cnc, msgbuf, strlen( msgbuf )) < 0 ) {
    perror("write");
    return NULL;
  }

  if((nbytes = read( cnc, msgbuf, sizeof( msgbuf ))) < 0 ) {
    perror("read");
    return NULL;
  }

  msgbuf[nbytes]=0;
  if( strcmp( msgbuf, "HLO" ) != 0 ) { 
    printf( "Connection ack error. Got %s.\n", msgbuf );
    close( cnc );
    cnc = -1;
    return NULL;
  }

  outSize = 0;
  memset( outBuffer, 0, sizeof( outBuffer ));

  printf( "Connected.\n" );

  pthread_create( &thread, NULL, receiverThread, NULL );

  while( bRun ) {
    if( pthread_mutex_lock(&mutexBuffer) < 0 ) {
      perror("pthread_mutex_lock");
    }

    if( outBuffer[0] != 0 )
    {
      if( cmdCount - MAXINPIPE < ackCount )
      {
        if( outSize != strlen( outBuffer ))
        {
          printf( "ERROR : buffer size mismatch. Expected %d, got %d.\n", outSize, strlen( outBuffer ));
          bRun = 0;
        }
        else
        {          
          if( write( cnc, outBuffer, outSize ) != outSize ) 
          {
            printf("Write failed.\n");
            bRun = 0;
          }
       
          outSize = 0;
          outBuffer[0] = 0;
        }
      }
      idleCount = 0;
    }
    else
    {
      // Each time we start to be idle, check the duration of the commands
      // currently pending acknowledge and set the timeout when it's been
      // too long for receiving an acknowledge. Give a 5sec grace period.
      if( idleCount == 0 )
      {
        inPipe = (( getDurationOfCommandsInPipe( ) + 5000 ) * 1000 ) / MSGPERIOD;
        // printf( "%.2f sec of commands. Will wait for %ld loops.\n", getDurationOfCommandsInPipe( ) / 1000.0, inPipe );
      }

      // If we've been idle for one second longer than the amount of
      // commands still pending to be acknowledged, check that the count
      // of sent matches the # of ACKs
      if( idleCount++ > inPipe )
      {
        if( cmdCount != ackCount )
        {
          printf( "Commands send:%ld. ACKs:%ld. ETA:%.1f sec (%ld).\n",
            cmdCount,
            ackCount,
            getDurationOfCommandsInPipe( ) / 1000.0,
            inPipe );
        }        
      }
    }
    if( pthread_mutex_unlock(&mutexBuffer) < 0 ) {
      perror("pthread_mutex_unlock");
    }
    usleep( MSGPERIOD );
  }

  printf( "Sender Thread Stopped\n" );
  return NULL;
}

int initSocketCom(void)
{
  pthread_t thread;

  timePipeInIdx = 0;
  timePipeOutIdx = 0;
  totalInPipe = 0;
  memset( timePipe, 0, sizeof( timePipe ));

  outSize = 0;
  memset( outBuffer, 0, sizeof( outBuffer ));

  pthread_mutex_init( &mutexBuffer, NULL );

  pthread_create( &thread, NULL, senderThread, NULL );
  return 0;
}

