/*
 * Adds RTLSDR Dongles capability to SDRNode
 * Copyright (C) 2016 Sylvain AZARIAN <sylvain.azarian@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <sys/types.h>
#include <pthread.h>
#include <semaphore.h>
#include <time.h>
#include <stdlib.h>
#include <unistd.h>
#include <math.h>

#include <rtl-sdr.h>

#include "jansson/jansson.h"

#include "entrypoint.h"
#define DEBUG_DRIVER (0)

char *driver_name ;
void* acquisition_thread( void *params ) ;

typedef struct __attribute__ ((__packed__)) _sCplx
{
    float re;
    float im;
} TYPECPX;

struct t_sample_rates {
    unsigned int *sample_rates ;
    int enum_length ;
    int preffered_sr_index ;
};

// this structure stores the device state
struct t_rx_device {

    rtlsdr_dev_t *rtlsdr_device ;
    char *device_name ;
    char *device_serial_number ;

    struct t_sample_rates* rates;
    int current_sample_rate ;

    int64_t min_frq_hz ; // minimal frequency for this device
    int64_t max_frq_hz ; // maximal frequency for this device
    int64_t center_frq_hz ; // currently set frequency


    float gain ;
    float gain_min ;
    float gain_max ;
    int gain_size ;
    int *gain_values;

    char *uuid ;
    bool running ;
    bool acq_stop ;
    sem_t mutex;

    pthread_t receive_thread ;
    // for DC removal
    TYPECPX xn_1 ;
    TYPECPX yn_1 ;

    struct ext_Context context ;
};

int device_count ;
char *stage_name ;
char *stage_unit ;

struct t_rx_device *rx;
json_t *root_json ;
_tlogFun* sdrNode_LogFunction ;
_pushSamplesFun *acqCbFunction ;

#ifdef _WIN64
#include <windows.h>
// Win  DLL Main entry
BOOL WINAPI DllMain( HINSTANCE hInstance, DWORD dwReason, LPVOID *lpvReserved ) {
    return( TRUE ) ;
}
#endif

void log( int device_id, int level, char *msg ) {
    if( sdrNode_LogFunction != NULL ) {
        (*sdrNode_LogFunction)(rx[device_id].uuid,level,msg);
        return ;
    }
    printf("Trace:%s\n", msg );
}



/*
 * First function called by SDRNode - must return 0 if hardware is not present or problem
 */
/**
 * @brief initLibrary is called when the DLL is loaded, only for the first instance of the devices (when the getBoardCount() function returns
 *        more than 1)
 * @param json_init_params a JSOn structure to pass parameters from scripting to drivers
 * @param ptr pointer to function for logging
 * @param acqCb pointer to RF IQ processing function
 * @return
 */
