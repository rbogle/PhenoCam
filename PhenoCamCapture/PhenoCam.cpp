#include "stdafx.h"
#include "PhenoCam.h"
#include "PhenoCamException.h"
#include "Utility.h"
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
        m_hThread[i] = NULL;
    }
	m_iSysStatus = CLOSED;
	m_iCapStatus = READY;
    m_iCameraCount = 0;
	m_sFilePath = "";
	m_iNIRFramesCapd=0;
	m_iRGBFramesCapd=0;
	m_iMaxFrames=1;
	m_iSysOpMode=ONE_SHOT;
	m_cCfg = NULL;
}


PhenoCam::~PhenoCam(void)
{
	// Close factory & camera
	this->Close();
	delete m_cCfg;
	delete &m_vSequences;
}

//--------------------------------------------------------------------------------------------------
// OpenFactoryAndCamera
//--------------------------------------------------------------------------------------------------
void PhenoCam::OpenFactoryAndCamera()
{
	J_STATUS_TYPE   retval;
    uint32_t        iSize;
    uint32_t        iNumDev;
    bool8_t         bHasChange;
	string			err;
    // Open factory
    retval = J_Factory_Open("" , &m_hFactory);
    if (retval != J_ST_SUCCESS)
    {
		err = "Could not open factory!";
		LOG4CXX_ERROR(logger,err.c_str());
        throw PhenoCamException(err,PCEXCEPTION_FACTORYFAIL);
    }
    LOG4CXX_DEBUG(logger,"Opening factory succeeded");

    // Update camera list
    retval = J_Factory_UpdateCameraList(m_hFactory, &bHasChange);
    if (retval != J_ST_SUCCESS)
    {
		err ="Could not update camera list!";
        LOG4CXX_ERROR(logger,err.c_str());
        throw PhenoCamException(err, PCEXCEPTION_UPDTCAMLIST);
    }
    LOG4CXX_DEBUG(logger,"Updating camera list succeeded");

    // Get the number of Cameras
    retval = J_Factory_GetNumOfCameras(m_hFactory, &iNumDev);
    if (retval != J_ST_SUCCESS)
    {
		err="Could not get the number of cameras!";
        LOG4CXX_ERROR(logger,err.c_str());
        throw PhenoCamException(err, PCEXCEPTION_NUMCAMSFAIL);
    }
    if (iNumDev == 0)
    {
        err="There is no camera!";
        LOG4CXX_ERROR(logger,err.c_str());
        throw PhenoCamException(err, PCEXCEPTION_NUMCAMSFAIL);
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
			err = "Could not get the camera ID for cam";
            LOG4CXX_ERROR(logger,err.c_str());
			throw PhenoCamException(err, PCEXCEPTION_CAMIDFAIL);
        }
        LOG4CXX_DEBUG(logger,"Camera ID[" << i << "]: " << m_sCameraId[iValidCamera]);

		if(0 == strncmp("TL=>GevTL , INT=>FD", m_sCameraId[iValidCamera], 19))
		{ // FD
			bFdUse = true;
			// Open camera
			retval = J_Camera_Open(m_hFactory, m_sCameraId[iValidCamera], &m_hCam[iValidCamera]);
			if (retval != J_ST_SUCCESS)
			{
				err ="Could not open the camera!";
				LOG4CXX_ERROR(logger,err.c_str());
				throw PhenoCamException(err, i+2);
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
					err ="Could not open the camera!";
					LOG4CXX_ERROR(logger,err.c_str());
					throw PhenoCamException(err, i+2);
				}
				iValidCamera++;
				LOG4CXX_DEBUG(logger,"Opening camera: "<<iValidCamera<<" with standard driver succeeded");
			}
		}
		if(iValidCamera >= MAX_CAMERAS)
			break;
    }
    m_iCameraCount = min(iValidCamera, MAX_CAMERAS);
	m_iSysStatus = READY;
}


