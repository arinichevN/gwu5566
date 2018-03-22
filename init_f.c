#include "main.h"

int readSettings(int *sock_port, const char *data_path) {
    TSVresult tsv = TSVRESULT_INITIALIZER;
    TSVresult* r = &tsv;
    if (!TSVinit(r, data_path)) {
        TSVclear(r);
        return 0;
    }
    char *str = TSVgetvalues(r, 0, "port");
    if (str == NULL) {
        return 0;
    }
    *sock_port = atoi(str);
    TSVclear(r);
    return 1;
}

static int getModeByStr(char *s) {
    if (strcmp(s, MODE_SYS_STR) == 0) {
        return MODE_SYS;
    } else if (strcmp(s, MODE_GPIO_STR) == 0) {
        return MODE_GPIO;
    }
    return UNKNOWN;
}

static int getTypeByStr(char *s) {
    if (strcmp(s, TYPE_MAX6675_STR) == 0) {
        return TYPE_MAX6675;
    } else if (strcmp(s, TYPE_MAX31855_STR) == 0) {
        return TYPE_MAX31855;
    }
    return UNKNOWN;
}

static int (*getReadFunctionForDevice(int type, int mode)) (float *, struct device_st *) {
    if (type == TYPE_MAX6675) {
        if (mode == MODE_SYS) {
            return max6675sys_read;
        } else if (mode == MODE_GPIO) {
            return max6675gpio_read;
        } else {
            return NULL;
        }
    } else if (type == TYPE_MAX31855) {
        if (mode == MODE_SYS) {
            return max31855sys_read;
        } else if (mode == MODE_GPIO) {
            return max31855gpio_read;
        } else {
            return NULL;
        }
    } else {
        return NULL;
    }
}

static int (*getSetupFunctionForDevice(int type, int mode)) (struct device_st *) {
    if (type == TYPE_MAX6675) {
        if (mode == MODE_SYS) {
            return max6675sys_setup;
        } else if (mode == MODE_GPIO) {
            return max6675gpio_setup;
        } else {
            return NULL;
        }
    } else if (type == TYPE_MAX31855) {
        if (mode == MODE_SYS) {
            return max31855sys_setup;
        } else if (mode == MODE_GPIO) {
            return max31855gpio_setup;
        } else {
            return NULL;
        }
    } else {
        return NULL;
    }
}

static int getSPISpeed(int type) {
    if (type == TYPE_MAX6675) {
        return SPI_SPEED_MAX6675;
    } else if (type == TYPE_MAX31855) {
        return SPI_SPEED_MAX31855;
    } else {
        return 0;
    }
}

static struct timespec getConversionTime(int type) {
    if (type == TYPE_MAX6675) {
        struct timespec tm = CONVERSION_TIME_MAX6675;
        return tm;
    } else if (type == TYPE_MAX31855) {
        struct timespec tm = CONVERSION_TIME_MAX31855;
        return tm;
    }
    struct timespec tm = {0, 0};
    return tm;
}

int initDevice(DeviceList *list, LCorrectionList *lcl, const char *data_path) {
    TSVresult tsv = TSVRESULT_INITIALIZER;
    TSVresult* r = &tsv;
    if (!TSVinit(r, data_path)) {
        TSVclear(r);
        return 0;
    }
    int n = TSVntuples(r);
    if (n <= 0) {
        TSVclear(r);
        return 1;
    }
    RESIZE_M_LIST(list, n);
    NULL_LIST(list);
    if (LML != n) {
#ifdef MODE_DEBUG
        fprintf(stderr, "%s(): failure while resizing list\n", F);
#endif
        TSVclear(r);
        return 0;
    }
    for (int i = 0; i < LML; i++) {
        LIi.result.id = LIi.id = TSVgetis(r, i, "id");
        LIi.type = getTypeByStr(TSVgetvalues(r, i, "type"));
        LIi.mode = getModeByStr(TSVgetvalues(r, i, "mode"));
        LIi.deviceRead = getReadFunctionForDevice(LIi.type, LIi.mode);
        LIi.deviceSetup = getSetupFunctionForDevice(LIi.type, LIi.mode);
        LIi.spi.speed = getSPISpeed(LIi.type);
        LIi.tconv = getConversionTime(LIi.type);
        ton_ts_touch(&LIi.tmrconv);
        LIi.sclk = TSVgetis(r, i, "sclk");
        LIi.mosi = TSVgetis(r, i, "mosi");
        LIi.miso = TSVgetis(r, i, "miso");
        LIi.cs = TSVgetis(r, i, "cs");
        strcpyma(&LIi.spi.path, TSVgetvalues(r, i, "spi_path"));
        int lcorrection_id = TSVgetis(r, i, "lcorrection_id");
        LIi.lcorrection = getLCorrectionById(lcorrection_id, lcl);
        if (TSVnullreturned(r)) {
            break;
        }
        if (!initMutex(&LIi.mutex)) {
            break;
        }
        LL++;
    }
    TSVclear(r);
    if (LL != LML) {
#ifdef MODE_DEBUG
        fprintf(stderr, "%s(): failure while reading rows\n", F);
#endif
        return 0;
    }
    return 1;
}

