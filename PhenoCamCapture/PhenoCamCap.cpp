// PhenoCamCapture.cpp : Defines the entry point for the console application.
//
#include "stdafx.h"
#include "PhenoCam.h"
#include "Utility.h"
#include "log4cxx/propertyconfigurator.h"

using namespace std;
using namespace log4cxx;
using namespace log4cxx::helpers;

LoggerPtr logger(Logger::getLogger("Main"));

int _tmain(int argc, _TCHAR* argv[])
{
	string exd = utility::getEXD();
	string logfile = exd+string(CONFIG_FILE_NAME);
	PropertyConfigurator::configure(logfile.c_str());
	PhenoCam ph;
	string out = ph.Get_FilePath();
	LOG4CXX_INFO(logger, "EXE File path is set to: " << exd.c_str());
	LOG4CXX_INFO(logger, "Output File path is set to: " << out.c_str());
	LOG4CXX_INFO(logger, "Starting Capture...");
	ph.Capture();
	LOG4CXX_INFO(logger, "Capture finished.");
	LOG4CXX_INFO(logger, "Closing Cameras...");
	ph.CloseFactoryAndCamera();
}