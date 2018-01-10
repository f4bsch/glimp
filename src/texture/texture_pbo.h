#pragma once

#include "texture2d.h"
#include "pack_buffer.h"

namespace gl {
	template<typename T=float, int C = 4>
	class texture_pbo : public texture2d<T, C> {
		pack_buffer<T, C> pb;
	public:
		texture_pbo(const std::string &name, int width, int height, buf_use use)
				: texture2d<T, C>(name, width, height), pb(width, height, use) {}

		~texture_pbo() {}

		void init() override {
			pb.init();
			texture2d<T, C>::init();
		}

		void deinit() override {
			texture2d<T, C>::deinit();
			pb.deinit();
		}


		inline void read(std::function<void(const T *)> &reader, int n) {
			// TODO: check n
			pb.read(*this, reader);
		}

		void read(T *buf, int n) override {
			if (buf != nullptr)
				throw std::logic_error("unsupported synchronous read");
			texture2d<T, C>::read(buf, n);
		}

	protected:
		void doUpload(const T *data) override {
			size_t nBytes = sizeof(T) * C * texture_base::width * texture_base::height;
			std::function<void(T *)> writer = [=](T *dst) {
				if (data != nullptr)
					memcpy(dst, data, nBytes);
			};
			pb.write(*this, writer);
		}
	};
}
