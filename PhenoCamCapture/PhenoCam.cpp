#include "stdafx.h"
#include "PhenoCam.h"
#include "ConfigFile.h"
#include <windows.h>
#include <direct.h>
#include <time.h>

using namespace std;
using namespace log4cxx;

LoggerPtr PhenoCam::logger(Logger::getLogger("PhenoCam"));

PhenoCam::PhenoCam()
{	

    m_hFactory = NULL;

    for (int i=0; i< MAX_CAMERAS; i++)
    {
        m_hCam[i] = NULL;
        m_hView[i] = NULL;
        m_hThread[i] = NULL;
    }
	m_SysStatus = CLOSED;
	m_CapStatus = READY;
    m_CameraCount = 0;
	
	string cwd = getCWD();
	string cfgfile = cwd+string(CONFIG_FILE_NAME);
	ConfigFile cfg(cfgfile);
	//get or set path for writing images 
	if(cfg.keyExists(CONFIG_KEY_OUTPATH)){
		m_FilePath = cfg.getValueOfKey<std::string>(CONFIG_KEY_OUTPATH);
		LOG4CXX_DEBUG(logger,"Read from cfg file -- key: "<< CONFIG_KEY_OUTPATH <<" value: "<< m_FilePath.c_str());
	}else{
		m_FilePath = getCWD();
		LOG4CXX_DEBUG(logger,"Set m_filepath to cwd as: "<<m_FilePath.c_str());
	}

	BOOL retval;

    // Open factory & camera
    retval = OpenFactoryAndCamera();
	if(retval){
		if (m_hCam[0]){
			LOG4CXX_INFO(logger, "RGB Camera Opened.");
			m_SysStatus = m_SysStatus ^ RGB_FAIL; // clear the flag with xor
		}
		else{
            LOG4CXX_ERROR(logger,"No RGB camera found");
			m_SysStatus = m_SysStatus | RGB_FAIL; // set the flag with or
		}
		if (m_hCam[1]){
            LOG4CXX_INFO(logger, "NIR Camera Opened.");
			m_SysStatus = m_SysStatus ^ NIR_FAIL; // clear the flag with xor
		}else{
            LOG4CXX_ERROR(logger,"No NIR camera found");
			m_SysStatus = m_SysStatus | NIR_FAIL; // set the flag with or
		}
	}else{
		LOG4CXX_ERROR(logger,"Camera discovery and initalization failed!");
    }
}

PhenoCam::~PhenoCam(void)
{
	// Close factory & camera
    CloseFactoryAndCamera();
}

//--------------------------------------------------------------------------------------------------
// OpenFactoryAndCamera
//--------------------------------------------------------------------------------------------------
BOOL PhenoCam::OpenFactoryAndCamera()
{
	J_STATUS_TYPE   retval;
    uint32_t        iSize;
    uint32_t        iNumDev;
    bool8_t         bHasChange;

    // Open factory
    retval = J_Factory_Open("" , &m_hFactory);
    if (retval != J_ST_SUCCESS)
    {
        LOG4CXX_ERROR(logger,"Could not open factory!");
        return FALSE;
    }
    LOG4CXX_DEBUG(logger,"Opening factory succeeded");

    // Update camera list
    retval = J_Factory_UpdateCameraList(m_hFactory, &bHasChange);
    if (retval != J_ST_SUCCESS)
    {
		LOG4CXX_ERROR(logger,"Could not update camera list!");
        return FALSE;
    }
    LOG4CXX_DEBUG(logger,"Updating camera list succeeded");

    // Get the number of Cameras
    retval = J_Factory_GetNumOfCameras(m_hFactory, &iNumDev);
    if (retval != J_ST_SUCCESS)
    {
		LOG4CXX_ERROR(logger,"Could not get the number of cameras!");
        return FALSE;
    }
    if (iNumDev == 0)
    {
        LOG4CXX_ERROR(logger,"There is no camera!");
        return FALSE;
    }
	LOG4CXX_INFO(logger,"Number of cameras found: " << iNumDev);

    // We only want to get MAX_CAMERAS cameras connected at a time
    // and we assume that iNumDev is the actual camera count*2 because we assume
    // that we have 2 drivers active (SockerDriver+FilerDriver)

	bool bFdUse = false;
	int	iValidCamera = 0;

    for (int i=0; i< (int)iNumDev; i++)
    {
        // Get camera IDs
        iSize = J_CAMERA_ID_SIZE;
		m_sCameraId[iValidCamera][0] = 0;
        retval = J_Factory_GetCameraIDByIndex(m_hFactory, i, m_sCameraId[iValidCamera], &iSize);
        if (retval != J_ST_SUCCESS)
        {
            LOG4CXX_ERROR(logger,"Could not get the camera ID!");
            return FALSE;
        }
        LOG4CXX_DEBUG(logger,"Camera ID[" << i << "]: " << m_sCameraId[iValidCamera]);

		if(0 == strncmp("TL=>GevTL , INT=>FD", m_sCameraId[iValidCamera], 19))
		{ // FD
			bFdUse = true;
			// Open camera
			retval = J_Camera_Open(m_hFactory, m_sCameraId[iValidCamera], &m_hCam[iValidCamera]);
			if (retval != J_ST_SUCCESS)
			{
				LOG4CXX_ERROR(logger,"Could not open the camera!");
				return FALSE;
			}
			iValidCamera++;
			LOG4CXX_DEBUG(logger,"Opening camera: "<<iValidCamera<<" with file driver succeeded");
		}
		else
		{ // SD
			if(bFdUse == false)
			{
				// Open camera
				retval = J_Camera_Open(m_hFactory, m_sCameraId[iValidCamera], &m_hCam[iValidCamera]);
				if (retval != J_ST_SUCCESS)
				{
					LOG4CXX_ERROR(logger,"Could not open the camera!");
					return FALSE;
				}
				iValidCamera++;
				LOG4CXX_DEBUG(logger,"Opening camera: "<<iValidCamera<<" with standard driver succeeded");
			}
		}
		if(iValidCamera >= MAX_CAMERAS)
			break;
    }
    m_CameraCount = min(iValidCamera, MAX_CAMERAS);

	return TRUE;
}