LIBRARY_API int initLibrary(char *json_init_params,
                            _tlogFun* ptr,
                            _pushSamplesFun *acqCb ) {
    json_error_t error;
    root_json = NULL ;
    struct t_rx_device *tmp ;
    int rc ;
    char manufact[256], product[256], serial[256] ;

    sdrNode_LogFunction = ptr ;
    acqCbFunction = acqCb ;

    if( json_init_params != NULL ) {
        root_json = json_loads(json_init_params, 0, &error);

    }
    if( DEBUG_DRIVER ) fprintf(stderr,"%s\n", __func__);

    driver_name = (char *)malloc( 100*sizeof(char));
    snprintf(driver_name,100,"RTLSDR");

    // Step 1 : count how many devices we have
    device_count = (int)rtlsdr_get_device_count();
    if( device_count == 0 ) {
        return(0); // no hardware
    }

    rx = (struct t_rx_device *)malloc( device_count * sizeof(struct t_rx_device));
    if( rx == NULL ) {
        return(0);
    }
    tmp = rx ;
    // iterate through devices to populate structure
    for( int d=0 ; d < device_count ; d++ , tmp++ ) {
        //
        rc = (int)rtlsdr_open( &tmp->rtlsdr_device, d );
        if( rc < 0 ) {
            // cannot open this device
            return(0);
        }
        if( DEBUG_DRIVER ) fprintf(stderr,"%s rtlsdr_open(%d) okay\n", __func__, d);

        tmp->uuid = NULL ;
        tmp->running = false ;
        tmp->acq_stop = false ;
        sem_init(&tmp->mutex, 0, 0);

        tmp->device_name = (char *)malloc( 64 *sizeof(char));
        tmp->device_serial_number = (char *)malloc( 16 *sizeof(char));
        rc = rtlsdr_get_usb_strings( tmp->rtlsdr_device, manufact, product, serial );
        if( rc == 0 ) {
            sprintf( tmp->device_serial_number, "%s", serial );
        }

        tmp->min_frq_hz = 70e6 ;
        tmp->max_frq_hz = 1700e6 ;
        enum rtlsdr_tuner ttype = rtlsdr_get_tuner_type( tmp->rtlsdr_device ) ;
        switch( ttype ) {
        case RTLSDR_TUNER_UNKNOWN:
            sprintf( tmp->device_name, "RTL%s", "SDR");
            break ;
        case RTLSDR_TUNER_E4000 :
            sprintf( tmp->device_name, "RTL%s", "E4000");
            tmp->min_frq_hz = 52e6 ;
            tmp->max_frq_hz = 2200e6 ;
            break ;
        case RTLSDR_TUNER_R820T:
            sprintf( tmp->device_name, "RTL%s", "820T");
            tmp->min_frq_hz = 24e6 ;
            tmp->max_frq_hz = 1766e6 ;
            break ;
        case RTLSDR_TUNER_R828D:
            sprintf( tmp->device_name, "RTL%s", "828D");
            tmp->min_frq_hz = 24e6 ;
            tmp->max_frq_hz = 1766e6 ;
            break ;
        case RTLSDR_TUNER_FC0013:
            sprintf( tmp->device_name, "RTL%s", "FC13");
            tmp->min_frq_hz = 22e6 ;
            tmp->max_frq_hz = 1100e6 ;
            break ;
        case RTLSDR_TUNER_FC0012:
            sprintf( tmp->device_name, "RTL%s", "FC12");
            tmp->min_frq_hz = 22e6 ;
            tmp->max_frq_hz = 948e6 ;
            break ;
        case RTLSDR_TUNER_FC2580:
            sprintf( tmp->device_name, "RTL%s", "FC2580");
            tmp->min_frq_hz = 146e6 ;
            tmp->max_frq_hz = 924e6 ;
            break ;

        default:
            sprintf( tmp->device_name, "RTL%s", "SDR");
            break ;
        }

        tmp->center_frq_hz = tmp->min_frq_hz + 1e6 ; // arbitrary startup freq

        // allocate rates
        tmp->rates = (struct t_sample_rates*)malloc( sizeof(struct t_sample_rates));
        tmp->rates->enum_length = 5 ; // we manage 5 different sampling rates
        tmp->rates->sample_rates = (unsigned int *)malloc( tmp->rates->enum_length * sizeof( unsigned int )) ;
        tmp->rates->sample_rates[0] = 256*1000 ;
        tmp->rates->sample_rates[1] = 1000*1000 ;
        tmp->rates->sample_rates[2] = 1024*1000 ;
        tmp->rates->sample_rates[3] = 2000*1000 ;
        tmp->rates->sample_rates[4] = 2*1024*1000 ;
        tmp->rates->preffered_sr_index = 2 ; // our default sampling rate will be 1024 KHz


        // set default SR
        tmp->current_sample_rate = tmp->rates->sample_rates[tmp->rates->preffered_sr_index] ;
        rc = rtlsdr_set_sample_rate( tmp->rtlsdr_device, tmp->current_sample_rate );

        // disable AGC
        rtlsdr_set_agc_mode( tmp->rtlsdr_device, 0 );
        tmp->gain_size = rtlsdr_get_tuner_gains( tmp->rtlsdr_device, NULL );
        tmp->gain_values = (int *)malloc( tmp->gain_size * sizeof(int) );
        tmp->gain_min = 999 ;
        tmp->gain_max = 0 ;
        rtlsdr_get_tuner_gains( tmp->rtlsdr_device, tmp->gain_values );
        for( rc = 0 ; rc < tmp->gain_size ;rc ++ ) {
            if( DEBUG_DRIVER ) fprintf( stderr, "gain[%d]=%d\n",  rc, tmp->gain_values[rc] );
            float v = tmp->gain_values[rc] / 10.0 ;
            if( v > 0 ) {
                if( v < tmp->gain_min ) tmp->gain_min = v ;
                else
                    if( v > tmp->gain_max ) tmp->gain_max = v ;
            }
        }
        tmp->gain = tmp->gain_min + (tmp->gain_max - tmp->gain_min)/2 ;

        tmp->context.ctx_version = 0 ;

        // create acquisition threads
        pthread_create(&tmp->receive_thread, NULL, acquisition_thread, tmp );
    }

    // all RTLSDR have one single gain stage
    stage_name = (char *)malloc( 10*sizeof(char));
    snprintf( stage_name,10,"RFGain");
    stage_unit = (char *)malloc( 10*sizeof(char));
    snprintf( stage_unit,10,"dB");

    srand(time(NULL));
    return(RC_OK);
}


