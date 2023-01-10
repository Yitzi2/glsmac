#pragma once

#include <string>

#include "../Loader.h"

#include "types/Texture.h"

namespace loader {
namespace texture {

MAJOR_CLASS( TextureLoader, Loader )

	virtual ~TextureLoader() {
		if (m_transparent_color) {
			delete m_transparent_color;
		}
	}

	// loadtexture flags
	static const uint8_t LT_NONE = 0;
	static const uint8_t LT_ROTATE = 1;
	static const uint8_t LT_FLIPV = 2;
	static const uint8_t LT_FLIPH = 4;
	static const uint8_t LT_ALPHA = 8;
	static const uint8_t LT_CONTRAST = 16;

	// load full texture
	virtual types::Texture *LoadTexture( const std::string &name ) = 0;

	// load part of texture
	virtual types::Texture *LoadTexture( const string &name, const size_t x1, const size_t y1, const size_t x2, const size_t y2, const uint8_t flags = LT_NONE, const float value = 1.0 ) = 0;
	
	// treat specific color as transparent
	void SetTransparentColor( const types::Color::rgba_t rgba ) {
		if (m_transparent_color) {
			delete m_transparent_color;
		}
		m_transparent_color = new types::Color::rgba_t;
		*m_transparent_color = rgba;
	}
	
protected:
	types::Color::rgba_t *m_transparent_color = nullptr;
	
};

} /* namespace texture */
} /* namespace loader */
