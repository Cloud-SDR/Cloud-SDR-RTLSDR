/* =====================================================================================
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
#ifndef ENTRYPOINT_H
#define ENTRYPOINT_H

#include <stdint.h>
#include <sys/types.h>

#ifdef _WIN64
#include <windows.h>
#define LIBRARY_API __stdcall __declspec(dllexport)
#else
 #define LIBRARY_API
#endif



#define RC_OK (1)
#define RC_NOK (0)

/*
 *  For more details on the following functions, please look at http://wiki.cloud-sdr.com/doku.php?id=documentation
 */

struct ext_Context {
    long ctx_version ;
    int64_t center_freq;
    unsigned int sample_rate;
};

// call this function to log something into the SDRNode central log file
// call is log( UUID, severity, msg)
typedef int   (LIBRARY_API _tlogFun)(char *, int, char *);

// call this function to push samples to the SDRNode
// call is pushSamples( UUID, ptr to float array of samples, sample count, channel count )
typedef int   (LIBRARY_API  _pushSamplesFun)( char *, float *, int, int, struct ext_Context*);

// driver instance specific functions
// will be called with device index in the range [0..getBoardCount()[

extern "C" {

    #ifdef _WIN64
    BOOL WINAPI DllMain( HINSTANCE hInstance, DWORD dwReason, LPVOID *lpvReserved );
    #endif


    LIBRARY_API int initLibrary(char *json_init_params, _tlogFun* ptr, _pushSamplesFun *acqCb );
    LIBRARY_API int getBoardCount();


    LIBRARY_API int setBoardUUID( int device_id, char *uuid );

    LIBRARY_API char *getHardwareName(int device_id);


    // manage sample rates
    LIBRARY_API int getPossibleSampleRateCount(int device_id) ;
    LIBRARY_API unsigned int getPossibleSampleRateValue(int device_id, int index);
    LIBRARY_API unsigned int getPrefferedSampleRateValue(int device_id);

    //manage min/max freqs
    LIBRARY_API int64_t getMin_HWRx_CenterFreq(int device_id);
    LIBRARY_API int64_t getMax_HWRx_CenterFreq(int device_id);

    // discover gain stages and settings
    LIBRARY_API int getRxGainStageCount(int device_id) ;
    LIBRARY_API char* getRxGainStageName( int device_id, int stage);
    LIBRARY_API char* getRxGainStageUnitName( int device_id,int stage);
    LIBRARY_API int getRxGainStageType( int device_id,int stage);
    LIBRARY_API float getMinGainValue(int device_id,int stage);
    LIBRARY_API float getMaxGainValue(int device_id,int stage);
    LIBRARY_API int getGainDiscreteValuesCount( int device_id,int stage );
    LIBRARY_API float getGainDiscreteValue( int device_id,int stage, int index ) ;

    // driver instance specific functions
    // will be called with device index in the range [0..getBoardCount()[
    LIBRARY_API char* getSerialNumber( int device_id );

    LIBRARY_API int prepareRXEngine( int device_id );
    LIBRARY_API int finalizeRXEngine( int device_id );

    LIBRARY_API int setRxSampleRate( int device_id , int sample_rate);
    LIBRARY_API int getActualRxSampleRate( int device_id );

    LIBRARY_API int setRxCenterFreq( int device_id , int64_t freq_hz );
    LIBRARY_API int64_t getRxCenterFreq( int device_id );

    LIBRARY_API int setRxGain( int device_id, int stage_id, float gain_value );
    LIBRARY_API float getRxGainValue( int device_id , int stage_id );
    LIBRARY_API bool setAutoGainMode( int device_id );
}

#endif // ENTRYPOINT_H
