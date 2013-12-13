// PhenoCamCapture.cpp : Defines the entry point for the console application.
//
#include "stdafx.h"
#include "PhenoCam.h"
#include "PhenoCamException.h"
#include "Utility.h"
#include "log4cxx/propertyconfigurator.h"

using namespace std;
using namespace log4cxx;
using namespace log4cxx::helpers;

LoggerPtr logger(Logger::getLogger("Main"));

int _tmain(int argc, _TCHAR* argv[])
{
	int rval = 0;
	string exd = utility::getEXD();
	string logfile = exd+string(CONFIG_FILE_NAME);
	PropertyConfigurator::configure(logfile.c_str());
	PhenoCam ph;

	LOG4CXX_DEBUG(logger, "EXE File path is set to: " << exd.c_str());
	LOG4CXX_INFO(logger, "Starting Capture...");
	if( ph.Open()){
		LOG4CXX_INFO(logger, "Camera init and config sucess");
		string out = ph.Get_ImageFilePath();
		LOG4CXX_DEBUG(logger, "Output File path is set to: " << out.c_str());
		try{
			ph.Capture();
			LOG4CXX_INFO(logger, "Capture finished.");
		}catch(PhenoCamException& e){
			LOG4CXX_ERROR(logger, "Capture threw exception, reseting device");
			ph.Reset();
			rval=31; 
			//Windows General Device Failure: ERROR_GEN_FAILURE
			//A device attached to the system is not functioning.
		}
	}else{
		LOG4CXX_ERROR(logger, "Camera init failed!");
		rval=30; 
		//Windows Read Fault:ERROR_READ_FAULT
		//The system cannot read from the specified device.
	}
	LOG4CXX_INFO(logger, "Closing Cameras...");
	ph.Close();
	return rval;
}