/**
 * @brief setBoardUUID this function is called by SDRNode to assign a unique ID to each device managed by the driver
 * @param device_id [0..getBoardCount()[
 * @param uuid the unique ID
 * @return
 */
LIBRARY_API int setBoardUUID( int device_id, char *uuid ) {
    int len = 0 ;

    if( DEBUG_DRIVER ) fprintf(stderr,"%s(%d,%s)\n", __func__, device_id, uuid );

    if( uuid == NULL ) {
        return(RC_NOK);
    }
    if( device_id >= device_count )
        return(RC_NOK);

    len = strlen(uuid);
    if( rx[device_id].uuid != NULL ) {
        free( rx[device_id].uuid );
    }
    rx[device_id].uuid = (char *)malloc( len * sizeof(char));
    strcpy( rx[device_id].uuid, uuid);
    return(RC_OK);
}

/**
 * @brief getHardwareName called by SDRNode to retrieve the name for the nth device
 * @param device_id [0..getBoardCount()[
 * @return a string with the hardware name, this name is listed in the 'devices' admin page and appears 'as is' in the scripts
 */
LIBRARY_API char *getHardwareName(int device_id) {
    if( DEBUG_DRIVER ) fprintf(stderr,"%s\n", __func__);
    if( device_id >= device_count )
        return(NULL);
    struct t_rx_device *dev = &rx[device_id] ;
    return( dev->device_name );
}

/**
 * @brief getBoardCount called by SDRNode to retrieve the number of different boards managed by the driver
 * @return the number of devices managed by the driver
 */
LIBRARY_API int getBoardCount() {
    if( DEBUG_DRIVER ) fprintf(stderr,"%s\n", __func__);
    return(device_count);
}

/**
 * @brief getPossibleSampleRateCount called to know how many sample rates are available. Used to fill the select zone in admin
 * @param device_id
 * @return sample rate in Hz
 */
LIBRARY_API int getPossibleSampleRateCount(int device_id) {
    if( DEBUG_DRIVER ) fprintf(stderr,"%s\n", __func__);
    if( device_id >= device_count )
        return(0);
    struct t_rx_device *dev = &rx[device_id] ;
    return( dev->rates->enum_length );
}

/**
 * @brief getPossibleSampleRateValue
 * @param device_id
 * @param index
 * @return
 */
