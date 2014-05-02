
#define VK_UP       0x00415b1b
#define VK_DOWN     0x00425b1b
#define VK_RIGHT    0x00435b1b
#define VK_LEFT     0x00445b1b
#define VK_PAGEUP   0x7e355b1b
#define VK_PAGEDN   0x7e365b1b

#define VK_ESC	    0x1b

int getkey( char* pt, int size );
int startKeyInput( void(*callback)(int));
