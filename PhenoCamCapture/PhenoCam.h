#pragma once
#include "stdafx.h"
#include <Jai_Factory.h>
#include "ConfigFile.h"

using namespace std;

//Common GENICAM Node Names
#define NODE_NAME_WIDTH     "Width"
#define NODE_NAME_HEIGHT    "Height"
#define NODE_NAME_GAIN      "GainRaw"
#define NODE_NAME_PIXELFORMAT   "PixelFormat"
#define NODE_NAME_OFFSETX	"OffsetX"
#define NODE_NAME_OFFSETY	"OffsetY"
//Continuous aquisition command
#define NODE_NAME_ACQSTART  "AcquisitionStart"
#define NODE_NAME_ACQSTOP   "AcquisitionStop"
//Trigger Mode
#define NODE_NAME_EXPMODE	"ExposureMode"
#define NODE_NAME_SEQEXPMODE	"SequentialEPSTrigger"
#define NODE_NAME_ONEEXPMODE	"EdgePreSelect"
#define NODE_NAME_CONTEXPMODE	"Continuous"
//Trigger source
#define NODE_NAME_TRIGSRC	"TriggerSource"
#define NODE_NAME_TRIGGER	"UserOutput0"
//Shutter control
#define NODE_NAME_SHUTTERMODE	"ShutterMode"
#define NODE_NAME_PROGEXP	"ProgrammableExposure" //value for ShutterMode
#define NODE_NAME_EXPOSURE	"ExposureTimeRaw"

//Config file definitions
#define CONFIG_FILE_NAME	"/PhenoCam.cfg"
#define CONFIG_KEY_OUTPATH	"PhenoCam.OutPath"
#define CONFIG_SYSOPMODE	"PhenoCam.SysOpMode"
#define CONFIG_KEY_SYNCMODE "PhenoCam.SyncMode"
#define CONFIG_KEY_NIRPXFMT "PhenoCam.NIR.PixelFormat"
#define CONFIG_KEY_RGBPXFMT	"PHenoCam.RGB.PixelFormat"

//hard defines 
#define MAX_CAMERAS	2
#define MAX_FRAMES 10
#define	NUM_OF_BUFFER 10

//Cap mode status codes
#define ONE_SHOT  1
#define SEQUENCE 2
#define CONTINUOUS 3

//Status Codes for Flags
#define READY 0

#define RGB_BUSY 1
#define NIR_BUSY 2
#define AQUIRING 3

#define RGB_FAIL 1
#define NIR_FAIL 2
#define CLOSED 3

class Sequence{
public:
	Sequence(){
		id=0; gain=0; shutterL=792; width=0; height=0; offsetx=0; offsety=0;
	}
	uint8_t id;
	int16_t gain;// -89 to 593
	uint16_t shutterL; //0-792(20us-33,333us)
	uint16_t width;//for setting ROI
	uint16_t height;//for setting ROI
	uint16_t offsetx;//for setting ROI
	uint16_t offsety;//for setting ROI
};

class PhenoCam{
public:
	PhenoCam(void);
	~PhenoCam(void);

	static log4cxx::LoggerPtr logger;
	void Set_ImageFilePath(string filepath);
	string Get_ImageFilePath(void);
	bool Open(string confpath); //always call to start.
	void Capture(void); //to be used for one-shot or sequence
	void StopAcquire(void); //to be called for continuous live view
	void StartAcquire(void); // to be called to close live view
	void Close(void); //always called when finished
	uint8_t Status(void);
	bool is_Ready(void);

protected:
	
	FACTORY_HANDLE  m_hFactory;             // Factory Handle
	CAM_HANDLE      m_hCam[MAX_CAMERAS];    // Camera Handles
	THRD_HANDLE     m_hThread[MAX_CAMERAS]; // Stream handles 
	VIEW_HANDLE     m_hView[MAX_CAMERAS];             // View window handles
	ConfigFile*		m_cCfg; //ConfigFile Obj
    int8_t          m_sCameraId[MAX_CAMERAS][J_CAMERA_ID_SIZE]; // Camera IDs
	uint8_t			m_iCapStatus; //Capture Status Flag
	uint8_t			m_iSysStatus; //system status codes
    uint8_t			m_iCameraCount;    
	string			m_sFilePath;
	uint8_t			m_iNIRFramesCapd;
	uint8_t			m_iRGBFramesCapd;
	uint8_t			m_iMaxFrames; // 1++
	uint8_t			m_iSysOpMode; // oneshot, sequence, continuous
	vector<Sequence>	m_vSequences; //use to hold sequence info for loading to camera.

	//Callback functions for image arrival
	void StreamCBFuncRGB(J_tIMAGE_INFO * pAqImageInfo);
	void StreamCBFuncNIR(J_tIMAGE_INFO * pAqImageInfo);
	//main functions to config camera capture sequences	
	void SetCaptureMode(uint8_t capmode); //config camera to oneshot,sequence, or continuous
	void TriggerFrame(uint8_t cam); //Sends the SoftwareTrigger to camera
	void Configure(string cfgfile); //read config file and set parameters
	void ParseConfigMode(); //parse read config file for capture mode setting
	void OpenFactoryAndCamera(void);
	void SetupSequenceMode(void);
	void SetupContinuousMode(void);
	void SetupOneShotMode(void);
	bool SaveImageToFile(J_tIMAGE_INFO * ptRawImageInfo, string sFileName);
};