LIBRARY_API unsigned int getPossibleSampleRateValue(int device_id, int index) {
    if( DEBUG_DRIVER ) fprintf(stderr,"%s(%d)\n", __func__, index );
    if( device_id >= device_count )
        return(0);
    struct t_rx_device *dev = &rx[device_id] ;

    struct t_sample_rates* rates = dev->rates ;
    if( index > rates->enum_length )
        return(0);

    return( rates->sample_rates[index] );
}

LIBRARY_API unsigned int getPrefferedSampleRateValue(int device_id) {
    if( DEBUG_DRIVER ) fprintf(stderr,"%s\n", __func__);
    if( device_id >= device_count )
        return(0);
    struct t_rx_device *dev = &rx[device_id] ;
    struct t_sample_rates* rates = dev->rates ;
    int index = rates->preffered_sr_index ;
    return( rates->sample_rates[index] );
}
//-------------------------------------------------------------------
LIBRARY_API int64_t getMin_HWRx_CenterFreq(int device_id) {
    if( DEBUG_DRIVER ) fprintf(stderr,"%s\n", __func__);
    if( device_id >= device_count )
        return(0);
    struct t_rx_device *dev = &rx[device_id] ;
    return( dev->min_frq_hz ) ;
}

LIBRARY_API int64_t getMax_HWRx_CenterFreq(int device_id) {
    if( DEBUG_DRIVER ) fprintf(stderr,"%s\n", __func__);
    if( device_id >= device_count )
        return(0);
    struct t_rx_device *dev = &rx[device_id] ;
    return( dev->max_frq_hz ) ;
}

//-------------------------------------------------------------------
// Gain management
// devices have stages (LNA, VGA, IF...) . Each stage has its own gain
// range, its own name and its own unit.
// each stage can be 'continuous gain' or 'discrete' (on/off for example)
//-------------------------------------------------------------------
LIBRARY_API int getRxGainStageCount(int device_id) {
    if( DEBUG_DRIVER ) fprintf(stderr,"%s(%d)\n", __func__, device_id);
    // RTLSDR have only one stage
    return(1);
}

LIBRARY_API char* getRxGainStageName( int device_id, int stage) {
    if( DEBUG_DRIVER ) fprintf(stderr,"%s(%d,%d)\n", __func__, device_id, stage );
    // RTLSDR have only one stage so the name is same for all
    return( stage_name );
}

LIBRARY_API char* getRxGainStageUnitName( int device_id, int stage) {
    if( DEBUG_DRIVER ) fprintf(stderr,"%s(%d,%d)\n", __func__, device_id, stage );
    // RTLSDR have only one stage so the unit is same for all
    return( stage_unit );
}

LIBRARY_API int getRxGainStageType( int device_id, int stage) {
    if( DEBUG_DRIVER ) fprintf(stderr,"%s(%d,%d)\n", __func__, device_id, stage );
    // continuous value
    return(0);
}

LIBRARY_API float getMinGainValue(int device_id,int stage) {
    if( DEBUG_DRIVER ) fprintf(stderr,"%s(%d,%d)\n", __func__, device_id, stage );
    if( device_id >= device_count )
        return(0);
    struct t_rx_device *dev = &rx[device_id] ;
    return( dev->gain_min ) ;
}

LIBRARY_API float getMaxGainValue(int device_id,int stage) {
    if( DEBUG_DRIVER ) fprintf(stderr,"%s(%d,%d)\n", __func__, device_id, stage );
    if( device_id >= device_count )
        return(0);
    struct t_rx_device *dev = &rx[device_id] ;
    return( dev->gain_max ) ;
}

LIBRARY_API int getGainDiscreteValuesCount( int device_id, int stage ) {
    if( DEBUG_DRIVER ) fprintf(stderr,"%s(%d,%d)\n", __func__, device_id, stage);
    return(0);
}