//--------------------------------------------------------------------------------------------------
// CloseFactoryAndCamera
//--------------------------------------------------------------------------------------------------
void PhenoCam::Close()
{
    for (int i=0; i<MAX_CAMERAS;i++)
    {
        if (m_hCam[i])
        {
            // Close camera
            J_Camera_Close(m_hCam[i]);
            m_hCam[i] = NULL;
			LOG4CXX_INFO(logger,"Closed camera: "<<i);
			m_iCameraCount--;
        }
    }

    if (m_hFactory)
    {
        // Close factory
        J_Factory_Close(m_hFactory);
        m_hFactory = NULL;
        LOG4CXX_INFO(logger,"Closed factory");
    }
	m_iSysStatus = CLOSED;
}
//--------------------------------------------------------------------------------------------------
// StreamCBFuncRGB
//--------------------------------------------------------------------------------------------------
void PhenoCam::StreamCBFuncRGB(J_tIMAGE_INFO * pAqImageInfo)
{
	stringstream ts;
	J_STATUS_TYPE retval;
    string filename;
	time_t currtime;
	LOG4CXX_DEBUG(logger,"Entering StreamCBFuncRGB. CapStatus is: "<<(int) m_iCapStatus);
	LOG4CXX_DEBUG(logger,"StreamCBFuncRGB. CapMode is: "<<(int) this->m_iSysOpMode);
	if(this->m_iSysOpMode != CONTINUOUS){
		//new frame captured
		this->m_iRGBFramesCapd++;
		//save it
		// Update timestamp create filename with time and frame info
		time(&currtime);
		ts<<currtime;
		ts<<"_"<<this->m_iRGBFramesCapd;
		filename = m_sFilePath + "/RGB_" + ts.str() + ".tif";
		//convert and write file
		SaveImageToFile(pAqImageInfo,filename);
		LOG4CXX_INFO(logger,"StreamCBFuncRGB Wrote file: "<<filename.c_str());
		//Time to close out or retrigger? 
		if(this->m_iRGBFramesCapd < this->m_iMaxFrames){ 
			this->TriggerFrame(0);
		}else{//We're done! close it up
			//unset rgb_busy flag
			if(m_iCapStatus==RGB_BUSY || m_iCapStatus==AQUIRING){
				m_iCapStatus = m_iCapStatus^RGB_BUSY;
			}
			LOG4CXX_DEBUG(logger,"StreamCBFuncRGB CapStatus is: "<<(int) m_iCapStatus)
			if(m_hThread[0])
			{
				// close stream
				LOG4CXX_DEBUG(logger,"StreamCBFuncRGB closed RGB stream.");
				LOG4CXX_DEBUG(logger,"Exiting StreamCBFuncRGB...");
				//LOG4CXX_INFO(logger,log.rdbuf());
				retval = J_Image_CloseStream(m_hThread[0]);
				m_hThread[0] = NULL;
		        
			}	
		}
	}
	//TODO: Add else for live view code here for Continuous Mode
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
	LOG4CXX_DEBUG(logger,"Entering StreamCBFuncNIR. CapStatus is: "<<(int) m_iCapStatus);
	LOG4CXX_DEBUG(logger,"StreamCBFuncNIR. CapMode is: "<<(int) this->m_iSysOpMode);
	if(this->m_iSysOpMode != CONTINUOUS){
		//new frame captured
		this->m_iNIRFramesCapd++;
		//save it
		// Update timestamp create filename
		time(&currtime);
		ts<<currtime;
		ts<<"_"<<this->m_iNIRFramesCapd;
		//ts << pAqImageInfo->iTimeStamp;
		filename = m_sFilePath + "/NIR_" + ts.str() + ".tif";
		//convert and write file
		SaveImageToFile(pAqImageInfo,filename);
		LOG4CXX_INFO(logger,"StreamCBFuncNIR Wrote file: "<<filename.c_str());
		//Time to close out or retrigger? Retrigger is done by RGB thread because we are set to sync
		if(this->m_iNIRFramesCapd >= this->m_iMaxFrames){ //We're done! close it up
		
			//unset nir_busy flag
			if(m_iCapStatus==NIR_BUSY || m_iCapStatus==AQUIRING){
				m_iCapStatus = m_iCapStatus^NIR_BUSY;
			}
			LOG4CXX_DEBUG(logger,"StreamCBFuncNIR CapStatus is: "<<(int) m_iCapStatus)

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
	}
	//TODO: Add live view code here for Continuous Mode
}

/*///////////////////////////////////////////////////////////////////////////////////////////////////
/	Start should be called when we want to start the continuous acquire and open live view
///////////////////////////////////////////////////////////////////////////////////////////////////*/
void PhenoCam::StartAcquire(){
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
					m_iCapStatus = m_iCapStatus ^ RGB_BUSY; //clear flag we failed to open stream
                    break;
                }
				m_iCapStatus = m_iCapStatus | RGB_BUSY; //set busy flag
				LOG4CXX_INFO(logger,"Opening RGB stream succeeded. CapStatus is: "<<(int) m_iCapStatus);
            }
            else if (i==1)
            {
                retval = J_Image_OpenStream(m_hCam[i], 0, reinterpret_cast<J_IMG_CALLBACK_OBJECT>(this), reinterpret_cast<J_IMG_CALLBACK_FUNCTION>(&PhenoCam::StreamCBFuncNIR), &m_hThread[i], (ViewSize.cx*ViewSize.cy*bpp)/8);
                if (retval != J_ST_SUCCESS) {
                    LOG4CXX_ERROR(logger,"Could not open NIR stream!");
					m_iCapStatus = m_iCapStatus ^ NIR_BUSY; //clear flag we failed to open stream
                    break;
                }
				m_iCapStatus = m_iCapStatus | NIR_BUSY; //set busy flag
                LOG4CXX_INFO(logger,"Opening NIR stream succeeded. CapStatus is: "<< (int) m_iCapStatus);
            }	


