#pragma once
#include "stdafx.h"

#define uint8_t unsigned char

#define PCEXCEPTION_COMMFAIL 1
#define PCEXCEPTION_RGBCAMFAIL 2
#define PCEXCEPTION_NIRCAMFAIL 3
#define PCEXCEPTION_RGBSTREAMFAIL 4
#define PCEXCEPTION_NIRSTREAMFAIL 5
#define PCEXCEPTION_CONFEXPNODE 6
#define PCEXCEPTION_CONFSYNCNODE 7
#define PCEXCEPTION_NOSEQINFO 8
#define PCEXCEPTION_CONFSEQFAIL 9
#define PCEXCEPTION_FACTORYFAIL 10
#define PCEXCEPTION_UPDTCAMLIST 11
#define PCEXCEPTION_NUMCAMSFAIL 12
#define PCEXCEPTION_CAMIDFAIL 13
#define PCEXCEPTION_PXFMTFAIL 14
#define PCEXCEPTION_CONFOSTFAIL 15
#define PCEXCEPTION_CONFCONTFAIL 16
#define PCEXCEPTION_NOVALIDCONF	17

using namespace std;

class PhenoCamException : runtime_error {

	public:
		PhenoCamException(string amsg,uint8_t aval);
		uint8_t getVal(void);

	protected:
		uint8_t val;
	
};