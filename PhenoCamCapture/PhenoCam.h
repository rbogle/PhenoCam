#pragma once
#include "stdafx.h"
#include <Jai_Factory.h>


using namespace std;

//Common GENICAM Node Names
#define NODE_NAME_WIDTH     "Width"
#define NODE_NAME_HEIGHT    "Height"
#define NODE_NAME_GAIN      "GainRaw"
#define NODE_NAME_PIXELFORMAT   "PixelFormat"
#define NODE_NAME_ACQSTART  "AcquisitionStart"
#define NODE_NAME_ACQSTOP   "AcquisitionStop"

//Config file definitions
#define CONFIG_FILE_NAME	"/PhenoCam.cfg"
#define CONFIG_KEY_OUTPATH	"PhenoCam.OutPath"

//hard defines 
#define MAX_CAMERAS	2
#define	NUM_OF_BUFFER 10

//Cap mode status codes
#define ONE_SHOT  0
#define SEQUENCE 1

//Status Codes for Flags
#define READY 0

#define RGB_BUSY 1
#define NIR_BUSY 2
#define AQUIRING 3

#define RGB_FAIL 1
#define NIR_FAIL 2
#define CLOSED 3


class PhenoCam{
public:
	PhenoCam(void);
	~PhenoCam(void);

	static log4cxx::LoggerPtr logger;
	void Set_ImageFilePath(string filepath);
	string Get_ImageFilePath(void);
	void Initialize(void);
	void Initialize(string confpath);

	void Capture(void);
	uint8_t Status(void);
	bool is_Ready(void);
 
	

protected:
	
	FACTORY_HANDLE  m_hFactory;             // Factory Handle
	CAM_HANDLE      m_hCam[MAX_CAMERAS];    // Camera Handles
	THRD_HANDLE     m_hThread[MAX_CAMERAS]; // Stream handles 
    int8_t          m_sCameraId[MAX_CAMERAS][J_CAMERA_ID_SIZE]; // Camera IDs
	uint8_t			m_iCapStatus; //Capture Status Flag
	uint8_t			m_iSysStatus; //system status codes
    uint8_t			m_iCameraCount;    
	string			m_sFilePath;
	uint8_t			m_iNIRFramesCapd;
	uint8_t			m_iRGBFramesCapd;
	uint8_t			m_iMaxFrames; // 1++
	uint8_t			m_iSysOpMode; // oneshot or sequence

	//Callback functions for image arrival
	void StreamCBFuncRGB(J_tIMAGE_INFO * pAqImageInfo);
	void StreamCBFuncNIR(J_tIMAGE_INFO * pAqImageInfo);
	//main functions to config camera capture sequences	
	void PrepareCamera(int iCameraIndex); //setup defaults for cameras
	void SetCaptureMode(int capmode); //config camera to oneshot or sequence
	void TriggerFrame(); //Sends the SoftwareTrigger to camera
	BOOL OpenFactoryAndCamera(void);
    void CloseFactoryAndCamera(void); 
	BOOL SaveImageToFile(J_tIMAGE_INFO * ptRawImageInfo, string sFileName);
};