//TODO add open view window code here.


			// Start Acquision
			if(this->m_iSysOpMode==CONTINUOUS){ //continuous mode is fired with AcquisitionStart
				retval = J_Camera_ExecuteCommand(m_hCam[i], NODE_NAME_ACQSTART);
				if (retval != J_ST_SUCCESS) {
					LOG4CXX_ERROR(logger,"Sending cmd "<<NODE_NAME_ACQSTART<<" failed for Sensor: "<<i);
				}else{
					LOG4CXX_INFO(logger,"Sending cmd "<<NODE_NAME_ACQSTART<<" succeeded");
				}

			}
		}
	}
}

/*///////////////////////////////////////////////////////////////////////////////////////////////////
/	Stop should be called when we want to stop the continuous aquire and close live view
///////////////////////////////////////////////////////////////////////////////////////////////////*/
void PhenoCam::StopAcquire(){

	J_STATUS_TYPE   retval;
	this->m_iCapStatus=READY;
	for(int i=0; i<MAX_CAMERAS; i++){
		if (m_hCam[i]){
			// Stop Acquision
			if (m_hCam[i]) {
				retval = J_Camera_ExecuteCommand(m_hCam[i], NODE_NAME_ACQSTOP);
				LOG4CXX_DEBUG(logger,"StreamCBFuncNIR Stopped NIR Aquisition");
			}
		}
		//TODO: Need to destroy view handle and close window.
		if(m_hThread[i])
		{
				// close stream
				LOG4CXX_DEBUG(logger,"StreamCBFuncNIR closed NIR stream.");
				LOG4CXX_DEBUG(logger,"Exiting StreamCBFuncNIR...");
				retval = J_Image_CloseStream(m_hThread[i]);
				m_hThread[i] = NULL;
		        
		}
	}
}

//--------------------------------------------------------------------------------------------------
// SaveImageToFile
//--------------------------------------------------------------------------------------------------

bool PhenoCam::SaveImageToFile(J_tIMAGE_INFO* ptRawImageInfo, string sFileName)
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
	
	this->m_iNIRFramesCapd=0;
	this->m_iRGBFramesCapd=0;
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
					m_iCapStatus = m_iCapStatus ^ RGB_BUSY; //clear flag we failed to open stream
                    break;
                }
				m_iCapStatus = m_iCapStatus | RGB_BUSY; //set busy flag
				LOG4CXX_INFO(logger,"Opening RGB stream succeeded. CapStatus is: "<<(int) m_iCapStatus);
            }
            else if (i==1)
            {
                retval = J_Image_OpenStream(m_hCam[i], 0, reinterpret_cast<J_IMG_CALLBACK_OBJECT>(this), reinterpret_cast<J_IMG_CALLBACK_FUNCTION>(&PhenoCam::StreamCBFuncNIR), &m_hThread[i], (ViewSize.cx*ViewSize.cy*bpp)/8);
                if (retval != J_ST_SUCCESS) {
                    LOG4CXX_ERROR(logger,"Could not open NIR stream!");
					m_iCapStatus = m_iCapStatus ^ NIR_BUSY; //clear flag we failed to open stream
                    break;
                }
				m_iCapStatus = m_iCapStatus | NIR_BUSY; //set busy flag
                LOG4CXX_INFO(logger,"Opening NIR stream succeeded. CapStatus is: "<< (int) m_iCapStatus);
            }
            
		}
	
    }

	//If we are not doing conintuous mode then toggle softwaretrigger0
	//we fire to Cam0 only since they are set to sync
	if(this->m_iSysOpMode!=CONTINUOUS) this->TriggerFrame(0); 
	//wait if status active
	while(m_iCapStatus>0){
		LOG4CXX_INFO(logger,"Waiting for Aquire to finish. CapStatus is: "<<(int)m_iCapStatus);
		Sleep(100);
	}
	

}
//--------------------------------------------------------------------------------------------------
// Status
//--------------------------------------------------------------------------------------------------
uint8_t PhenoCam::Status(){
	return m_iSysStatus;
}
//--------------------------------------------------------------------------------------------------
// is_Ready
//--------------------------------------------------------------------------------------------------
bool PhenoCam::is_Ready(){
	if (m_hCam[0]){
		LOG4CXX_INFO(logger,"RGB Camera: " << m_sCameraId[0] << " open!"); // Display camera ID
		m_iSysStatus = m_iSysStatus ^ RGB_FAIL; // clear the flag with xor
	}
	else{
        LOG4CXX_ERROR(logger,"No RGB camera found");
		m_iSysStatus = m_iSysStatus | RGB_FAIL; // set the flag with or
	}
	if (m_hCam[1]){
        LOG4CXX_INFO(logger,"NIR Camera: " << m_sCameraId[1] << " open !"); // Display camera ID
		m_iSysStatus = m_iSysStatus ^ NIR_FAIL; // clear the flag with xor
	}else{
        LOG4CXX_ERROR(logger,"No NIR camera found");
		m_iSysStatus = m_iSysStatus | NIR_FAIL; // set the flag with or
	}
	if(m_iSysStatus)return false;
	else return true;
}

