#include "app_camera.h"
// #include "app_httpserver.h"
#include "app_wifi.h"
#include "app_speech_srcif.h"

#define VERSION "0.9.0"

#define IIS_SCLK    14//26  
#define IIS_LCLK    32  //ws
#define IIS_DSIN    -1
#define IIS_DOUT    33

typedef enum
{
    WAIT_FOR_WAKEUP,
    WAIT_FOR_CONNECT,
    START_DETECT,
    START_RECOGNITION,
    START_ENROLL,
    START_DELETE,

} en_fsm_state;

extern en_fsm_state g_state;
extern int g_is_enrolling;
extern int g_is_deleting;

