#include "stdafx.h"
#include "Utility.h"
#include <windows.h>
#include <direct.h>

using namespace std;

namespace utility{

	void path_strip_filename(TCHAR *Path) {
		size_t Len = _tcslen(Path);
		if (Len==0) {return;};
		size_t Idx = Len-1;
		while (TRUE) {
			TCHAR Chr = Path[Idx];
			if (Chr==TEXT('\\')||Chr==TEXT('/')) {
				if (Idx==0||Path[Idx-1]==':') {Idx++;};
				break;
			} else if (Chr==TEXT(':')) {
				Idx++; break;
			} else {
				if (Idx==0) {break;} else {Idx--;};
			};
		};
		Path[Idx] = TEXT('\0');
	}



	//get current working directory as string
	string getCWD(void){

		char cCurrentPath[FILENAME_MAX];
 		_getcwd(cCurrentPath, sizeof(cCurrentPath));
		string cwd(cCurrentPath);
		return cwd;
	}

	//get location of executable: WIN OS only
	string getEXD(void){
		TCHAR cExePath[FILENAME_MAX];//always use FILENAME_MAX for filepaths
		GetModuleFileName(NULL,cExePath,sizeof(cExePath)); 
		path_strip_filename(cExePath);
		wstring wexd(cExePath); 
		//have to convert from wstring to string. 
		string exd (wexd.begin(), wexd.end());
		return exd;
	}
}