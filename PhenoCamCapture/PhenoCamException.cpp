#include "stdafx.h"
#include "PhenoCamException.h"

PhenoCamException::PhenoCamException(string amsg,uint8_t aval):runtime_error(amsg){
	this->val=aval;
}
uint8_t PhenoCamException::getVal(){return this->val;}