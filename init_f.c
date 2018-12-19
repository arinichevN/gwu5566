#include "main.h"

int readSettings(int *sock_port, const char *data_path) {
    TSVresult tsv = TSVRESULT_INITIALIZER;
    TSVresult* r = &tsv;
    if (!TSVinit(r, data_path)) {
        TSVclear(r);
        return 0;
    }
    int _sock_port = TSVgetis ( r, 0, "port" );
    if ( TSVnullreturned ( r ) ) {
        TSVclear(r);
        return 0;
    }
    *sock_port = _sock_port;
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

static int (*getReadFunctionForDevice(int type, int mode)) (double *, struct device_st *) {
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
        putsde("failure while resizing list\n");
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
        LIi.miso = TSVgetis(r, i, "miso");
        LIi.cs = TSVgetis(r, i, "cs");
        strcpyma(&LIi.spi.path, TSVgetvalues(r, i, "spi_path"));
        int lcorrection_id = TSVgetis(r, i, "lcorrection_id");
        LIST_GETBYID ( LIi.lcorrection, lcl, lcorrection_id);
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
        putsde("failure while reading rows\n");
        return 0;
    }
    return 1;
}

static int countChannelMaExpLength(int *ma_length, int *exp_length, int channel_id, TSVresult* r_ma, TSVresult* r_exp, TSVresult* r_map, int n_map, int n_ma, int n_exp) {
    *ma_length = *exp_length = 0;
    for (int j = 0; j < n_map; j++) {
        int fchannel_id = TSVgetis(r_map, j, "channel_id");
        if (TSVnullreturned(r_map)) {
            return 0;
        }
        if (channel_id == fchannel_id) {
            (*ma_length)++;
            (*exp_length)++;
        }
    }
    return 1;
}

static int fillChannelFilter(FilterMAList *ma_list, FilterEXPList *exp_list, FilterList *f_list, int channel_id, TSVresult* r_ma, TSVresult* r_exp, TSVresult* r_map, int n_map, int n_ma, int n_exp) {
    for (int j = 0; j < n_map; j++) {
        int fchannel_id = TSVgetis(r_map, j, "channel_id");
        if (TSVnullreturned(r_map)) {
            return 0;
        }
        if (channel_id == fchannel_id) {
            int filter_id = TSVgetis(r_map, j, "filter_id");
            if (TSVnullreturned(r_map)) {
                return 0;
            }
            for (int k = 0; k < n_ma; k++) {
                int filter_p_id = TSVgetis(r_ma, k, "id");
                int filter_p_length = TSVgetis(r_ma, k, "length");
                if (TSVnullreturned(r_ma)) {
                    return 0;
                }
                if (filter_p_id == filter_id) {
                    if (ma_list->length >= ma_list->max_length) {
                        printde("ma_list overflow where channel_id=%d and filter_id=%d\n", channel_id, filter_id);
                        return 0;
                    }
                    if (!fma_init(&ma_list->item[ma_list->length], filter_p_id, filter_p_length)) {
                        return 0;
                    }
                    ma_list->length++;
                    if (f_list->length >= f_list->max_length) {
                        printde("f_list overflow where channel_id=%d and filter_id=%d\n", channel_id, filter_id);
                        return 0;
                    }
                    f_list->item[f_list->length].filter_ptr = &ma_list->item[ma_list->length-1];
                    f_list->item[f_list->length].filter_fun = fma_calc;
                    f_list->length++;
                }
            }
            for (int k = 0; k < n_exp; k++) {
                int filter_p_id = TSVgetis(r_exp, k, "id");
                float filter_p_a = TSVgetfs(r_exp, k, "a");
                if (TSVnullreturned(r_exp)) {
                    return 0;
                }
                if (filter_p_id == filter_id) {
                    if (exp_list->length >= exp_list->max_length) {
                        printde("exp_list overflow where channel_id=%d and filter_id=%d\n", channel_id, filter_id);
                        return 0;
                    }
                    if (!fexp_init(&exp_list->item[exp_list->length], filter_p_id, filter_p_a)) {
                        return 0;
                    }
                    exp_list->length++;
                    if (f_list->length >= f_list->max_length) {
                        printde("f_list overflow where channel_id=%d and filter_id=%d\n", channel_id, filter_id);
                        return 0;
                    }
                    f_list->item[f_list->length].filter_ptr = &exp_list->item[exp_list->length-1];
                    f_list->item[f_list->length].filter_fun = fexp_calc;
                    f_list->length++;
                }
            }
        }
    }
    return 1;
}