LIBRARY_API float getGainDiscreteValue( int device_id, int stage, int index ) {
    if( DEBUG_DRIVER ) fprintf(stderr,"%s(%d, %d,%d)\n", __func__, device_id, stage, index);
    return(0);
}

/**
 * @brief getSerialNumber returns the (unique for this hardware name) serial number. Serial numbers are useful to manage more than one unit
 * @param device_id
 * @return
 */
LIBRARY_API char* getSerialNumber( int device_id ) {
    if( DEBUG_DRIVER ) fprintf(stderr,"%s(%d)\n", __func__, device_id);
    if( device_id >= device_count )
        return(RC_NOK);
    struct t_rx_device *dev = &rx[device_id] ;
    return( dev->device_serial_number );
}

//----------------------------------------------------------------------------------
// Manage acquisition
// SDRNode calls 'prepareRxEngine(device)' to ask for the start of acquisition
// Then, the driver shall call the '_pushSamplesFun' function passed at initLibrary( ., ., _pushSamplesFun* fun , ...)
// when the driver shall stop, SDRNode calls finalizeRXEngine()

/**
 * @brief prepareRXEngine trig on the acquisition process for the device
 * @param device_id
 * @return RC_OK if streaming has started, RC_NOK otherwise
 */
LIBRARY_API int prepareRXEngine( int device_id ) {
    if( DEBUG_DRIVER ) fprintf(stderr,"%s(%d)\n", __func__, device_id);
    if( device_id >= device_count )
        return(RC_NOK);

    // here we keep it simple, just fire the relevant mutex
    struct t_rx_device *dev = &rx[device_id] ;
    dev->acq_stop = false ;
    rtlsdr_reset_buffer( dev->rtlsdr_device);
    sem_post(&dev->mutex);

    return(RC_OK);
}

/**
 * @brief finalizeRXEngine stops the acquisition process
 * @param device_id
 * @return
 */
LIBRARY_API int finalizeRXEngine( int device_id ) {
    if( DEBUG_DRIVER ) fprintf(stderr,"%s(%d)\n", __func__, device_id);
    if( device_id >= device_count )
        return(RC_NOK);

    struct t_rx_device *dev = &rx[device_id] ;
    dev->acq_stop = true ;
    rtlsdr_cancel_async( dev->rtlsdr_device ) ;

    return(RC_OK);
}

/**
 * @brief setRxSampleRate configures the sample rate for the device (in Hz). Can be different from the enum given by getXXXSampleRate
 * @param device_id
 * @param sample_rate
 * @return
 */
LIBRARY_API int setRxSampleRate( int device_id , int sample_rate) {
    if( DEBUG_DRIVER ) fprintf(stderr,"%s(%d,%d)\n", __func__, device_id,sample_rate);
    if( device_id >= device_count )
        return(RC_NOK);

    struct t_rx_device *dev = &rx[device_id] ;
    if( sample_rate == dev->current_sample_rate ) {
        return(RC_OK);
    }
    if( (sample_rate<225001)) {
        sample_rate = 256e3 ;
    } else if( (sample_rate>300e3) && (sample_rate<900e3)) {
        sample_rate = 1000e3 ;
    } else if( sample_rate>3200e3) {
        sample_rate = 3200e3 ;
    }

    int rc = rtlsdr_set_sample_rate( dev->rtlsdr_device, sample_rate );
    if( rc == 0 ) {
        dev->current_sample_rate = sample_rate ;
        dev->context.ctx_version++ ;
        dev->context.sample_rate = sample_rate ;
    } else {
        dev->current_sample_rate = rtlsdr_get_sample_rate( dev->rtlsdr_device );
    }
    return(RC_OK);
}

/**
 * @brief getActualRxSampleRate called to know what is the actual sampling rate (hz) for the given device
 * @param device_id
 * @return
 */
LIBRARY_API int getActualRxSampleRate( int device_id ) {
    if( DEBUG_DRIVER ) fprintf(stderr,"%s(%d)\n", __func__, device_id);
    if( device_id >= device_count )
        return(RC_NOK);
    struct t_rx_device *dev = &rx[device_id] ;
    return(dev->current_sample_rate);
}

