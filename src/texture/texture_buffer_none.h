#pragma once

#include "texture_base.h"

namespace gl {
	template<typename T=float, int C = 4>
	class texture_buffer : public texture_base {
	public:
		texture_buffer(std::string n, int w, int h, buf_use use) : texture_base("",0,0) {}
		inline void read(T *imgData, size_t n) {		}
	};
}