#ifndef EXTERNAL_HARDWARE_DEF_H
#define EXTERNAL_HARDWARE_DEF_H

#include <stdint.h>
#include <sys/types.h>

// global for all devices
typedef int   (__stdcall _initLibrary)();
typedef int   (__stdcall _getBoardCount)();
typedef char* (__stdcall _getHardwareName)();

typedef int   (__stdcall _getPossibleSampleRateCount)();
typedef unsigned int   (__stdcall _getPossibleSampleRateValue)(int);
typedef unsigned int   (__stdcall _getPrefferedSampleRateValue)();
// Gain management
typedef int    (__stdcall  _getRxGainStageCount)();
typedef const char* (__stdcall _getRxGainStageName)( int );
typedef const char* (__stdcall _getRxGainStageUnitName)( int );
typedef int     (__stdcall _getRxGainStageType)(int);
typedef float  (__stdcall _getMinGainValue)(int);
typedef float  (__stdcall _getMaxGainValue)(int);
typedef int    (__stdcall _getGainDiscreteValuesCount)(int);
typedef float  (__stdcall _getGainDiscreteValue)(int, int);
typedef uint64_t (__stdcall _getMin_HWRx_CenterFreq)();
typedef uint64_t (__stdcall _getMax_HWRx_CenterFreq)();

// device specific
//int device id, float[], int channel_count, int sample_count

typedef int   (_stdcall  _pushSamples)(int, float *, int, int);

typedef const char* (__stdcall _getSerialNumber)( int );
typedef int   (__stdcall _prepareRXEngine)(int);
typedef int   (__stdcall _finalizeRXEngine)(int);
typedef int   (__stdcall _setRxSampleRate)(int, int);
typedef int   (__stdcall _getActualRxSampleRate)(int);


typedef int   (__stdcall _setRxCenterFreq)(int,int64_t);
typedef uint64_t (__stdcall _getRxCenterFreq)(int);


typedef int    (__stdcall _setRxGain)(int,int,float); // device, stage, value
typedef float  (__stdcall _getRxGainValue)(int);
typedef bool   (__stdcall _setAutoGainMode)(int);


#endif // EXTERNAL_HARDWARE_DEF_H