/**
 * @brief setRxCenterFreq tunes device to frq_hz (center frequency)
 * @param device_id
 * @param frq_hz
 * @return
 */
LIBRARY_API int setRxCenterFreq( int device_id, int64_t frq_hz ) {
    if( DEBUG_DRIVER ) fprintf(stderr,"%s(%d,%ld)\n", __func__, device_id, (long)frq_hz);
    if( DEBUG_DRIVER ) fflush(stderr);
    if( device_id >= device_count )
        return(RC_NOK);

    struct t_rx_device *dev = &rx[device_id] ;
    int rc = rtlsdr_set_center_freq( dev->rtlsdr_device, (uint32_t)frq_hz  );
    if( rc == 0 ) {
        dev->center_frq_hz = frq_hz ;
        dev->context.ctx_version++ ;
        dev->context.center_freq = frq_hz ;
        return(RC_OK);
    }
    return(RC_NOK);
}

/**
 * @brief getRxCenterFreq retrieve the current center frequency for the device
 * @param device_id
 * @return
 */
LIBRARY_API int64_t getRxCenterFreq( int device_id ) {
    if( DEBUG_DRIVER ) fprintf(stderr,"%s(%d)\n", __func__, device_id);
    if( device_id >= device_count )
        return(RC_NOK);

    struct t_rx_device *dev = &rx[device_id] ;
    int64_t frequency = (int64_t)rtlsdr_get_center_freq( dev->rtlsdr_device ) ;
    if( frequency > 0 ) {
        dev->center_frq_hz = frequency ;
    }
    return( dev->center_frq_hz ) ;
}

/**
 * @brief setRxGain sets the current gain
 * @param device_id
 * @param stage_id
 * @param gain_value
 * @return
 */
LIBRARY_API int setRxGain( int device_id, int stage_id, float gain_value ) {
    if( DEBUG_DRIVER ) fprintf(stderr,"%s(%d,%d,%f)\n", __func__, device_id,stage_id,gain_value);
    if( device_id >= device_count )
        return(RC_NOK);
    if( stage_id >= 1 )
        return(RC_NOK);

    struct t_rx_device *dev = &rx[device_id] ;
    // make sure device is in manual gain mode
    int rc = rtlsdr_set_tuner_gain_mode( dev->rtlsdr_device, 1);

    if( rc == 0 ) {
        // check value against device range
        if( gain_value > dev->gain_max ) {
            gain_value = dev->gain_max ;
        }
        if( gain_value < dev->gain_min ) {
            gain_value = dev->gain_min ;
        }
        // find the most appropriate device value
        int tenthdb = (int)(gain_value*10); // RTLSDR gains are in tenth of db
        for( int k=0 ; k < dev->gain_size-1 ; k++ ) {
            if( ( dev->gain_values[k]<=tenthdb) && (dev->gain_values[k+1]>=tenthdb)) {
                tenthdb = dev->gain_values[k];
                break ;
            }
        }
        // set it
        rc = rtlsdr_set_tuner_gain( dev->rtlsdr_device, tenthdb );
        if( rc == 0 ) {
            // keep value
            dev->gain = tenthdb/10.0 ;
            return(RC_OK);
        }
    }
    return(RC_NOK);
}

/**
 * @brief getRxGainValue reads the current gain value
 * @param device_id
 * @param stage_id
 * @return
 */
LIBRARY_API float getRxGainValue( int device_id , int stage_id ) {
    if( DEBUG_DRIVER ) fprintf(stderr,"%s(%d,%d)\n", __func__, device_id,stage_id);

    if( device_id >= device_count )
        return(RC_NOK);
    if( stage_id >= 1 )
        return(RC_NOK);
    struct t_rx_device *dev = &rx[device_id] ;
    int rc = rtlsdr_get_tuner_gain( dev->rtlsdr_device ) ;
    if( rc > 0 ) {
        dev->gain = rc/10.0 ;
    }
    return( dev->gain) ;
}

