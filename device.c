
#include "main.h"

static void printInt16(uint16_t d) {
    for (int i = 15; i >= 0; i--) {
        int v = (d >> i) & 1;
        printf("%d", v);
    }
    puts("");
}
static void printInt32(uint32_t d) {
    for (int i = 31; i >= 0; i--) {
        int v = (d >> i) & 1;
        printf("%d", v);
    }
    puts("");
}

int max6675gpio_setup(struct device_st * device) {
    return 1;
    pinModeOut(device->cs);
    pinModeOut(device->sclk);
    pinModeIn(device->miso);
    pinPUD(device->miso, PUD_DOWN);
    pinHigh(device->cs);
    return 1;
}

int max6675sys_setup(struct device_st * device) {
    return 1;
    if (!spi_setup(&device->spi)) {
        return 0;
    }
    return 1;
}

int max6675gpio_read(float *result, struct device_st * device) {
    return 1;
    int sclk = device->sclk;
    int miso = device->miso;
    int cs = device->cs;
    uint16_t v;
    pinLow(cs);
    delayUsBusy(1);
    {
        int i;
        for (i = 15; i >= 0; i--) {
            pinLow(sclk);
            delayUsBusy(1);
            if (pinRead(miso)) {
                v |= (1 << i);
            }
            pinHigh(sclk);
            delayUsBusy(1);
        }
    }
    pinHigh(cs);
#ifdef MODE_DEBUG
    printInt16(v);
#endif
    if (v & 0x4) {
#ifdef MODE_DEBUG
        fprintf(stderr, "%s(): thermocouple input is open where device_id=%d\n", F, device->id);
#endif
        return 0;
    }
    v >>= 3;
    *result = v * 0.25;
    return 1;
}

int max6675sys_read(float *result, struct device_st * device) {
    return 1;
}

int max31855gpio_setup(struct device_st * device) {
    return 1;
    pinModeOut(device->cs);
    pinModeOut(device->sclk);
    pinModeIn(device->miso);
    pinPUD(device->miso, PUD_DOWN);
    pinHigh(device->cs);
    return 1;
}

int max31855sys_setup(struct device_st * device) {
    return 1;
    if (!spi_setup(&device->spi)) {
        return 0;
    }
    return 1;
}

int max31855gpio_read(float *result, struct device_st * device) {return 1;
    int sclk = device->sclk;
    int miso = device->miso;
    int cs = device->cs;
    uint32_t v;
    pinLow(cs);
    delayUsBusy(1);
    {
        int i;
        for (i = 31; i >= 0; i--) {
            pinHigh(sclk);
            delayUsBusy(1);
            if (pinRead(miso)) {
                v |= (1 << i);
            }
            pinLow(sclk);
            delayUsBusy(1);
        }
    }
    pinHigh(cs);
#ifdef MODE_DEBUG
    printInt32(v);
#endif
    int error = 0;
    if (v & 0x20000) {
#ifdef MODE_DEBUG
        fprintf(stderr, "%s(): warning: bit 18 should be 0 where device_id=%d\n",F, device->id);
#endif
    }
    if (v & 0x8) {
#ifdef MODE_DEBUG
        fprintf(stderr, "%s(): warning: bit 4 should be 0 where device_id=%d\n",F, device->id);
#endif
    }
    if (v & 0x4) {
#ifdef MODE_DEBUG
        fprintf(stderr, "%s(): thermocouple is short-circuited to VCC  where device_id=%d\n",F, device->id);
#endif
        error = 1;
    }
    if (v & 0x2) {
#ifdef MODE_DEBUG
        fprintf(stderr, "%s(): thermocouple is short-circuited to GND  where device_id=%d\n",F, device->id);
#endif
        error = 1;
    }
    if (v & 0x1) {
#ifdef MODE_DEBUG
        fprintf(stderr, "%s(): thermocouple input is open where where device_id=%d\n",F, device->id);
#endif
        error = 1;
    }
    if (v & 0x8000) {
#ifdef MODE_DEBUG
        fprintf(stderr, "%s(): fault has been found where device_id=%d\n",F, device->id);
#endif
        error = 1;
    }
    if (error) {
        return 0;
    }
    if (v & 0x80000000) {
        v = 0xFFFFC000 | ((v >> 18) & 0x00003FFFF);
    } else {
        v >>= 18;
    }
    *result = v * 0.25;
    return 1;
}

int max31855sys_read(float *result, struct device_st * device) {
    uint32_t data;
    int v;

    spi_rw(&device->spi, (unsigned char *) &data, sizeof data);

    data = __bswap_32(data);
    data >>= 18;
    v = data & 0x1FFF;
    if ((data & 0x2000) != 0) {
        v = -v;
    }
    *result = ((((float) v * 25) + 0.5) / 10.0);
    return 1;
}