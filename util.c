
#include "main.h"

FUN_LIST_GET_BY_ID(Device)
FUN_LIST_GET_BY_ID(Thread)

int checkDevice(DeviceList *list) {
    int valid = 1;
    FORLi{
        if (LIi.type == UNKNOWN) {
            fprintf(stderr, "%s(): check device configuration file: bad type where id=%d\n", F, LIi.id);
            valid = 0;
        }
        if (LIi.mode == UNKNOWN) {
            fprintf(stderr, "%s(): check device configuration file: bad mode where id=%d\n", F, LIi.id);
            valid = 0;
        }
        if (LIi.mode == MODE_GPIO) {
            if (!checkPin(LIi.sclk)) {
                fprintf(stderr, "%s(): check device configuration file: bad sclk where id=%d\n", F, LIi.id);
                valid = 0;
            }
            if (!checkPin(LIi.miso)) {
                fprintf(stderr, "%s(): check device configuration file: bad miso where id=%d\n", F, LIi.id);
                valid = 0;
            }
            if (!checkPin(LIi.cs)) {
                fprintf(stderr, "%s(): check device configuration file: bad cs where id=%d\n", F, LIi.id);
                valid = 0;
            }
        }
        if (LIi.deviceRead == NULL) {
            fprintf(stderr, "%s(): check device configuration file: no deviceRead function assigned where id=%d\n", F, LIi.id);
            valid = 0;
        }
        if (LIi.deviceSetup == NULL) {
            fprintf(stderr, "%s(): check device configuration file: no deviceSetup function assigned where id=%d\n", F, LIi.id);
            valid = 0;
        }
        //unique id
        FORLLj
        {
            if (LIi.id == LIj.id) {
                fprintf(stderr, "%s(): check device table: id should be unique, repetition found where id=%d\n", F, LIi.id);
                valid = 0;
            }
        }

    }

    return valid;
}

void freeDeviceList(DeviceList *list) {
    FORLi{
        free(LIi.spi.path);
        freeMutex(&LIi.mutex);
        FREE_LIST(&LIi.f_list);
        fma_freeList(&LIi.fma_list);
        fexp_freeList(&LIi.fexp_list);
    }
    FREE_LIST(list);
}

void freeThreadList(ThreadList *list) {
    FORLi{
        FREE_LIST(&LIi.device_plist);
    }
    FREE_LIST(list);
}

void stopAllThreads(ThreadList * list) {
    FORLi{
#ifdef MODE_DEBUG
        printf("signaling thread %d to cancel...\n", LIi.id);
#endif
        if (pthread_cancel(LIi.thread) != 0) {
#ifdef MODE_DEBUG
            perror("pthread_cancel()");
#endif
        }
    }

    FORLi{
        void * result;
#ifdef MODE_DEBUG
        printf("joining thread %d...\n", LIi.id);
#endif
        if (pthread_join(LIi.thread, &result) != 0) {
#ifdef MODE_DEBUG
            perror("pthread_join()");
#endif
        }
        if (result != PTHREAD_CANCELED) {
#ifdef MODE_DEBUG
            printf("thread %d not canceled\n", LIi.id);
#endif
        }
    }
}

char * getEnumStr(int e) {
    switch (e) {
        case UNKNOWN:
            return "UNKNOWN";
        case TYPE_MAX6675:
            return TYPE_MAX6675_STR;
        case TYPE_MAX31855:
            return TYPE_MAX31855_STR;
        case MODE_SYS:
            return MODE_SYS_STR;
        case MODE_GPIO:
            return MODE_GPIO_STR;
    }
    return NULL;
}

void deviceRead(Device *item) {
    float v;
    int r = item->deviceRead(&v, item);
    if (r || (!r && item->result.state)) {
        if (lockMutex(&item->mutex)) {
            if (r) {
                item->result.tm = getCurrentTime();
                item->result.value = v;
                lcorrect(&item->result.value, item->lcorrection);
            }
            item->result.state = r;
            unlockMutex(&item->mutex);
        }
    }
}

int catFTS(Device *item, ACPResponse * response) {
    return acp_responseFTSCat(item->result.id, item->result.value, item->result.tm, item->result.state, response);
}