LIBRARY_API bool setAutoGainMode( int device_id ) {
    if( DEBUG_DRIVER ) fprintf(stderr,"%s(%d)\n", __func__, device_id);
    if( device_id >= device_count )
        return(false);
    return(false);
}

//-----------------------------------------------------------------------------------------
// functions below are RTLSDR specific
// One thread is started by device, and each sample frame calls rtlsdr_callback() with a block
// of IQ samples as bytes.
// Samples are converted to float, DC is removed and finally samples are passed to SDRNode
//


#define ALPHA_DC (0.9996)
/**
 * @brief rtlsdr_callback called by rtlsdr driver. This function converts to float and removes DC offset
 * @param buf
 * @param len
 * @param ctx
 */
void rtlsdr_callback(unsigned char *buf, uint32_t len, void *ctx) {
    TYPECPX *samples ;
    TYPECPX tmp ;
    float I,Q ;


    struct t_rx_device* my_device = (struct t_rx_device*)ctx ;
    if( my_device->acq_stop == true ) {
        if( DEBUG_DRIVER ) fprintf(stderr,"%s(len=%d) my_device->acq_stop == true\n", __func__, len );
        fflush(stderr);
        return ;
    }

    int sample_count = len/2 ;
    samples = (TYPECPX *)malloc( sample_count * sizeof( TYPECPX ));
    if( samples == NULL ) {
        if( DEBUG_DRIVER ) fprintf(stderr,"%s(len=%d) samples == NULL\n", __func__, len );
        fflush(stderr);
        return ;
    }

    // convert samples from 8bits to float and remove DC component

    for( int i=0 ; i < sample_count ; i++ ) {
        int j = 2*i ;
        I =  ((int)buf[j  ] - 127)/ 127.0f   ;
        Q =  ((int)buf[j+1] - 127)/ 127.0f   ;
        // DC
        // y[n] = x[n] - x[n-1] + alpha * y[n-1]
        // see http://peabody.sapp.org/class/dmp2/lab/dcblock/
        tmp.re = I - my_device->xn_1.re + ALPHA_DC * my_device->yn_1.re ;
        tmp.im = Q - my_device->xn_1.im + ALPHA_DC * my_device->yn_1.im ;

        my_device->xn_1.re = I ;
        my_device->xn_1.im = Q ;

        my_device->yn_1.re = tmp.re ;
        my_device->yn_1.im = tmp.im ;

        samples[i] = tmp ;
    }
    // push samples to SDRNode callback function
    // we only manage one channel per device
    if( (*acqCbFunction)( my_device->uuid,
                          (float *)samples, sample_count, 1,
                          &my_device->context) <= 0 ) {
        free(samples);
    }
}

/**
 * @brief acquisition_thread This function is locked by the mutex and waits before starting the acquisition in asynch mode
 * @param params
 * @return
 */
void* acquisition_thread( void *params ) {
    struct t_rx_device* my_device = (struct t_rx_device*)params ;
    rtlsdr_dev_t *rtlsdr_device = my_device->rtlsdr_device ;
    if( DEBUG_DRIVER ) fprintf(stderr,"%s() start thread\n", __func__ );
    for( ; ; ) {
        my_device->running = false ;
        if( DEBUG_DRIVER ) fprintf(stderr,"%s() thread waiting\n", __func__ );
        fflush(stderr);
        sem_wait( &my_device->mutex );
        if( DEBUG_DRIVER ) fprintf(stderr,"%s() rtlsdr_read_async\n", __func__ );
        rtlsdr_read_async(rtlsdr_device, rtlsdr_callback, (void *)my_device, 0, 65536);
        my_device->running = true ;

    }
    return(NULL);
}

