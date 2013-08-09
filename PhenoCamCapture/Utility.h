#include "stdafx.h"

using namespace std;

namespace utility{

	//strip filename off a path
	void path_strip_filename(TCHAR *Path);
	//get current working directory as string
	 string getCWD(void);
	//get location of executable: WIN OS only
	 string getEXD(void);
}