//--------------------------------------------------------------------------------------------------
// GetFilePath
//--------------------------------------------------------------------------------------------------
string PhenoCam::Get_ImageFilePath(){
	return m_sFilePath;
}
//--------------------------------------------------------------------------------------------------
// SetFilePath
//--------------------------------------------------------------------------------------------------
void PhenoCam::Set_ImageFilePath(string filepath){
	m_sFilePath = filepath;
}
//--------------------------------------------------------------------------------------------------
// Configure 
//	-- reads in configuration file and sets parameters, called by initalized
//	-- throws PhenoCamException if operation mode setup fails. 
//--------------------------------------------------------------------------------------------------
void PhenoCam::Configure(string cfgfile=""){
	J_STATUS_TYPE retval;
	NODE_HANDLE hNode;

	if(cfgfile=="" ){//default config file should live in executable dir
		string exd = utility::getEXD();
		cfgfile = exd+string(CONFIG_FILE_NAME);
	}

	m_cCfg = new ConfigFile(cfgfile);
	string val;
	//get or set path for writing images use CWD if not spec'd in config
	if(m_cCfg->keyExists(CONFIG_KEY_OUTPATH)){
		m_sFilePath = m_cCfg->getValueOfKey<std::string>(CONFIG_KEY_OUTPATH);
		LOG4CXX_DEBUG(logger,"Read from cfg file -- key: "<< CONFIG_KEY_OUTPATH <<" value: "<< m_sFilePath.c_str());
	}else{
		m_sFilePath = utility::getCWD();
		LOG4CXX_DEBUG(logger,"Set m_sFilePath to cwd as: "<<m_sFilePath.c_str());
	}

	//Setup PixelFormat and Sync mode.
	//Check conf file for these else set to sync and 12bit packed
	string syncmode; string nirpxfmt; string rgbpxfmt;
	//get or set syncmode use 'Sync' if not spec'd in config
	if(m_cCfg->keyExists(CONFIG_KEY_SYNCMODE)){
		syncmode = m_cCfg->getValueOfKey<std::string>(CONFIG_KEY_SYNCMODE);
		LOG4CXX_DEBUG(logger,"Conf file SyncMode val: "<<syncmode.c_str());
	}else{
		syncmode = "Sync";
		LOG4CXX_DEBUG(logger,"No SyncMode key found in config, setting to 'Sync'");
	}
	//NIR pixelFormat
	if(m_cCfg->keyExists(CONFIG_KEY_NIRPXFMT)){
		nirpxfmt = m_cCfg->getValueOfKey<std::string>(CONFIG_KEY_NIRPXFMT);
		LOG4CXX_DEBUG(logger,"Conf file NIR.PixelFormat val: "<<nirpxfmt.c_str());
	}else{
		nirpxfmt = "Mono12Packed";
		LOG4CXX_DEBUG(logger,"No Nir.PixelFormat key found in config, setting to 'Mono12Packed'");
	}
	//RGB pixelFormat
	if(m_cCfg->keyExists(CONFIG_KEY_RGBPXFMT)){
		rgbpxfmt = m_cCfg->getValueOfKey<std::string>(CONFIG_KEY_RGBPXFMT);
		LOG4CXX_DEBUG(logger,"Conf file RGB.PixelFormat val: "<<rgbpxfmt.c_str());
	}else{
		rgbpxfmt = "BayerRG12Packed";
		LOG4CXX_DEBUG(logger,"No RGB.PixelFormat key found in config, setting to 'BayerRG12Packed'");
	}
	for(int i=0; i<MAX_CAMERAS; i++){
		 // Here we force the two sensors to be synchronized
        // SyncMode=Sync
        // This is only valid for sensor iCameraIndex=0 since the other channel is the "slave"
        if (i == 0)
        {
            hNode = NULL;
            retval = J_Camera_GetNodeByName(m_hCam[i], "SyncMode", &hNode);
            // Does the "SyncMode" node exist?
            if ((retval == J_ST_SUCCESS) && (hNode != NULL))
            {
				retval = J_Camera_SetValueString(m_hCam[i], "SyncMode", (int8_t*) syncmode.c_str());
                if (retval != J_ST_SUCCESS)
                {
					string err = "Could not set SyncMode: "+syncmode+"!";
                    LOG4CXX_ERROR(logger,err.c_str());
                    throw PhenoCamException(err,PCEXCEPTION_CONFSYNCNODE);
                }
            }
			//PixelFormat setup for RGB(Camera 0)
			hNode = NULL;
            retval = J_Camera_GetNodeByName(m_hCam[i], "PixelFormat", &hNode);
			// Does the "PixelFormat" node exist?
            if ((retval == J_ST_SUCCESS) && (hNode != NULL))
            {
				retval = J_Camera_SetValueString(m_hCam[i], "PixelFormat", (int8_t*) rgbpxfmt.c_str());
                if (retval != J_ST_SUCCESS)
                {
					string err = "Could not set RGB.PixelFormat: "+rgbpxfmt+"!";
                    LOG4CXX_ERROR(logger,err.c_str());
                    throw PhenoCamException(err,PCEXCEPTION_PXFMTFAIL);
                }
            }
        }
		//NIR Camera
		if (i==1){
			//PixelFormat
			hNode = NULL;
            retval = J_Camera_GetNodeByName(m_hCam[i], "PixelFormat", &hNode);
			// Does the "PixelFormat" node exist?
            if ((retval == J_ST_SUCCESS) && (hNode != NULL))
            {
				retval = J_Camera_SetValueString(m_hCam[i], "PixelFormat", (int8_t*) nirpxfmt.c_str());
                if (retval != J_ST_SUCCESS)
                {
					string err = "Could not set RGB.PixelFormat: "+nirpxfmt+"!";
                    LOG4CXX_ERROR(logger,err.c_str());
                    throw PhenoCamException(err,PCEXCEPTION_PXFMTFAIL);
                }
            }
		}

	}
	this->ParseConfigMode(); 
}

