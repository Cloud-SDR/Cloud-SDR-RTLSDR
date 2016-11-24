/*---------------------------------------------------------*/
/* customhardware.js : load our custom drivers to SDRNode  */
/*---------------------------------------------------------*/

// Load RTLSDR shared library
if( SDRNode.loadDriver('CloudSDR_RTLSDR','') ) {
	print('RTLSDR Driver loaded');
}