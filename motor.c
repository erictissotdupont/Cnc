#include <unistd.h>
#include <sys/stat.h> 
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <sys/time.h>
#include <pthread.h>
#include <string.h>

#include "status.h"
#include "geometry.h"
#include "motor.h"
#include "keyboard.h"
#include "socket.h"

FILE *exportFile = NULL;
tAxis XMotor,YMotor,ZMotor;
tAxis* pMotor[] = {&XMotor,&YMotor,&ZMotor};
tSpindle Spindle;

void setExportFile( FILE* file )
{
  exportFile = file;
}

void resetCompensation( )
{
  XMotor.cutComp = 0;
  YMotor.cutComp = 0;
  ZMotor.cutComp = 0;
}

void getCompensation( double* x, double* y, double* z )
{
  if( x ) *x = XMotor.cutComp;
  if( y ) *y = YMotor.cutComp;
  if( z ) *z = ZMotor.cutComp;
}

void getCurPos( tPoint* P )
{
  P->x = XMotor.step * XMotor.scale;
  P->y = YMotor.step * YMotor.scale;
  P->z = ZMotor.step * ZMotor.scale;
}

void getRawStepPos( int* x, int* y, int* z )
{
  if( x ) *x = XMotor.step;
  if( y ) *y = YMotor.step;
  if( z ) *z = ZMotor.step;
}

void addCompensation( double x, double y, double z )
{
  XMotor.cutComp += x;
  YMotor.cutComp += y;
  ZMotor.cutComp += z;
}

void initAxis( int a, double scale )
{
  tAxis* pA = pMotor[a];
  pA->step = 0;
  pA->scale = scale;
  pA->cutComp = 0.0;
}

void initSpindle( )
{
  Spindle.currentState = 0;
  Spindle.nextState = 0;
}

void resetMotorPosition( )
{
  XMotor.step = 0;
  YMotor.step = 0;
  ZMotor.step = 0;
}

double getLargestStep( )
{
  return maxOf3( XMotor.scale, YMotor.scale, ZMotor.scale );
}

double getSmalestStep( )
{
  return minOf3( XMotor.scale, YMotor.scale, ZMotor.scale );
}

double getMaxDistanceError( )
{
  tPoint oneStep;
  oneStep.x = XMotor.scale;
  oneStep.y = YMotor.scale;
  oneStep.z = ZMotor.scale;
  return vector3DLength(oneStep);
}


long calculateMove( tAxis* A, double target )
{
  double delta = target - A->step * A->scale;
  long step = delta / A->scale;
  A->step += step;
  return step;
}

int setSpindleState( int state )
{
  if( Spindle.nextState != state )
  {
    Spindle.nextState = state;
    return 1;
  }
  return 0;
}

long getSpindleState( )
{
  if( Spindle.currentState == Spindle.nextState ) return -1;
  Spindle.currentState = Spindle.nextState;
  return( Spindle.nextState == 3 );
}

tStatus doMove( void(*posAtStep)(tPoint*,int,int,void*), int stepCount, double duration, void* pArg )
{
  int i;
  long x,y,z,d,s;
  tPoint Ideal;
  char str[ 100 ];
  char tmp[ 30 ];
  tStatus status = retNoOutputFound;
 
  if( stepCount == 0 ) return retUnknownErr;

  duration = duration / stepCount;
  d = duration * 1000;
 
  for( i=1; i<=stepCount; i++ )
  {
    // Get the position we should be at for step i of stepCount
    posAtStep( &Ideal, i, stepCount, pArg );

    x = calculateMove( &XMotor, Ideal.x );
    y = calculateMove( &YMotor, Ideal.y );
    z = calculateMove( &ZMotor, Ideal.z );
    s = getSpindleState( );

    strcpy( str, "@" );
    if( x ) { sprintf( tmp, "X%ld", x ); strcat( str, tmp ); }
    if( y ) { sprintf( tmp, "Y%ld", y ); strcat( str, tmp ); }
    if( z ) { sprintf( tmp, "Z%ld", z ); strcat( str, tmp ); }
    if( d ) { sprintf( tmp, "D%ld", d ); strcat( str, tmp ); }
    if( s >= 0 ) { sprintf( tmp, "S%ld", s ); strcat( str, tmp ); }

    strcat( str, "\n" );

    status = sendCommand( str, duration );

    if( exportFile )
    {
      if( fwrite( str, 1, strlen( str ), exportFile ) > 0 && status == retCncNotConnected )
      {
        status = retSuccess;
      }
    }
  }

  return status;
}