static int checkThreadDevice(TSVresult* r) {
    int n = TSVntuples(r);
    int valid = 1;
    //unique thread_id and device_id
    for (int k = 0; k < n; k++) {
        int thread_id_k = TSVgetis(r, k, "thread_id");
        int device_id_k = TSVgetis(r, k, "device_id");
        if (TSVnullreturned(r)) {
            fprintf(stderr, "%s(): check thread_device configuration file: bad format\n", F);
            return 0;
        }
        for (int g = k + 1; g < n; g++) {
            int thread_id_g = TSVgetis(r, g, "thread_id");
            int device_id_g = TSVgetis(r, g, "device_id");
            if (TSVnullreturned(r)) {
                fprintf(stderr, "%s(): check thread_device configuration file: bad format\n", F);
                return 0;
            }
            if (thread_id_k == thread_id_g && device_id_k == device_id_g) {
                fprintf(stderr, "%s(): check thread_device configuration file: thread_id and device_id shall be unique (row %d and row %d)\n", F, k, g);
                valid = 0;
            }
        }

    }
    //unique device_id
    for (int k = 0; k < n; k++) {
        int device_id_k = TSVgetis(r, k, "device_id");
        if (TSVnullreturned(r)) {
            fprintf(stderr, "%s(): check thread_device configuration file: bad format\n", F);
            return 0;
        }
        for (int g = k + 1; g < n; g++) {
            int device_id_g = TSVgetis(r, g, "device_id");
            if (TSVnullreturned(r)) {
                fprintf(stderr, "%s(): check thread_device configuration file: bad format\n", F);
                return 0;
            }
            if (device_id_k == device_id_g) {
                fprintf(stderr, "%s(): check thread_device configuration file: device_id shall be unique (row %d and row %d)\n", F, k, g);
                valid = 0;

                break;
            }
        }

    }
    return valid;
}

static int countThreadItem(int thread_id_in, TSVresult* r) {
    int c = 0;
    int n = TSVntuples(r);
    for (int k = 0; k < n; k++) {
        int thread_id = TSVgetis(r, k, "thread_id");
        if (TSVnullreturned(r)) {
            return 0;
        }
        if (thread_id == thread_id_in) {
            c++;
        }
    }
    return c;
}

int initThread(ThreadList *list, DeviceList *dl, const char *thread_path, const char *thread_device_path) {
    TSVresult tsv = TSVRESULT_INITIALIZER;
    TSVresult* r = &tsv;
    if (!TSVinit(r, thread_path)) {
        TSVclear(r);
        return 0;
    }
    int n = TSVntuples(r);
    if (n <= 0) {
        TSVclear(r);
#ifdef MODE_DEBUG
        fprintf(stderr, "%s(): no data rows in file\n", F);
#endif
        return 0;
    }
    RESIZE_M_LIST(list, n);
    NULL_LIST(list);
    printf("threads count: %d\n", n);
    NULL_LIST(list);
    if (LML != n) {
#ifdef MODE_DEBUG
        fprintf(stderr, "%s(): failure while resizing list\n", F);
#endif
        TSVclear(r);
        return 0;
    }
    for (int i = 0; i < LML; i++) {
        LIi.id = TSVgetis(r, i, "id");
        LIi.cycle_duration.tv_sec = TSVgetis(r, i, "cd_sec");
        LIi.cycle_duration.tv_nsec = TSVgetis(r, i, "cd_nsec");
        RESET_LIST(&LIi.device_plist);
        if (TSVnullreturned(r)) {
            break;
        }
        LL++;
    }
    TSVclear(r);
    if (LL != LML) {
#ifdef MODE_DEBUG
        fprintf(stderr, "%s(): failure while reading rows\n", F);
#endif
        return 0;
    }
    if (!TSVinit(r, thread_device_path)) {
        TSVclear(r);
        return 0;
    }
    n = TSVntuples(r);
    if (n <= 0) {
#ifdef MODE_DEBUG
        fprintf(stderr, "%s(): no data rows in thread device file\n", F);
#endif
        TSVclear(r);
        return 0;
    }
    if (!checkThreadDevice(r)) {
        TSVclear(r);
        return 0;
    }

    FORLi{
        int thread_device_count = countThreadItem(LIi.id, r);
        //allocating memory for thread device pointers
        RESET_LIST(&LIi.device_plist)
        if (thread_device_count <= 0) {
            continue;
        }
        RESIZE_M_LIST(&LIi.device_plist, thread_device_count);
        NULL_LIST(&LIi.device_plist);
        if (LIi.device_plist.max_length != thread_device_count) {
#ifdef MODE_DEBUG
            fprintf(stderr, "%s(): failure while resizing device_plist list\n", F);
#endif
            TSVclear(r);
            return 0;
        }
        //assigning devices to this thread
        for (int k = 0; k < n; k++) {
            int thread_id = TSVgetis(r, k, "thread_id");
            int device_id = TSVgetis(r, k, "device_id");
            if (TSVnullreturned(r)) {
                break;
            }
            if (thread_id == LIi.id) {
                Device *d = getDeviceById(device_id, dl);
                if (d == NULL) {
#ifdef MODE_DEBUG
                    fprintf(stderr, "%s(): device with id=%d not found\n", F, device_id);
#endif
                    continue;
                }
                LIi.device_plist.item[LIi.device_plist.length] = d;
                LIi.device_plist.length++;
            }
        }
        if (LIi.device_plist.max_length != LIi.device_plist.length) {
#ifdef MODE_DEBUG
            fprintf(stderr, "%s(): failure while assigning devices to threads: some devices not found\n", F);
#endif
            TSVclear(r);
            return 0;
        }
    }
    TSVclear(r);

    //starting threads
    FORLi{
        if (!createMThread(&LIi.thread, &threadFunction, &LIi)) {
            return 0;
        }
    }
    return 1;
}