//--------------------------------------------------------------------------------------------------
// CloseFactoryAndCamera
//--------------------------------------------------------------------------------------------------
void PhenoCam::CloseFactoryAndCamera()
{
    for (int i=0; i<MAX_CAMERAS;i++)
    {
        if (m_hCam[i])
        {
            // Close camera
            J_Camera_Close(m_hCam[i]);
            m_hCam[i] = NULL;
			LOG4CXX_INFO(logger,"Closed camera: "<<i);
			m_CameraCount--;
        }
    }

    if (m_hFactory)
    {
        // Close factory
        J_Factory_Close(m_hFactory);
        m_hFactory = NULL;
        LOG4CXX_INFO(logger,"Closed factory");
    }
}
//--------------------------------------------------------------------------------------------------
// StreamCBFuncRGB
//--------------------------------------------------------------------------------------------------
void PhenoCam::StreamCBFuncRGB(J_tIMAGE_INFO * pAqImageInfo)
{
	stringstream ts;
	J_STATUS_TYPE  retval;
    string filename;	
	time_t currtime;
	LOG4CXX_DEBUG(logger,"Entering StreamCBFuncRGB. CapStatus is: "<<(int) m_CapStatus);

	
	//send command to stop aquiring
    if (m_hCam[0])
    {
        // Stop Acquision
        if (m_hCam[0]) {
            retval = J_Camera_ExecuteCommand(m_hCam[0], NODE_NAME_ACQSTOP);
			LOG4CXX_DEBUG(logger,"StreamCBFuncRGB Stopped RGB Aquisition"); 
        }
    }
	
	// Update timestamp create filename
	time(&currtime);
	ts<<currtime;
	//ts << pAqImageInfo->iTimeStamp;
	filename = m_FilePath + "/RGB_" + ts.str()+".tif";
	//convert and write file
	SaveImageToFile(pAqImageInfo,filename);
	LOG4CXX_INFO(logger,"StreamCBFuncRGB Wrote file: "<<filename.c_str());

	//unset rgb_busy flag
	if(m_CapStatus==RGB_BUSY || m_CapStatus==AQUIRING){
		m_CapStatus = m_CapStatus^RGB_BUSY;
	}
	LOG4CXX_DEBUG(logger,"StreamCBFuncRGB CapStatus is: "<<(int)m_CapStatus);

    if(m_hThread[0])
    {
        // close stream
		LOG4CXX_DEBUG(logger,"StreamCBFuncRGB closed RGB stream");
		LOG4CXX_DEBUG(logger,"Exitiing StreamCBFuncRGB...");
        retval = J_Image_CloseStream(m_hThread[0]);
        m_hThread[0] = NULL;
        
    }
}

