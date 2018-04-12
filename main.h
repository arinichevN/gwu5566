
#ifndef GWU5566_H
#define GWU5566_H

#include "lib/app.h"
#include "lib/timef.h"
#include "lib/acp/main.h"
#include "lib/acp/app.h"
#include "lib/udp.h"
#include "lib/tsv.h"
#include "lib/gpio.h"
#include "lib/lcorrection.h"
#include "lib/spi.h"
#include "lib/filter/ma.h"
#include "lib/filter/exp.h"

#define APP_NAME gwu5566
#define APP_NAME_STR TOSTRING(APP_NAME)

#ifdef MODE_FULL
#define CONF_DIR "/etc/controller/" APP_NAME_STR "/"
#endif
#ifndef MODE_FULL
#define CONF_DIR "./config/"
#endif
#define CONF_MAIN_FILE CONF_DIR "main.tsv"
#define CONF_DEVICE_FILE CONF_DIR "device.tsv"
#define CONF_THREAD_FILE CONF_DIR "thread.tsv"
#define CONF_THREAD_DEVICE_FILE CONF_DIR "thread_device.tsv"
#define CONF_LCORRECTION_FILE CONF_DIR "lcorrection.tsv"
#define CONF_FILTER_MA_FILE CONF_DIR "filter_ma.tsv"
#define CONF_FILTER_EXP_FILE CONF_DIR "filter_exp.tsv"
#define CONF_CHANNEL_FILTER_FILE CONF_DIR "channel_filter.tsv"

#define TYPE_MAX6675_STR "max6675"
#define TYPE_MAX31855_STR "max31855"
#define MODE_SYS_STR "sys"
#define MODE_GPIO_STR "gpio"

#define SPI_SPEED_MAX6675  4000000
#define SPI_SPEED_MAX31855 4500000

#define CONVERSION_TIME_MAX6675  {0,230000000}
#define CONVERSION_TIME_MAX31855 {0,110000000}

enum {
    UNKNOWN = 1,
    TYPE_MAX6675,
    TYPE_MAX31855,
    MODE_SYS,
    MODE_GPIO
} StateAPP;

typedef struct{
    void *filter_ptr;
    void (*filter_fun)(float *, void *);
} Filter; 

DEC_LIST(Filter)

struct device_st {
    int id;
    int mode;
    int type;
    int sclk;
    int miso;
    int cs;
    SPI spi;
    struct timespec tconv;
    Ton_ts tmrconv;
    int (*deviceRead) (float *, struct device_st*);
    int (*deviceSetup) (struct device_st*);
    FTS result;
    LCorrection *lcorrection;
    FilterMAList fma_list;
    FilterEXPList fexp_list;
    FilterList f_list;
    Mutex mutex;
};
typedef struct device_st Device;

DEC_LIST(Device)
DEC_PLIST(Device)

struct thread_st {
    int id;
    DevicePList device_plist;
    pthread_t thread;
    struct timespec cycle_duration;
};
typedef struct thread_st Thread;
DEC_LIST(Thread)

extern int readSettings();

extern void serverRun(int *state, int init_state);

extern void *threadFunction(void *arg);

extern void initApp();

extern int initData();

extern void freeData();

extern void freeApp();

extern void exit_nicely();

extern void exit_nicely_e(char *s);

#endif