void printData(ACPResponse * response) {
#define DLi device_list.item[i]
#define TLi thread_list.item[i]
#define DPLj device_plist.item[j]
#define FMALj fma_list.item[j]
#define FEXPLj fexp_list.item[j]
#define FLj f_list.item[j]

    char q[LINE_SIZE];
    snprintf(q, sizeof q, "CONF_MAIN_FILE: %s\n", CONF_MAIN_FILE);
    SEND_STR(q)
    snprintf(q, sizeof q, "CONF_DEVICE_FILE: %s\n", CONF_DEVICE_FILE);
    SEND_STR(q)
    snprintf(q, sizeof q, "CONF_THREAD_FILE: %s\n", CONF_THREAD_FILE);
    SEND_STR(q)
    snprintf(q, sizeof q, "CONF_THREAD_DEVICE_FILE: %s\n", CONF_THREAD_DEVICE_FILE);
    SEND_STR(q)
    snprintf(q, sizeof q, "CONF_LCORRECTION_FILE: %s\n", CONF_LCORRECTION_FILE);
    SEND_STR(q)
    snprintf(q, sizeof q, "CONF_FILTER_MA_FILE: %s\n", CONF_FILTER_MA_FILE);
    SEND_STR(q)
    snprintf(q, sizeof q, "CONF_FILTER_EXP_FILE: %s\n", CONF_FILTER_EXP_FILE);
    SEND_STR(q)
    snprintf(q, sizeof q, "CONF_CHANNEL_FILTER_FILE: %s\n", CONF_CHANNEL_FILTER_FILE);
    SEND_STR(q)
    snprintf(q, sizeof q, "port: %d\n", sock_port);
    SEND_STR(q)
    snprintf(q, sizeof q, "app_state: %s\n", getAppState(app_state));
    SEND_STR(q)
    snprintf(q, sizeof q, "PID: %d\n", getpid());
    SEND_STR(q)

    acp_sendLCorrectionListInfo(&lcorrection_list, response, &peer_client);

    SEND_STR("+--------------------------------------------------------------------------+\n")
    SEND_STR("|                               device                                     |\n")
    SEND_STR("+-----------+----+--------+----+----+---------+----------------+-----------+\n")
    SEND_STR("|  pointer  | id |  type  |mode|sclk|miso| cs |     spi_path   | lcorr_ptr |\n")
    SEND_STR("+-----------+----+--------+----+----+----+----+----------------+-----------+\n")
    FORLISTN(device_list, i) {
        char *type = getEnumStr(DLi.type);
        char *mode = getEnumStr(DLi.mode);
        snprintf(q, sizeof q, "|%11p|%4d|%8s|%4s|%4d|%4d|%4d|%16s|%11p|\n",
                (void *) &DLi,
                DLi.id,
                type,
                mode,
                DLi.sclk,
                DLi.miso,
                DLi.cs,
                DLi.spi.path,
                (void *) DLi.lcorrection
                );
        SEND_STR(q)
    }

    SEND_STR("+-----------+----+--------+----+----+----+----+----------------+-----------+\n")

    SEND_STR("+-----------------------------------------------+\n")
    SEND_STR("|               channel ma filter               |\n")
    SEND_STR("+-----------+-----------+-----------+-----------+\n")
    SEND_STR("|channel_id | filter_id |filter_ptr |filter_leng|\n")
    SEND_STR("+-----------+-----------+-----------+-----------+\n")
    FORLISTN(device_list, i) {

        FORLISTN(DLi.fma_list, j) {
            snprintf(q, sizeof q, "|%11d|%11d|%11p|%11d|\n",
                    DLi.id,
                    DLi.FMALj.id,
                    (void *) &DLi.FMALj,
                    DLi.FMALj.length
                    );
            SEND_STR(q)
        }
    }

    SEND_STR("+-----------+-----------+-----------+-----------+\n")

    SEND_STR("+-----------------------------------------------+\n")
    SEND_STR("|               channel exp filter              |\n")
    SEND_STR("+-----------+-----------+-----------+-----------+\n")
    SEND_STR("|channel_id | filter_id |filter_ptr | filter_a  |\n")
    SEND_STR("+-----------+-----------+-----------+-----------+\n")
    FORLISTN(device_list, i) {

        FORLISTN(DLi.fexp_list, j) {
            snprintf(q, sizeof q, "|%11d|%11d|%11p|%11.3f|\n",
                    DLi.id,
                    DLi.FEXPLj.id,
                    (void *) &DLi.FEXPLj,
                    DLi.FEXPLj.a
                    );
            SEND_STR(q)
        }
    }

    SEND_STR("+-----------+-----------+-----------+-----------+\n")

    SEND_STR("+-----------------------+\n")
    SEND_STR("|    channel filter     |\n")
    SEND_STR("+-----------+-----------+\n")
    SEND_STR("|channel_id |filter_ptr |\n")
    SEND_STR("+-----------+-----------+\n")
    FORLISTN(device_list, i) {

        FORLISTN(DLi.f_list, j) {
            snprintf(q, sizeof q, "|%11d|%11p|\n",
                    DLi.id,
                    DLi.FLj.filter_ptr
                    );
            SEND_STR(q)
        }
    }
    SEND_STR("+-----------+-----------+\n")


    SEND_STR("+----------------------------------------------------+\n")
    SEND_STR("|               device runtime                       |\n")
    SEND_STR("+----+-----------+-----------+-----------+-----------+\n")
    SEND_STR("| id |   value   |value_state|  tm_sec   |  tm_nsec  |\n")
    SEND_STR("+----+-----------+-----------+-----------+-----------+\n")
    FORLISTN(device_list, i) {
        snprintf(q, sizeof q, "|%4d|%11.3f|%11d|%11ld|%11ld|\n",
                DLi.id,
                DLi.result.value,
                DLi.result.state,
                DLi.result.tm.tv_sec,
                DLi.result.tm.tv_nsec
                );
        SEND_STR(q)
    }

    SEND_STR("+----+-----------+-----------+-----------+-----------+\n")

    SEND_STR("+-----------------------+\n")
    SEND_STR("|      thread device    |\n")
    SEND_STR("+-----------+-----------+\n")
    SEND_STR("| thread_id | device_ptr|\n")
    SEND_STR("+-----------+-----------+\n")
    FORLISTN(thread_list, i) {

        FORLISTN(TLi.device_plist, j) {
            snprintf(q, sizeof q, "|%11d|%11p|\n",
                    TLi.id,
                    (void *) TLi.DPLj
                    );
            SEND_STR(q)
        }
    }

    SEND_STR_L("+-----------+-----------+\n")
#undef DLi 
#undef TLi
#undef DPLj
#undef UPLj
#undef FMALj
#undef FEXPLj
#undef FLj
}