void PhenoCam::SetCaptureMode(uint8_t capmode=0){
	switch(capmode){
		case CONTINUOUS:
			SetupContinuousMode();
			break;
		case SEQUENCE:
			SetupSequenceMode();
			break;
		case ONE_SHOT:
			SetupOneShotMode();
			break;
		case 0:
			ParseConfigMode();
	}
}

void PhenoCam::ParseConfigMode(){
	string val;
	if(m_cCfg){
		//Configure the operation mode -- 'one_shot', 'continuous' or 'sequence'. ONE_SHOT configured  by default on constr.
		if(m_cCfg->keyExists(CONFIG_SYSOPMODE)){
			val = m_cCfg->getValueOfKey<std::string>(CONFIG_SYSOPMODE);
			if(val=="Sequence"){
				SetupSequenceMode();
			}else if(val=="Continuous"){ //we're doing one_shot so look for exposure and gain and roi setting and set up camera
				SetupContinuousMode();
			}else{ //doing one-shot
				SetupOneShotMode();
			}
		}else{//no key found for mode so we set to one-shot.
			SetupOneShotMode();
		}
	}else{
		string err = "Config handle is not valid";
		LOG4CXX_ERROR(logger, err.c_str());
		throw PhenoCamException(err, PCEXCEPTION_NOVALIDCONF);
	}
}
/*--------------------------------------------------------------------------------------------------
/ SetupOneShotMode 
/	-- sets parameters on camera to trigger one frame capture after toggling softwaretrigger0
/	-- Only call if key: PhenoCam.SysOpMode == OneShot or not specified
/------------------------------------------------------------------------------------------------*/
void PhenoCam::SetupOneShotMode(){
	J_STATUS_TYPE status;
	string err;
	for(int i=0; i<MAX_CAMERAS; i++){
		if(this->m_hCam[i]){
			status = J_Camera_SetValueString(m_hCam[i],"ExposureMode", "EdgePreSelect");
			if(status != J_ST_SUCCESS) 
			{
				err = "Could not set ExposureMode!";
				LOG4CXX_ERROR(logger, err.c_str());
				throw PhenoCamException(err, PCEXCEPTION_CONFOSTFAIL);
			}

			// Set LineSelector="CameraTrigger0" 
			status = J_Camera_SetValueString(m_hCam[i],"LineSelector", "CameraTrigger0");
			if(status != J_ST_SUCCESS) 
			{
				err = "Could not set LineSelector!";
				LOG4CXX_ERROR(logger, err.c_str());
				throw PhenoCamException(err, PCEXCEPTION_CONFOSTFAIL);
			}

			// Set LineSource="SoftwareTrigger0" 
			status = J_Camera_SetValueString(m_hCam[i],"LineSource", "SoftwareTrigger0");
			if(status != J_ST_SUCCESS) 
			{
				err= "Could not set LineSource!";
				LOG4CXX_ERROR(logger, err.c_str());
				throw PhenoCamException(err, PCEXCEPTION_CONFOSTFAIL);
			}
		}else{
			LOG4CXX_ERROR(logger, "Camera handle: "<<i<<" not valid");
			throw PhenoCamException("Camera Handle Unavailable to configure.",0);
		}
	}
	this->m_iMaxFrames=1;
	this->m_iSysOpMode=ONE_SHOT;
}

