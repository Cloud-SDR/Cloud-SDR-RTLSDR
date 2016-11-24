# Cloud-SDR-RTLSDR
Open source driver to add RTLSDR dongles to Cloud-SDR SDRNode. 

SDRNode can load custom driver files from shared library.
to implement you own driver, you have to code all the functions defined in external_hardware_def.h and then update your customhardware.js file.

Example :
```javascript
// Load RTLSDR shared library
if( SDRNode.loadDriver('CloudSDR_RTLSDR','') ) {
	print('RTLSDR Driver loaded');
}
```

The key steps are :
* when the javascript call is handled, SDRNode calls *initLibrary()*.
* then SDRNode calls *getBoardCount()*
* SDRNode assumes this first call, if *getBoardCount()* >= 1, that we have one hardware device of this type
* SDRNode loops from 0 to *getBoardCount()-1* to retrieve parameters for other devices, BUT DOES NOT CALL initLibrary()


# Building
Using Qt Creator just open the .pro file and compile (release). The binary file will be copied to \SDRNode\addons subfolder.
# windows
Place the librtlsdr.dll into the SDRNode main folder (next to other DLLs)
# Linux
Make sure the RTLSDR packages are installed, make sure the RTLSDR modules are blacklist.

Custom drivers can be loaded at any time by scripting. 
Check http://wiki.cloud-sdr.com/doku.php?id=documentation for more details.