void printHelp(ACPResponse * response) {
    char q[LINE_SIZE];
    SEND_STR("COMMAND LIST\n")
    snprintf(q, sizeof q, "%s\tput process into active mode; process will read configuration\n", ACP_CMD_APP_START);
    SEND_STR(q)
    snprintf(q, sizeof q, "%s\tput process into standby mode; all running programs will be stopped\n", ACP_CMD_APP_STOP);
    SEND_STR(q)
    snprintf(q, sizeof q, "%s\tfirst stop and then start process\n", ACP_CMD_APP_RESET);
    SEND_STR(q)
    snprintf(q, sizeof q, "%s\tterminate process\n", ACP_CMD_APP_EXIT);
    SEND_STR(q)
    snprintf(q, sizeof q, "%s\tget state of process; response: B - process is in active mode, I - process is in standby mode\n", ACP_CMD_APP_PING);
    SEND_STR(q)
    snprintf(q, sizeof q, "%s\tget some variable's values; response will be packed into multiple packets\n", ACP_CMD_APP_PRINT);
    SEND_STR(q)
    snprintf(q, sizeof q, "%s\tget this help; response will be packed into multiple packets\n", ACP_CMD_APP_HELP);
    SEND_STR(q)
    snprintf(q, sizeof q, "%s\tget temperature in format: sensorId\\ttemperature\\ttimeSec\\ttimeNsec\\tvalid; program id expected\n", ACP_CMD_GET_FTS);
    SEND_STR_L(q)
}