/*--------------------------------------------------------------------------------------------------
/ SetupContinuousMode 
/	-- reads in configuration file and sets parameters, called by Configure
/	-- Only call if key: PhenoCam.SysOpMode == Continuous
/------------------------------------------------------------------------------------------------*/
void PhenoCam::SetupContinuousMode(){
	J_STATUS_TYPE retval;
	for(int i=0; i<MAX_CAMERAS; i++){
		if(this->m_hCam[i]){
			// Here we assume that this is JAI trigger so we do the following:
            // ExposureMode=Continuous

            // Set ExposureMode=Continuous
		    retval = J_Camera_SetValueString(m_hCam[i], "ExposureMode", "Continuous");
		    if(retval != J_ST_SUCCESS) 
		    {
				string err = "Could not set ExposureMode=Continuous!";
                LOG4CXX_ERROR(logger, err.c_str());
			    throw PhenoCamException(err,PCEXCEPTION_CONFEXPNODE);
		    }
		}else{
			LOG4CXX_ERROR(logger, "Camera handle: "<<i<<" not valid");
			throw PhenoCamException("Camera Handle Unavailable to configure in Continuous.",PCEXCEPTION_CONFCONTFAIL);
		}

	}
	this->m_iSysOpMode=CONTINUOUS;
}