int initDeviceFilter(DeviceList *list, const char *ma_path, const char *exp_path, const char *mapping_path) {
#define CLEAR_TSV_LIB  TSVclear(r_map);TSVclear(r_exp);TSVclear(r_ma);
#define RETURN_FAILURE  CLEAR_TSV_LIB return 0;
    TSVresult tsv1 = TSVRESULT_INITIALIZER;
    TSVresult* r_ma = &tsv1;
    if (!TSVinit(r_ma, ma_path)) {
        TSVclear(r_ma);
        return 0;
    }
    TSVresult tsv2 = TSVRESULT_INITIALIZER;
    TSVresult* r_exp = &tsv2;
    if (!TSVinit(r_exp, exp_path)) {
        TSVclear(r_exp);
        TSVclear(r_ma);
        return 0;
    }
    TSVresult tsv3 = TSVRESULT_INITIALIZER;
    TSVresult* r_map = &tsv3;
    if (!TSVinit(r_map, mapping_path)) {
        RETURN_FAILURE;
    }
    int n_map = TSVntuples(r_map);
    int n_ma = TSVntuples(r_ma);
    int n_exp = TSVntuples(r_exp);
    FORL{
        int ma_length = 0;
        int exp_length = 0;
        if (!countChannelMaExpLength(&ma_length, &exp_length, LIi.id, r_ma, r_exp, r_map, n_map, n_ma, n_exp)) {
            RETURN_FAILURE;
        }
        RESET_LIST(&LIi.fma_list);
        RESIZE_M_LIST(&LIi.fma_list, ma_length);
        if (LIi.fma_list.max_length != ma_length) {
            printde("failure while resizing fma_list where channel_id=%d\n", LIi.id);
            RETURN_FAILURE;
        }
        RESET_LIST(&LIi.fexp_list);
        RESIZE_M_LIST(&LIi.fexp_list, exp_length);
        if (LIi.fexp_list.max_length != exp_length) {
            printde("failure while resizing fexp_list where channel_id=%d\n", LIi.id);
            RETURN_FAILURE;
        }
        int f_length = exp_length + ma_length;
        RESET_LIST(&LIi.f_list);
        RESIZE_M_LIST(&LIi.f_list, f_length);
        if (LIi.f_list.max_length != f_length) {
            printde( "failure while resizing f_list where channel_id=%d\n", LIi.id);
            RETURN_FAILURE;
        }
        if (!fillChannelFilter(&LIi.fma_list, &LIi.fexp_list, &LIi.f_list, LIi.id, r_ma, r_exp, r_map, n_map, n_ma, n_exp)) {
            RETURN_FAILURE;
        }
    }
    CLEAR_TSV_LIB;
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
        putsde("no data rows in file\n");
        return 0;
    }
    RESIZE_M_LIST(list, n);
    NULL_LIST(list);
    printdo("threads count: %d\n", n);
    NULL_LIST(list);
    if (LML != n) {
        putsde("failure while resizing list\n");
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
        putsde("failure while reading rows\n");
        return 0;
    }
    if (!TSVinit(r, thread_device_path)) {
        TSVclear(r);
        return 0;
    }
    n = TSVntuples(r);
    if (n <= 0) {
        putsde("no data rows in thread device file\n");
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
            putsde("failure while resizing device_plist list\n");
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
                Device *d;
                LIST_GETBYID ( d, dl, device_id);
                if (d == NULL) {
                    printde("device with id=%d not found\n", device_id);
                    continue;
                }
                LIi.device_plist.item[LIi.device_plist.length] = d;
                LIi.device_plist.length++;
            }
        }
        if (LIi.device_plist.max_length != LIi.device_plist.length) {
            putsde("failure while assigning devices to threads: some devices not found\n");
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
