#ifndef EASTWOOD_SDL_SURFACE_H
#define EASTWOOD_SDL_SURFACE_H

#include <SDL/SDL_video.h>

#include "eastwood/Decode.h"
#include "eastwood/PalFile.h"

namespace eastwood { namespace SDL {

class Surface : public eastwood::Surface, public SDL_Surface
{
    public:
	Surface(const eastwood::Surface& surface, uint32_t flags = SDL_HWSURFACE,
		uint32_t Rmask = 0, uint32_t Gmask = 0, uint32_t Bmask = 0, uint32_t Amask = 0);
	Surface(const SDL_Surface& surface);
	virtual ~Surface();

    private:
	SDL_Surface *_surface;
};

}}

#endif // EASTWOOD_SDL_SURFACE_H