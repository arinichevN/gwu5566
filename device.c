
#include "main.h"

static void printInt16 ( uint16_t d ) {
    for ( int i = 15; i >= 0; i-- ) {
        int v = ( d >> i ) & 1;
        printf ( "%d", v );
    }
}
static void printInt32 ( uint32_t d ) {
    for ( int i = 31; i >= 0; i-- ) {
        int v = ( d >> i ) & 1;
        printf ( "%d", v );
    }
}

int max6675gpio_setup ( struct device_st * device ) {
    pinModeOut ( device->cs );
    pinModeOut ( device->sclk );
    pinModeIn ( device->miso );
    pinPUD ( device->cs, PUD_OFF );
    pinPUD ( device->sclk, PUD_OFF );
    pinPUD ( device->miso, PUD_OFF );
    pinHigh ( device->cs );
    pinLow ( device->sclk );
    return 1;
}

int max6675sys_setup ( struct device_st * device ) {
    return 1;
    if ( !spi_setup ( &device->spi ) ) {
        return 0;
    }
    return 1;
}

int max6675gpio_read ( double *result, struct device_st * device ) {
    int sclk = device->sclk;
    int miso = device->miso;
    int cs = device->cs;
    uint16_t v=0;
    pinLow ( cs );
    DELAY_100NS;
    for ( int i = 15; i >= 0; i-- ) {
        pinHigh ( sclk );
        DELAY_100NS;
        if ( pinRead ( miso ) ) {
            v |= ( (uint16_t)1 << i );
        }
        pinLow ( sclk );
        DELAY_100NS;
    }
    pinHigh ( cs );
#ifdef MODE_DEBUG
    printf ( "%d: ", device->id );
    printInt16 ( v );
#endif
    if ( v & 0x4 ) {
#ifdef MODE_DEBUG
        fprintf ( stderr, "%s(): thermocouple input is open where device_id=%d\n", F, device->id );
#endif
        return 0;
    }
    v >>= 3;
    *result = v * 0.25;
#ifdef MODE_DEBUG
    printf ( " = %f\n", *result );
#endif
    return 1;
}

int max6675sys_read ( double *result, struct device_st * device ) {
    *result=0.0f;
    return 1;
}

int max31855gpio_setup ( struct device_st * device ) {
    pinModeOut ( device->cs );
    pinModeOut ( device->sclk );
    pinModeIn ( device->miso );
    pinPUD ( device->cs, PUD_OFF );
    pinPUD ( device->sclk, PUD_OFF );
    pinPUD ( device->miso, PUD_OFF );
    pinHigh ( device->cs );
    pinLow ( device->sclk );
    return 1;
}

int max31855sys_setup ( struct device_st * device ) {
    return 1;
    if ( !spi_setup ( &device->spi ) ) {
        return 0;
    }
    return 1;
}

int max31855gpio_read ( double *result, struct device_st * device ) {
    int sclk = device->sclk;
    int miso = device->miso;
    int cs = device->cs;
    uint32_t v=0;
    pinLow ( cs );
    DELAY_100NS;
    for ( int i = 31; i >= 0; i-- ) {
        pinHigh ( sclk );
        DELAY_100NS;
        if ( pinRead ( miso ) ) {
            v |= ( (uint32_t)1 << i );
        }
        pinLow ( sclk );
        DELAY_100NS;
    }
    pinHigh ( cs );
#ifdef MODE_DEBUG
    printf ( "%d: ", device->id );
    printInt32 ( v );
#endif
    int error = 0;
    if ( v & 0x20000 ) {
#ifdef MODE_DEBUG
        fprintf ( stderr, "%s(): warning: bit 18 should be 0 where device_id=%d\n",F, device->id );
#endif
    }
    if ( v & 0x8 ) {
#ifdef MODE_DEBUG
        fprintf ( stderr, "%s(): warning: bit 4 should be 0 where device_id=%d\n",F, device->id );
#endif
    }
    if ( v & 0x4 ) {
#ifdef MODE_DEBUG
        fprintf ( stderr, "%s(): thermocouple is short-circuited to VCC  where device_id=%d\n",F, device->id );
#endif
        error = 1;
    }
    if ( v & 0x2 ) {
#ifdef MODE_DEBUG
        fprintf ( stderr, "%s(): thermocouple is short-circuited to GND  where device_id=%d\n",F, device->id );
#endif
        error = 1;
    }
    if ( v & 0x1 ) {
#ifdef MODE_DEBUG
        fprintf ( stderr, "%s(): thermocouple input is open where where device_id=%d\n",F, device->id );
#endif
        error = 1;
    }
    if ( v & 0x8000 ) {
#ifdef MODE_DEBUG
        fprintf ( stderr, "%s(): fault has been found where device_id=%d\n",F, device->id );
#endif
        error = 1;
    }
    if ( error ) {
        return 0;
    }
    /*
    if (v & 0x80000000) {
        v = 0xFFFFC000 | ((v >> 18) & 0x00003FFFF);
    } else {
        v >>= 18;
    }
    *result = v * 0.25;
    * */
    double r;
    if ( v & 0x80000000 ) { //-
        v=~v;
        v= ( v >> 18 );
        r = ( v *0.25+0.25 ) *-1;
    } else {//+
        v >>= 18;
        r = v *0.25;
    }
    *result = r;
#ifdef MODE_DEBUG
    printf ( " = %f\n", *result );
#endif
    return 1;
}

int max31855sys_read ( double *result, struct device_st * device ) {
    uint32_t data;
    int v;

    spi_rw ( &device->spi, ( unsigned char * ) &data, sizeof data );

    data = __bswap_32 ( data );
    data >>= 18;
    v = data & 0x1FFF;
    if ( ( data & 0x2000 ) != 0 ) {
        v = -v;
    }
    *result = ( ( ( ( double ) v * 25 ) + 0.5 ) / 10.0 );
    return 1;
}