/*--------------------------------------------------------------------------------------------------
/ SetupSequenceMode 
/	-- reads in configuration file and sets parameters, called by Configure
/	-- Only call if key: PhenoCam.SysOpMode == Sequence
/------------------------------------------------------------------------------------------------*/
void PhenoCam::SetupSequenceMode(){
	string delim = ".";
	string prefix = "PhenoCam";
	string sqnum = "NumberOfSequences";
	string sqnode = "Sequence";
	string sqgain = "Gain";
	string sqexp = "Exposure";
	string sqwidth = "Width";
	string sqheight = "Height";
	string sqoffx = "OffsetX";
	string sqoffy = "OffsetY";
	
	J_STATUS_TYPE status;
	NODE_HANDLE hNode;
    int64_t int64Val = 0;

	uint8_t frames = 0;
	string node = prefix+delim+sqnum; //PhenoCam.NumberOfSequences;
	if(m_cCfg->keyExists(node)){
		frames = m_cCfg->getValueOfKey<uint8_t>(node);
		frames = min(frames, MAX_FRAMES);
		//if(frames>1){ //?
		m_iMaxFrames = frames;
		Sequence* s;
		for(int i=1; i<frames+1; i++){
			s = new Sequence;
			s->id = i;
			stringstream intss; 
			intss << i;
			string num = intss.str();
			node = prefix+delim+sqnode+delim+num+delim+sqgain; //PhenoCam.Sequence.i.Gain
			if(m_cCfg->keyExists(node)) s->gain = m_cCfg->getValueOfKey<int16_t>(node);
			node = prefix+delim+sqnode+delim+num+delim+sqexp; //PhenoCam.Sequence.i.Exposure;
			if(m_cCfg->keyExists(node)) s->shutterL = m_cCfg->getValueOfKey<uint16_t>(node);
			node = prefix+delim+sqnode+delim+num+delim+sqwidth; //PhenoCam.Sequence.i.Width;
			if(m_cCfg->keyExists(node)) s->width = m_cCfg->getValueOfKey<uint16_t>(node);
			node =prefix+delim+sqnode+delim+num+delim+sqheight; //PhenoCam.Sequence.i.Height;
			if(m_cCfg->keyExists(node)) s->height = m_cCfg->getValueOfKey<uint16_t>(node);
			node = prefix+delim+sqnode+delim+num+delim+sqoffx; //PhenoCam.Sequence.i.OffsetX;
			if(m_cCfg->keyExists(node)) s->offsetx = m_cCfg->getValueOfKey<uint16_t>(node);
			node = prefix+delim+sqnode+delim+num+delim+sqoffy; //PhenoCam.Sequence.i.OffsetY;
			if(m_cCfg->keyExists(node)) s->offsety = m_cCfg->getValueOfKey<uint16_t>(node);
			m_vSequences.push_back(*s);
			delete s;
			delete intss;
		}
	}else throw PhenoCamException("No Sequences specified in config", PCEXCEPTION_NOSEQINFO);
	//Now set params in camera....
	//}
	for(int cam=0; cam<MAX_CAMERAS; cam++){
        // Here we assume that this is JAI trigger so we do the following:
        // ExposureMode=SequentialEPSTrigger
        // LineSelector=CameraTrigger0
        // LineSource=SoftwareTrigger0
		string err;
	    // Set ExposureMode="SequentialEPSTrigger"
	    status = J_Camera_SetValueString(m_hCam[cam],"ExposureMode", "SequentialEPSTrigger");
	    if(status != J_ST_SUCCESS) 
	    {
			err = "Could not set ExposureMode!";
		    LOG4CXX_ERROR(logger, err.c_str());
		    throw PhenoCamException(err, PCEXCEPTION_CONFSEQFAIL);
	    }

	    // Set LineSelector="CameraTrigger0" 
	    status = J_Camera_SetValueString(m_hCam[cam],"LineSelector", "CameraTrigger0");
	    if(status != J_ST_SUCCESS) 
	    {
			err = "Could not set LineSelector!";
		    LOG4CXX_ERROR(logger, err.c_str());
		    throw PhenoCamException(err, PCEXCEPTION_CONFSEQFAIL);
	    }

	    // Set LineSource="SoftwareTrigger0" 
	    status = J_Camera_SetValueString(m_hCam[cam],"LineSource", "SoftwareTrigger0");
	    if(status != J_ST_SUCCESS) 
	    {
			err= "Could not set LineSource!";
		    LOG4CXX_ERROR(logger, err.c_str());
		    throw PhenoCamException(err, PCEXCEPTION_CONFSEQFAIL);
	    }
        // Set the Sequence Repetition count to 0 (0=forever)
		status = J_Camera_SetValueInt64(m_hCam[cam],"SequenceRepetitions", 0);
		if(status != J_ST_SUCCESS) 
		{
			err = "Could not set SequenceRepetitions!";
			LOG4CXX_ERROR(logger, err.c_str());
			throw PhenoCamException(err, PCEXCEPTION_CONFSEQFAIL);
		}

        // Set the last sequence number to 2 (2 step sequence)
		status = J_Camera_SetValueInt64(m_hCam[cam],"SequenceEndingPosition", m_iMaxFrames);
		if(status != J_ST_SUCCESS) 
		{
			err = "Could not set SequenceEndingPosition!";
			LOG4CXX_ERROR(logger, err.c_str());
			throw PhenoCamException(err, PCEXCEPTION_CONFSEQFAIL);
		}

		//Now we loop over Sequence Vector and config each sequnce frame:
		for(int frame=0; frame<m_iMaxFrames; frame++){
			// Set the ROI's for the two sequences to the whole image size
			Sequence s = this->m_vSequences[frame];
			//Frame #
			status = J_Camera_SetValueInt64(m_hCam[cam],"SequenceSelector", s.id);
			if(status != J_ST_SUCCESS) 
			{
				LOG4CXX_ERROR(logger,"Could not set SequenceSelector!");
			}
			//Exposure
			status = J_Camera_SetValueInt64(m_hCam[cam],"SequenceExposureTimeRaw", s.shutterL);
			if(status != J_ST_SUCCESS) 
			{
				LOG4CXX_ERROR(logger, "Could not set SequenceROIOffsetX!");
			}
			//Gain
			status = J_Camera_SetValueInt64(m_hCam[cam],"SequenceMasterGain", s.gain);
			if(status != J_ST_SUCCESS) 
			{
				LOG4CXX_ERROR(logger, "Could not set SequenceROIOffsetX!");
			}
			//offsetx
			status = J_Camera_SetValueInt64(m_hCam[cam],"SequenceROIOffsetX", s.offsetx);
			if(status != J_ST_SUCCESS) 
			{
				LOG4CXX_ERROR(logger, "Could not set SequenceROIOffsetX!");
			}
			//offsety
			status = J_Camera_SetValueInt64(m_hCam[cam],"SequenceROIOffsetY", s.offsety);
			if(status != J_ST_SUCCESS) 
			{
				LOG4CXX_ERROR(logger,"Could not set SequenceROIOffsetY!");
			}
			//width
			status = J_Camera_GetNodeByName(m_hCam[cam],"SequenceROISizeX", &hNode);
			if(status != J_ST_SUCCESS) 
			{
				LOG4CXX_ERROR(logger,"Could not get SequenceROISizeX node handle!");
			}

			//if width is 0 or greater than max use max
			int64_t max;
			J_Node_GetMaxInt64(hNode, &max);
			if(s.width==0 || s.width>max) int64Val=max;
			else int64Val=s.width;
			//set it
			J_Node_SetValueInt64(hNode, FALSE, int64Val);

			//height
			status = J_Camera_GetNodeByName(m_hCam[cam],"SequenceROISizeY", &hNode);
			if(status != J_ST_SUCCESS) 
			{
				LOG4CXX_ERROR(logger,"Could not get SequenceROISizeY node handle!");
			}
			//if height is 0 or greater than max use max
			J_Node_GetMaxInt64(hNode, &max);
			if(s.height==0 || s.height>max) int64Val=max;
			else int64Val=s.height;
			//set it
			J_Node_SetValueInt64(hNode, FALSE, int64Val);

		}
        // Save Sequence Settings
        status = J_Camera_ExecuteCommand(m_hCam[cam], "SequenceSaveCommand");
		if(status != J_ST_SUCCESS) 
		{
			err = "Could not execute SequenceSaveCommand command!";
			LOG4CXX_ERROR(logger, err.c_str());
			throw PhenoCamException(err, PCEXCEPTION_CONFSEQFAIL);
		}

	}
	this->m_iSysOpMode=SEQUENCE;
}

