#ifndef EASTWOOD_PALETTE_H
#define EASTWOOD_PALETTE_H

#include <vector>

namespace eastwood {

struct Color {
    uint8_t r;
    uint8_t g;
    uint8_t b;
    uint8_t unused;
};

class Palette {
    public:
	Palette(uint16_t colors = 256);
	virtual ~Palette();

	Color& operator[] (uint16_t i) { return _palette.at(i); }

	size_t size() const { return _palette.size(); }

    protected:
	std::vector<Color> _palette;
};
}
#endif // EASTWOOD_PALETTE_H