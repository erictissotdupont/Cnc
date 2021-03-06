
typedef struct _tAxis {
  long step;
  double scale;
  double cutComp;
} tAxis;

typedef struct _tSpindle {
  int currentState;
  int nextState;
} tSpindle;

void setExportFile( FILE* file );
void initAxis( int a, double scale );
void initSpindle( );

void getCurPos( tPoint* P );
void getRawStepPos( int* x, int* y, int* z );

void resetMotorPosition( );
void resetCompensation( );
void getCompensation( double* x, double* y, double* z );
void addCompensation( double x, double y, double z );



double getLargestStep( );
double getSmalestStep( );
double getMaxDistanceError( );

tStatus doMove( void(*posAtStep)(tPoint*,int,int,void*), int stepCount, double duration, void* pArg );

int setSpindleState( int state );