//--------------------------------------------------------------------------------------------------
// StreamCBFuncNIR
//--------------------------------------------------------------------------------------------------
void PhenoCam::StreamCBFuncNIR(J_tIMAGE_INFO * pAqImageInfo)
{
	
	stringstream ts;
	J_STATUS_TYPE retval;
    string filename;
	time_t currtime;
	LOG4CXX_DEBUG(logger,"Entering StreamCBFuncNIR. CapStatus is: "<<(int) m_CapStatus);
;
	//send command to stop aquiring
	//stop stream for nir
    if (m_hCam[1]){
        // Stop Acquision
        if (m_hCam[1]) {
            retval = J_Camera_ExecuteCommand(m_hCam[1], NODE_NAME_ACQSTOP);
			LOG4CXX_DEBUG(logger,"StreamCBFuncNIR Stopped NIR Aquisition");
        }
    }
	// Update timestamp create filename
	time(&currtime);
	ts<<currtime;
	//ts << pAqImageInfo->iTimeStamp;
	filename = m_FilePath + "/NIR_" + ts.str() + ".tif";
	//convert and write file
	SaveImageToFile(pAqImageInfo,filename);
	LOG4CXX_INFO(logger,"StreamCBFuncNIR Wrote file: "<<filename.c_str());

	//unset nir_busy flag
	if(m_CapStatus==NIR_BUSY || m_CapStatus==AQUIRING){
		m_CapStatus = m_CapStatus^NIR_BUSY;
	}
	LOG4CXX_DEBUG(logger,"StreamCBFuncNIR CapStatus is: "<<(int) m_CapStatus)

	if(m_hThread[1])
    {
        // close stream
		LOG4CXX_DEBUG(logger,"StreamCBFuncNIR closed NIR stream.");
		LOG4CXX_DEBUG(logger,"Exiting StreamCBFuncNIR...");
		//LOG4CXX_INFO(logger,log.rdbuf());
        retval = J_Image_CloseStream(m_hThread[1]);
        m_hThread[1] = NULL;
        
    }
}

//--------------------------------------------------------------------------------------------------
// SaveImageToFile
//--------------------------------------------------------------------------------------------------

BOOL PhenoCam::SaveImageToFile(J_tIMAGE_INFO* ptRawImageInfo, string sFileName)
{
	J_tIMAGE_INFO tCnvImageInfo; // Image info structure
	// Allocate the buffer to hold converted the image
	if (J_Image_Malloc(ptRawImageInfo, &tCnvImageInfo) != J_ST_SUCCESS)
		return FALSE;
	// Convert the raw image to image format
	if (J_Image_FromRawToImage(ptRawImageInfo, &tCnvImageInfo) != J_ST_SUCCESS)
		return FALSE;
	// Save the image to disk in TIFF format
	LPCSTR sFNptr = sFileName.c_str(); //get const char*
	if (J_Image_SaveFileA(&tCnvImageInfo, sFNptr) != J_ST_SUCCESS)
		return FALSE;
	// Free up the image buffer
	if (J_Image_Free(&tCnvImageInfo) != J_ST_SUCCESS)
		return FALSE;
	return TRUE;
}

