#include "eastwood/ArcStream.h"
#include "eastwood/Log.h"
#include "eastwood/IniFile.h"
#include "eastwood/StdDef.h"
#include "eastwood/Surface.h"
#include "eastwood/CpsFile.h"
#include "eastwood/FntFile.h"
#include "eastwood/StringFile.h"

const char* mixes[] = {"tdtest.mix", "ratest.mix", "rasub.mix"};
const char* strfiles[] = {"conquer.eng", "setup.dip", "redalert.eng"};
const char* inifile[] = { "redalert.ini" };

using namespace eastwood;

int main(int argc, char** argv)
{
    ArcFileInfo finfo = {18, 11564, "tdtest.mix"};
    ArcIOStream file(finfo);
    //file.open(finfo);
    StringFile strf(file);
    strf.list();
    
    return 0;
}
