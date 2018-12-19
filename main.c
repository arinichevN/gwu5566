#include "main.h"

int app_state = APP_INIT;
int sock_port = -1;
int sock_fd = -1;

Peer peer_client = {.fd = &sock_fd, .addr_size = sizeof peer_client.addr};

DeviceList device_list = LIST_INITIALIZER;
LCorrectionList lcorrection_list = LIST_INITIALIZER;
ThreadList thread_list = LIST_INITIALIZER;

#include "device.c"
#include "util.c"
#include "init_f.c"

void serverRun ( int *state, int init_state ) {
    SERVER_HEADER
    SERVER_APP_ACTIONS
    DEF_SERVER_I1LIST
    if ( ACP_CMD_IS ( ACP_CMD_GET_FTS ) ) {
        SERVER_PARSE_I1LIST
        FORLISTN ( i1l, i ) {
            Device *item;
            LIST_GETBYID ( item, &device_list, i1l.item[i] );
            if ( item == NULL ) continue;
            if ( !catFTS ( item, &response ) ) return;
        }
    }
    acp_responseSend ( &response, &peer_client );
}

void cleanup_handler ( void *arg ) {
    Thread *item = arg;
    printdo ( "cleaning up thread %d\n", item->id );
}

void *threadFunction ( void *arg ) {
    Thread *item = arg;
    printdo ( "thread for program with id=%d has been started\n", item->id );
#ifdef MODE_DEBUG
    pthread_cleanup_push ( cleanup_handler, item );
#endif

#define DPL item->device_plist
#define DPLL DPL.length
#define DPLIi DPL.item[i]

#ifndef CPU_ANY
    for ( size_t i = 0; i < DPLL; i++ ) {
        if ( !DPLIi->deviceSetup ( DPLIi ) ) {
            printde ( "setup failed for device with id=%d\n", DPLIi->id );
            pthread_exit(NULL);
        }
    }
#endif
    while ( 1 ) {
        struct timespec t1 = getCurrentTime();
        int old_state;
        if ( threadCancelDisable ( &old_state ) ) {
            for ( int i = 0; i < DPLL; i++ ) {
                if ( ton_ts ( DPLIi->tconv, &DPLIi->tmrconv ) ) {
                    deviceRead ( DPLIi );
                }
            }
            threadSetCancelState ( old_state );
        }
        sleepRest ( item->cycle_duration, t1 );
    }
#ifdef MODE_DEBUG
    pthread_cleanup_pop ( 1 );
#endif

#undef DPL
#undef DPLL
#undef DPLIi
}

int initApp() {
    if ( !readSettings ( &sock_port, CONF_MAIN_FILE ) ) {
        putsde ( "failed to read settings\n" );
        return 0;
    }
    if ( !initServer ( &sock_fd, sock_port ) ) {
        putsde ( "failed to initialize server\n" );
        return 0;
    }
    if ( !gpioSetup() ) {
        freeSocketFd ( &sock_fd );
        putsde ( "failed to initialize GPIO\n" );
        return 0;
    }
    return 1;
}

int initData() {
    initLCorrection ( &lcorrection_list, CONF_LCORRECTION_FILE );
    if ( !initDevice ( &device_list, &lcorrection_list, CONF_DEVICE_FILE ) ) {
        freeDeviceList ( &device_list );
        FREE_LIST ( &lcorrection_list );
        return 0;
    }
    if ( !initDeviceFilter ( &device_list, CONF_FILTER_MA_FILE, CONF_FILTER_EXP_FILE, CONF_CHANNEL_FILTER_FILE ) ) {
        freeDeviceList ( &device_list );
        FREE_LIST ( &lcorrection_list );
        return 0;
    }
    if ( !checkDevice ( &device_list ) ) {
        freeDeviceList ( &device_list );
        FREE_LIST ( &lcorrection_list );
        return 0;
    }
    if ( !initThread ( &thread_list, &device_list, CONF_THREAD_FILE, CONF_THREAD_DEVICE_FILE ) ) {
        freeThreadList ( &thread_list );
        freeDeviceList ( &device_list );
        FREE_LIST ( &lcorrection_list );
        return 0;
    }
    return 1;
}

void freeData() {
    stopAllThreads ( &thread_list );
    freeThreadList ( &thread_list );
    freeDeviceList ( &device_list );
    FREE_LIST ( &lcorrection_list );
}

void freeApp() {
    freeData();
    freeSocketFd ( &sock_fd );
    gpioFree();
}

void exit_nicely ( ) {
    freeApp();
    putsdo ( "\nexiting now...\n" );
    exit ( EXIT_SUCCESS );
}

int main ( int argc, char** argv ) {
    if ( geteuid() != 0 ) {
        putsde ( "root user expected\n" );
        return ( EXIT_FAILURE );
    }
#ifndef MODE_DEBUG
    daemon ( 0, 0 );
#endif
    conSig ( &exit_nicely );
    if ( mlockall ( MCL_CURRENT | MCL_FUTURE ) == -1 ) {
        perrorl ( "mlockall()" );
    }
#ifndef MODE_DEBUG
    setPriorityMax ( SCHED_FIFO );
#endif
    int data_initialized = 0;
    while ( 1 ) {
#ifdef MODE_DEBUG
        printf ( "%s(): %s %d\n", F, getAppState ( app_state ), data_initialized );
#endif
        switch ( app_state ) {
        case APP_INIT:
            if ( !initApp() ) {
                return ( EXIT_FAILURE );
            }
            app_state = APP_INIT_DATA;
            break;
        case APP_INIT_DATA:
            data_initialized = initData();
            app_state = APP_RUN;
            break;
        case APP_RUN:
            serverRun ( &app_state, data_initialized );
            break;
        case APP_STOP:
            freeData();
            data_initialized = 0;
            app_state = APP_RUN;
            break;
        case APP_RESET:
            freeApp();
            data_initialized = 0;
            app_state = APP_INIT;
            break;
        case APP_EXIT:
            exit_nicely();
            break;
        default:
            freeApp();
            putsde ( "unknown application state\n" );
            return ( EXIT_FAILURE );
        }
    }
    freeApp();
    return ( EXIT_SUCCESS );
}