//--------------------------------------------------------------------------------------------------
// Capture
//--------------------------------------------------------------------------------------------------
void PhenoCam::Capture(){

    J_STATUS_TYPE   retval;
    int64_t int64Val;
    int64_t pixelFormat;
    int     bpp = 0;
    SIZE	ViewSize;
	
	//init streams & send aquire cmd
    for (int i=0; i<MAX_CAMERAS;i++)
    {	
        if (m_hCam[i])
        {
            // Get Width from the camera
            retval = J_Camera_GetValueInt64(m_hCam[i], NODE_NAME_WIDTH, &int64Val);
            ViewSize.cx = (LONG)int64Val;     // Set window size cx

            // Get Height from the camera
            retval = J_Camera_GetValueInt64(m_hCam[i], NODE_NAME_HEIGHT, &int64Val);
            ViewSize.cy = (LONG)int64Val;     // Set window size cy

            // Get pixelformat from the camera
            retval = J_Camera_GetValueInt64(m_hCam[i], NODE_NAME_PIXELFORMAT, &int64Val);
            pixelFormat = int64Val;

            // Calculate number of bits (not bytes) per pixel using macro
            bpp = J_BitsPerPixel(pixelFormat);

            // Open stream
            if (i==0)
            {
                retval = J_Image_OpenStream(m_hCam[i], 0, reinterpret_cast<J_IMG_CALLBACK_OBJECT>(this), reinterpret_cast<J_IMG_CALLBACK_FUNCTION>(&PhenoCam::StreamCBFuncRGB), &m_hThread[i], (ViewSize.cx*ViewSize.cy*bpp)/8);
                if (retval != J_ST_SUCCESS) {
                    LOG4CXX_ERROR(logger,"Could not open RGB stream!");
					m_CapStatus = m_CapStatus ^ RGB_BUSY; //clear flag we failed to open stream
                    break;
                }
				m_CapStatus = m_CapStatus | RGB_BUSY; //set busy flag
				LOG4CXX_INFO(logger,"Opening RGB stream succeeded. CapStatus is: "<<(int) m_CapStatus);
            }
            else if (i==1)
            {
                retval = J_Image_OpenStream(m_hCam[i], 0, reinterpret_cast<J_IMG_CALLBACK_OBJECT>(this), reinterpret_cast<J_IMG_CALLBACK_FUNCTION>(&PhenoCam::StreamCBFuncNIR), &m_hThread[i], (ViewSize.cx*ViewSize.cy*bpp)/8);
                if (retval != J_ST_SUCCESS) {
                    LOG4CXX_ERROR(logger,"Could not open NIR stream!");
					m_CapStatus = m_CapStatus ^ NIR_BUSY; //clear flag we failed to open stream
                    break;
                }
				m_CapStatus = m_CapStatus | NIR_BUSY; //set busy flag
                LOG4CXX_INFO(logger,"Opening NIR stream succeeded. CapStatus is: "<< (int) m_CapStatus);
            }
            // Start Acquision
            retval = J_Camera_ExecuteCommand(m_hCam[i], NODE_NAME_ACQSTART);
			if (retval != J_ST_SUCCESS) {
				LOG4CXX_ERROR(logger,"Sending cmd "<<NODE_NAME_ACQSTART<<" failed for Sensor: "<<i);
			}else{
				LOG4CXX_INFO(logger,"Sending cmd "<<NODE_NAME_ACQSTART<<" succeeded");
			}
		}
	
    }

	//wait if status active
	while(m_CapStatus>0){
		LOG4CXX_INFO(logger,"Waiting for Aquire to finish. CapStatus is: "<<(int)m_CapStatus);
		Sleep(100);
	}
	

}
//--------------------------------------------------------------------------------------------------
// Status
//--------------------------------------------------------------------------------------------------
uint8_t PhenoCam::Status(){
	return m_SysStatus;
}
//--------------------------------------------------------------------------------------------------
// is_Ready
//--------------------------------------------------------------------------------------------------
bool PhenoCam::is_Ready(){
	if (m_hCam[0]){
		LOG4CXX_INFO(logger,"RGB Camera: " << m_sCameraId[0] << " open!"); // Display camera ID
		m_SysStatus = m_SysStatus ^ RGB_FAIL; // clear the flag with xor
	}
	else{
        LOG4CXX_ERROR(logger,"No RGB camera found");
		m_SysStatus = m_SysStatus | RGB_FAIL; // set the flag with or
	}
	if (m_hCam[1]){
        LOG4CXX_INFO(logger,"NIR Camera: " << m_sCameraId[1] << " open !"); // Display camera ID
		m_SysStatus = m_SysStatus ^ NIR_FAIL; // clear the flag with xor
	}else{
        LOG4CXX_ERROR(logger,"No NIR camera found");
		m_SysStatus = m_SysStatus | NIR_FAIL; // set the flag with or
	}
	if(m_SysStatus)return false;
	else return true;
}

//--------------------------------------------------------------------------------------------------
// GetFilePath
//--------------------------------------------------------------------------------------------------
string PhenoCam::Get_FilePath(){
	return m_FilePath;
}
//--------------------------------------------------------------------------------------------------
// SetFilePath
//--------------------------------------------------------------------------------------------------
void PhenoCam::Set_FilePath(string filepath){
	m_FilePath = filepath;
}
//--------------------------------------------------------------------------------------------------
// getCWD
//--------------------------------------------------------------------------------------------------
 string PhenoCam::getCWD(){
	char cCurrentPath[FILENAME_MAX];
 	_getcwd(cCurrentPath, sizeof(cCurrentPath));
	string cwd(cCurrentPath);
	return cwd;
}
	