/*--------------------------------------------------------------------------------------------------
/ Initialize 
/	-- public member method calls OpenFactoryAndCamera then Configure
/	-- will fail if either throws exception and return false. 
/------------------------------------------------------------------------------------------------*/
bool PhenoCam::Open(string confpath = ""){
	try{
		this->OpenFactoryAndCamera();
		this->Configure(confpath);
		LOG4CXX_INFO(logger, "Camera Initialization Success.");
	}catch(exception& e){
		LOG4CXX_ERROR(logger, e.what());
		return false;
	}
	return true;
}

void PhenoCam::TriggerFrame(uint8_t cam){
	J_STATUS_TYPE status = J_ST_SUCCESS;

    // We have two possible ways of software triggering: JAI or GenICam SFNC
    // The GenICam SFNC software trigger uses a node called "TriggerSoftware" and the JAI
    // uses nodes called "SoftwareTrigger0" to "SoftwareTrigger3".
    // Therefor we have to determine which way to use here.
    // First we see if a node called "TriggerSoftware" exists.
    NODE_HANDLE hNode = NULL;
    status = J_Camera_GetNodeByName(m_hCam[cam], "TriggerSoftware", &hNode);

    // Does the "TriggerSoftware" node exist?
    if ((status == J_ST_SUCCESS) && (hNode != NULL))
    {
        // Here we assume that this is GenICam SFNC software trigger so we do the following:
        // TriggerSelector=FrameStart
        // Execute TriggerSoftware Command
	    status = J_Camera_SetValueString(m_hCam[cam],"TriggerSelector", "FrameStart");
	    if(status != J_ST_SUCCESS) 
	    {
		    LOG4CXX_ERROR(logger,"Could not set TriggerSelector!");
		    return;
	    }
	    status = J_Camera_ExecuteCommand(m_hCam[cam],"TriggerSoftware");
	    if(status != J_ST_SUCCESS) 
	    {
		    LOG4CXX_ERROR(logger,"Could not set TriggerSoftware!");
		    return;
	    }
    }
    else
    {
        // Here we assume that this is JAI software trigger so we do the following:
        // SoftwareTrigger0=1
        // SoftwareTrigger0=0

	    status = J_Camera_SetValueInt64(m_hCam[cam], "SoftwareTrigger0", 1);
	    if(status != J_ST_SUCCESS) 
	    {
		   LOG4CXX_ERROR(logger,"Could not set SoftwareTrigger0!");
		    return;
	    }

	    status = J_Camera_SetValueInt64(m_hCam[cam], "SoftwareTrigger0", 0);
	    if(status != J_ST_SUCCESS) 
	    {
		    LOG4CXX_ERROR(logger,"Could not set SoftwareTrigger0!");
		    return;
	    }
    }
}