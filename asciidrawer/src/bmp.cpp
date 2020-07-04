#include <iostream>
#include <fstream>
#include <memory>

unsigned char *loadBMP(const char *filepath, unsigned int &width, unsigned int &height) {
	std::ifstream file(filepath, std::ios::in | std::ios::binary | std::ios::ate);
	if (!file.good()) {
		std::cout << "Couldn't load texture from " << filepath << std::endl;
		file.close();
		return 0;
	}
	const int size = file.tellg();
	file.seekg(0, std::ios::beg);
	const std::unique_ptr<unsigned char[]> data(new unsigned char[size]);
	file.read((char*)data.get(), size);
	file.close();

	// Test compatibility
	if (data[30]) {
		std::cout << "Bitmap compression " << (int)data[30] << " not supported!" << std::endl;
		return 0;
	}
	const unsigned char bpp = data[28];
	if (bpp != 16 && bpp != 24 && bpp != 32) {
		std::cout << "Bitmap format " << (int)bpp << " bits per pixel not supported!" << std::endl;
		return 0;
	}

	// Dimensions
	width = data[18] + (data[19] << 8) + (data[20] << 16) + (data[21] << 24);
	height = data[22] + (data[23] << 8) + (data[24] << 16) + (data[25] << 24);
	unsigned char *pixels = new unsigned char[width * height * 3];
	// Calculate padding
	unsigned int padding = 0;
	if (bpp == 16 && width % 2) padding = 2;
	if (bpp == 24 && width % 4) padding = 4 - (width * 3) % 4;
	// Start of pixel data
	unsigned int count = data[10] + (data[11] << 8) + (data[12] << 16) + (data[13] << 24);

	// Pixel data
	for (unsigned int i = 0; i < height; i++) {
		for (unsigned int j = 0; j < width; j++) {
			const unsigned int pos = (i * width + j) * 3;
			if (bpp == 16) {
				pixels[pos + 2] = (unsigned char)(float(data[count] & 31) * 8.23); // 00054321 00000000
				pixels[pos + 1] = (unsigned char)(float(((data[count] >> 5) & 7) + ((data[count + 1] << 3) & 24)) * 8.23); // 32100000 00000054
				pixels[pos] = (unsigned char)(float((data[count + 1] >> 2) & 31) * 8.23); // 00000000 05432100
				//pixels[pos + 3] = 255;//(data[count + 1] & 128) ? 255 : 0; // 00000000 10000000
				count += 2;
			}
			else {
				pixels[pos + 2] = data[count];
				pixels[pos + 1] = data[count + 1];
				pixels[pos] = data[count + 2];
				if (bpp == 32) {
					//pixels[pos + 3] = data[count + 3];
					count += 4;
				}
				else {
					//pixels[pos + 3] = 255;
					count += 3;
				}
			}
		}
		count += padding;
	}

	return pixels;
}

inline void updateHeader(char *headerPos, const unsigned int value) {
	headerPos[0] = value & 0xff;
	headerPos[1] = (value >> 8) & 0xff;
	headerPos[2] = (value >> 16) & 0xff;
	headerPos[3] = value >> 24;
}

bool saveBMP(const unsigned char *data, const char *filepath, const unsigned int width, const unsigned int height) {
	std::ofstream file(filepath, std::ios::binary);
	if (!file.good()) {
		file.close();
		return false;
	}

	// Create the header
	char header[54] = {
		66, 77,      // BM
		0, 0, 0, 0,  // size of the file
		0, 0, 0, 0,
		54, 0, 0, 0, // offset to image data
		40, 0, 0, 0, // size of this header
		0, 0, 0, 0,  // width of the image
		0, 0, 0, 0,  // height of the image
		1, 0, 24, 0, // bits per pixel
		0, 0, 0, 0,
		0, 0, 0, 0,  // size of the pixel data
		0, 0, 0, 0,
		0, 0, 0, 0,
		0, 0, 0, 0,
		0, 0, 0, 0,
	};

	const unsigned char padding = width % 4;
	const char *paddingArray = header + 50; // just use part of the header for writing padding (zeros) to the file

	// Update the header
	updateHeader(header + 2, width * height * 3 + padding * height + 54); // size of the file
	updateHeader(header + 18, width); // width of the image
	updateHeader(header + 22, height); // height of the image
	updateHeader(header + 34, width * height * 3); // size of the pixel data
	file.write(header, 54);

	// Save the pixel data
	const unsigned char *dataPos = data;
	for(unsigned int i = 0; i < height; i++) {
		for(unsigned int j = 0; j < width; j++) {
			file.put(dataPos[2]);
			file.put(dataPos[1]);
			file.put(dataPos[0]);
			dataPos += 3;
		}
		file.write(paddingArray, padding);
	}

	file.close();
	return true;
}
