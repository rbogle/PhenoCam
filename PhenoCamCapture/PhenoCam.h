#pragma once
#include "stdafx.h"
#include <Jai_Factory.h>


using namespace std;

#define NODE_NAME_WIDTH     "Width"
#define NODE_NAME_HEIGHT    "Height"
#define NODE_NAME_GAIN      "GainRaw"
#define NODE_NAME_PIXELFORMAT   "PixelFormat"
#define NODE_NAME_ACQSTART  "AcquisitionStart"
#define NODE_NAME_ACQSTOP   "AcquisitionStop"
#define CONFIG_FILE_NAME	"/PhenoCam.cfg"
#define CONFIG_KEY_OUTPATH	"PhenoCam.OutPath"

#define MAX_CAMERAS	2

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
	void Set_FilePath(string filepath);
	string Get_FilePath(void);
	void Capture(void);
	uint8_t Status(void);
	bool is_Ready(void);
	BOOL OpenFactoryAndCamera(void);
    void CloseFactoryAndCamera(void);  
	static string getCWD(void);

protected:
	
	FACTORY_HANDLE  m_hFactory;             // Factory Handle
	CAM_HANDLE      m_hCam[MAX_CAMERAS];    // Camera Handles
    VIEW_HANDLE     m_hView[MAX_CAMERAS];   // View Window handles
	THRD_HANDLE     m_hThread[MAX_CAMERAS]; // Stream handles
    int8_t          m_sCameraId[MAX_CAMERAS][J_CAMERA_ID_SIZE]; // Camera IDs
	uint8_t			m_CapStatus; //Capture Status Flag
	uint8_t			m_SysStatus; //system status codes
    uint8_t			m_CameraCount;    
	string			m_FilePath;
 
	void StreamCBFuncRGB(J_tIMAGE_INFO * pAqImageInfo);
	void StreamCBFuncNIR(J_tIMAGE_INFO * pAqImageInfo);
	BOOL SaveImageToFile(J_tIMAGE_INFO * ptRawImageInfo, string sFileName);
	
};
