#include <iostream>
#include <string>
#include <vector>
#include <SDL.h>

#include "StdDef.h"

#include "CpsFile.h"
#include "Exception.h"
#include "Log.h"
#include "PalFile.h"

using namespace eastwood;

CpsFile::CpsFile(std::istream &stream, SDL_Palette *palette) : Decode(stream, 320, 200, palette)
{
    _stream.ignore(2);
    if(readU16LE(_stream) != 0x0004 ||
	    readU16LE(_stream) != 0xFA00)
	throw(Exception(LOG_ERROR, "CpsFile", "Invalid header"));

    _stream.ignore(2);

    if(readU16LE(_stream)== sizeof(Palette)){
	LOG_INFO("CpsFile", "CPS has embedded palette, loading...");
	PalFile pal(_stream);
	_palette = pal.getPalette();
    }
}

CpsFile::~CpsFile()
{	
}

SDL_Surface *CpsFile::getSurface()
{
    std::vector<uint8_t> ImageOut(_width*_height);

    if(decode80(&ImageOut.front(),0) == -2)
	throw(Exception(LOG_ERROR, "CpsFile", "Cannot decode Cps-File"));

    return createSurface(&ImageOut.front(), SDL_SWSURFACE);
}
