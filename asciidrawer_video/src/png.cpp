#include <png.h>
#include <iostream>
#include <fstream>

#define PNGSIGSIZE 8

#define PNG_COMPRESSION 1

void userReadData(png_structp pngPtr, png_bytep data, png_size_t length) {
	png_voidp a = png_get_io_ptr(pngPtr);
	((std::istream*)a)->read((char*)data, length);
}

unsigned char *loadPNG(const char *filename, unsigned int &width, unsigned int &height, unsigned int &_channels) {
	std::ifstream file(filename, std::ifstream::in | std::ifstream::binary);
	if (file.bad() || !file.is_open()) {
		std::cout << "Couldn't load texture from " << filename << std::endl;
		file.close();
		return 0;
	}

	png_byte pngsig[PNGSIGSIZE];
	file.read((char*)pngsig, PNGSIGSIZE);
	if (png_sig_cmp(pngsig, 0, PNGSIGSIZE) != 0) {
		std::cout << "Not a valid PNG!" << std::endl;
		file.close();
		return 0;
	}

	png_structp pngPtr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
	if (!pngPtr) {
		std::cout << "Couldn't initialize png read struct" << std::endl;
		file.close();
		return 0;
	}
	png_infop infoPtr = png_create_info_struct(pngPtr);
	if (!infoPtr) {
		std::cout << "Couldn't initialize png info struct" << std::endl;
		png_destroy_read_struct(&pngPtr, (png_infopp)0, (png_infopp)0);
		file.close();
		return 0;
	}
	if (setjmp(png_jmpbuf(pngPtr))) {
		png_destroy_read_struct(&pngPtr, &infoPtr,(png_infopp)0);
		std::cout << "An error occured while reading the PNG file" << std::endl;
		file.close();
		return 0;
	}

	png_set_read_fn(pngPtr,(png_voidp)&file, userReadData);

	png_set_sig_bytes(pngPtr, PNGSIGSIZE);
	png_read_info(pngPtr, infoPtr);

	png_uint_32 imgWidth = png_get_image_width(pngPtr, infoPtr);
	png_uint_32 imgHeight = png_get_image_height(pngPtr, infoPtr);
	png_uint_32 bitdepth = png_get_bit_depth(pngPtr, infoPtr);
	png_uint_32 channels = png_get_channels(pngPtr, infoPtr);
	png_uint_32 color_type = png_get_color_type(pngPtr, infoPtr);

	switch (color_type) {
		case PNG_COLOR_TYPE_PALETTE:
			png_set_palette_to_rgb(pngPtr);
			channels = 3;
			break;
		case PNG_COLOR_TYPE_GRAY:
			if (bitdepth < 8)
			png_set_expand_gray_1_2_4_to_8(pngPtr);
			bitdepth = 8;
			break;
	}
	if (png_get_valid(pngPtr, infoPtr, PNG_INFO_tRNS)) {
		png_set_tRNS_to_alpha(pngPtr);
		channels += 1;
	}

	png_bytep* rowPtrs = new png_bytep[imgHeight];
	char unsigned* data = new unsigned char[imgWidth * imgHeight * bitdepth * channels / 8];
	const unsigned int stride = imgWidth * bitdepth * channels / 8;
	for (size_t i = 0; i < imgHeight; i++) {
		png_uint_32 q = (imgHeight - i - 1) * stride;
		rowPtrs[i] = (png_bytep)data + q;
	}
	png_read_image(pngPtr, rowPtrs);

	delete[] (png_bytep)rowPtrs;
	png_destroy_read_struct(&pngPtr, &infoPtr,(png_infopp)0);

	width = imgWidth;
	height = imgHeight;
	_channels = channels;

	return data;
}

bool savePNG(const unsigned char *data, const char* filename, const unsigned int width, const unsigned int height) {
	if (!data) return false;
	FILE* file = fopen(filename, "wb");
	if (!file) return false;

	png_structp png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
	if (!png_ptr) {
		fclose(file);
		return false;
	}

	png_infop info_ptr = png_create_info_struct(png_ptr);
	if (!info_ptr) {
		fclose(file);
		return false;
	}

	if (setjmp(png_jmpbuf(png_ptr))) {
		fclose(file);
		png_destroy_write_struct(&png_ptr, &info_ptr);
		return false;
	}

	png_init_io(png_ptr, file);

	png_set_compression_level(png_ptr, PNG_COMPRESSION);

	png_set_IHDR(
		png_ptr,
		info_ptr,
		width,
		height,
		8,
		PNG_COLOR_TYPE_RGB,
		PNG_INTERLACE_NONE,
		PNG_COMPRESSION_TYPE_DEFAULT,
		PNG_FILTER_TYPE_DEFAULT
	);

	png_write_info(png_ptr, info_ptr);

	for (unsigned int y = 0; y < height; y++) {
		unsigned char* pRow = (unsigned char*)&data[(height - y - 1) * width * 3];
		png_write_row(png_ptr, pRow);
	}

	png_write_end(png_ptr, info_ptr);
	png_destroy_write_struct(&png_ptr, &info_ptr);
	fclose(file);

	return true;
}
