#include <iostream>
#include <vector>
#include <memory>
#include <chrono>
#include <limits>
#include <cmath>
#if defined(_OPENMP)
	#include <omp.h>
#endif
#include "settings.hpp"

#define M_PI_F 3.14159265358979323846f
#define M_E_F 2.7182818284590452354f

unsigned char* loadBMP(const char *filepath, unsigned int &width, unsigned int &height);
bool saveBMP(const unsigned char *data, const char *filepath, const unsigned int width, const unsigned int height);

// This represents a single colored and styled letter
struct Result {
	public:
		unsigned char c;
		unsigned short fg, bg;
		bool bold, underline;
};

// Returns values linearly from y1 to y2 when x has values from x1 to x2
inline float mix(const float x1, const float x2, const float y1, const float y2, const float x) {
	return (y1 - y2) * (x - x1) / (x1 - x2) + y1;
}

template <typename T, typename U, typename V> T clamp(const T v, const U lo, const V hi) {
	return v < lo ? lo : (v > hi ? hi : v);
}

// Bicubic constant
#define BCC -0.5f

// Get the bicubic multiplier for upscaling purposes
float getBicubicMult(const float x1, const float y1, const float x2, const float y2) {
	const float dx = sqrt((x2 - x1) * (x2 - x1) + (y2 - y1) * (y2 - y1));
	if (dx < 1.0f) return dx * dx * ((BCC + 2.0f) * dx - (BCC + 3.0f)) + 1.0f;
	if (dx < 2.0f) return BCC * (dx * (dx * (dx - 5.0f) + 8.0f) - 4.0f);
	return 0.0f;
}

int main() {
	const auto benchmark = std::chrono::high_resolution_clock::now();

	#if defined(_OPENMP)
		std::cout << "Using " << omp_get_max_threads() << " threads" << std::endl << std::endl;
	#endif

	// Load underline images
	unsigned int width; // temp variable
	unsigned int letterHeight;
	const std::unique_ptr<unsigned char[]> underline1(loadBMP(UNDERLINE1, width, letterHeight));
	const std::unique_ptr<unsigned char[]> underline1b(loadBMP(UNDERLINE1B, width, letterHeight));

	// Load the input image
	unsigned int inputWidth, inputHeight;
	std::unique_ptr<unsigned char[]> input(loadBMP(INPUT, inputWidth, inputHeight));

	std::vector<std::unique_ptr<unsigned char[]>> letters1;
	std::vector<std::unique_ptr<unsigned char[]>> letters1b;
	const unsigned int letterWidth = width;
	const unsigned int RESULT_HEIGHT = (letterWidth * inputHeight * RESULT_WIDTH + (letterHeight * inputWidth - 1)) / letterHeight / inputWidth;
	std::vector<Result> results(RESULT_HEIGHT * RESULT_WIDTH);

	// Separate letters from the images
	unsigned char min1 = 255, max1 = 0;
	unsigned char min2 = 255, max2 = 0;
	for (unsigned int t = 0; t < TEXT_AMOUNT; t++) {
		const std::unique_ptr<unsigned char[]> letterImg(loadBMP(TEXT[t], width, letterHeight));
		const std::unique_ptr<unsigned char[]> letterbImg(loadBMP(TEXTB[t], width, letterHeight));
		for (unsigned int i = 0; i < TEXT_SIZE[t]; i++) {
			letters1.push_back(std::unique_ptr<unsigned char[]>(new unsigned char[letterWidth * letterHeight]));
			letters1b.push_back(std::unique_ptr<unsigned char[]>(new unsigned char[letterWidth * letterHeight]));
			unsigned char *data = letters1.back().get();
			unsigned char *data2 = letters1b.back().get();
			for (unsigned int y = 0; y < letterHeight; y++) {
				for (unsigned int x = 0; x < letterWidth; x++) {
					// Only one color channel is used, so the image should be gray scale
					data[x + y * letterWidth] = letterImg[(x + i * letterWidth + y * width) * 3];
					data2[x + y * letterWidth] = letterbImg[(x + i * letterWidth + y * width) * 3];
					min1 = data[x + y * letterWidth] < min1 ? data[x + y * letterWidth] : min1;
					max1 = data[x + y * letterWidth] > max1 ? data[x + y * letterWidth] : max1;
					min2 = data2[x + y * letterWidth] < min2 ? data2[x + y * letterWidth] : min2;
					max2 = data2[x + y * letterWidth] > max2 ? data2[x + y * letterWidth] : max2;
				}
			}
		}
	}

	// Update the min and max values also from the underline images
	for (unsigned int y = 0; y < letterHeight; y++) {
		for (unsigned int x = 0; x < letterWidth; x++) {
			// Only one color channel is used, so the image should be gray scale
			min1 = underline1[(x + y * letterWidth) * 3] < min1 ? underline1[(x + y * letterWidth) * 3] : min1;
			max1 = underline1[(x + y * letterWidth) * 3] > max1 ? underline1[(x + y * letterWidth) * 3] : max1;
			min2 = underline1b[(x + y * letterWidth) * 3] < min2 ? underline1b[(x + y * letterWidth) * 3] : min2;
			max2 = underline1b[(x + y * letterWidth) * 3] > max2 ? underline1b[(x + y * letterWidth) * 3] : max2;
		}
	}

	std::cout << "Normal color range: " << (int)min1 << "-" << (int)max1 << std::endl;
	std::cout << "Bold color range:   " << (int)min2 << "-" << (int)max2 << std::endl;

	const unsigned int outputWidth = RESULT_WIDTH * letterWidth;
	const unsigned int outputHeight = RESULT_HEIGHT * letterHeight;
	const unsigned int letterArea = letterWidth * letterHeight;

	// Upscaling using bicubic filtering if any of the resulting dimensions are larger than the input image
	if (outputWidth > inputWidth || outputHeight > inputHeight) {
		std::cout << "Scaling up from " << inputWidth << " x " << inputHeight
			<< " to " << outputWidth << " x " << outputHeight << std::endl;

		unsigned char *newInput = new unsigned char[outputWidth * outputHeight * 3];

		// Go through scaled pixels
		#pragma omp parallel for
		for (unsigned int y = 0; y < outputHeight; y++) {
			for (unsigned int x = 0; x < outputWidth; x++) {
				// x and y in the original image
				const float xo = mix(0, outputWidth, 0, inputWidth, x);
				const float yo = mix(0, outputHeight, 0, inputHeight, y);
				float sum = 0, r = 0, g = 0, b = 0;
				// Go through a 4 x 4 grid in the original image
				for(int i = (int)xo - 1; i < (int)xo + 3; i++) {
					for(int j = (int)yo - 1; j < (int)yo + 3; j++) {
						const unsigned int pos = (clamp(j, 0, (int)inputHeight - 1) * inputWidth + clamp(i, 0, (int)inputWidth - 1)) * 3;
						const float mult = getBicubicMult(xo, yo, i, j);
						sum += mult;
						r += input[pos    ] * mult;
						g += input[pos + 1] * mult;
						b += input[pos + 2] * mult;
					}
				}
				const unsigned int pos = (x + y * outputWidth) * 3;
				newInput[pos    ] = clamp(r / sum, 0, 255);
				newInput[pos + 1] = clamp(g / sum, 0, 255);
				newInput[pos + 2] = clamp(b / sum, 0, 255);
			}
		}

		input.reset(newInput);
	}
	// Downscaling using gaussian blurring if the dimensions don't match
	else if (outputWidth != inputWidth || outputHeight != inputHeight) {
		std::cout << "Scaling down from " << inputWidth << " x " << inputHeight
			<< " to " << outputWidth << " x " << outputHeight << std::endl;

		const std::unique_ptr<unsigned char[]> temp(new unsigned char[outputWidth * inputHeight * 3]);
		unsigned char *newInput = new unsigned char[outputWidth * outputHeight * 3];
		const float gaussSizeX = (float)inputWidth / outputWidth;
		const float gaussSizeY = (float)inputHeight / outputHeight;

		// Scale down horizontally
		#pragma omp parallel for
		for (unsigned int y = 0; y < inputHeight; y++) {
			for (int x = 0; x < (int)outputWidth; x++) {
				const float origX = x * gaussSizeX + gaussSizeX * 0.5f;
				float sum1 = 0, sum2 = 0, sum3 = 0;
				float count = 0;
				for (int x2 = std::max(0, int(origX - gaussSizeX)); x2 <= std::min(int(inputWidth - 1), int(ceil(origX + gaussSizeX))); x2++) {
					const float mult = 1.0f / sqrt(2.0f * M_PI_F * gaussSizeX * gaussSizeX / 9.0f) * pow(M_E_F, -pow(abs(origX - x2), 2) / 2.0f / gaussSizeX / gaussSizeX * 9.0f);
					count += mult;
					sum1 += input[(y * inputWidth + x2) * 3    ] * mult;
					sum2 += input[(y * inputWidth + x2) * 3 + 1] * mult;
					sum3 += input[(y * inputWidth + x2) * 3 + 2] * mult;
				}
				temp[(y * outputWidth + x) * 3    ] = sum1 / count;
				temp[(y * outputWidth + x) * 3 + 1] = sum2 / count;
				temp[(y * outputWidth + x) * 3 + 2] = sum3 / count;
			}
		}

		// Scale down vertically
		#pragma omp parallel for
		for (int y = 0; y < (int)outputHeight; y++) {
			const float origY = y * gaussSizeY + gaussSizeY * 0.5f;
			for (unsigned int x = 0; x < outputWidth; x++) {
				float sum1 = 0, sum2 = 0, sum3 = 0;
				float count = 0;
				for (int y2 = std::max(0, int(origY - gaussSizeY)); y2 <= std::min(int(inputHeight - 1), int(ceil(origY + gaussSizeY))); y2++) {
					const float mult = 1.0f / sqrt(2.0f * M_PI_F * gaussSizeY * gaussSizeY / 9.0f) * pow(M_E_F, -pow(abs(origY - y2), 2) / 2.0f / gaussSizeY / gaussSizeY * 9.0f);
					count += mult;
					sum1 += temp[(y2 * outputWidth + x) * 3    ] * mult;
					sum2 += temp[(y2 * outputWidth + x) * 3 + 1] * mult;
					sum3 += temp[(y2 * outputWidth + x) * 3 + 2] * mult;
				}
				newInput[(y * outputWidth + x) * 3    ] = sum1 / count;
				newInput[(y * outputWidth + x) * 3 + 1] = sum2 / count;
				newInput[(y * outputWidth + x) * 3 + 2] = sum3 / count;
			}
		}

		input.reset(newInput);
	}
	else {
		std::cout << "Image size is " << inputWidth << " x " << inputHeight << std::endl;
	}

	std::cout << "Creating the result image..." << std::endl;

	// Go through the letter positions in the resulting image
	for (unsigned int y2 = 0; y2 < RESULT_HEIGHT; y2++) {
		std::cout << (y2 + 1) << " / " << RESULT_HEIGHT << "\r" << std::flush;

		const unsigned int ys = y2 * letterHeight;
		const unsigned int ye = ys + letterHeight;

		#pragma omp parallel for schedule(dynamic)
		for (unsigned int x2 = 0; x2 < RESULT_WIDTH; x2++) {

			const unsigned int xs = x2 * letterWidth;
			const unsigned int xe = xs + letterWidth;

			int best = std::numeric_limits<int>::max() / 2;

			// Create lookup tables for all normal and bold colors for the current patch of the original image
			// This speedup works better if the letter images contain many fully dark/bright pixels
			const std::unique_ptr<unsigned int[]> lookup(new unsigned int[letterArea * 16]);
			unsigned int i = 0;
			for (unsigned char c = 0; c < 16; c++) {
				const short r = (c < 8 ? COLORS[c] : COLORS2[c - 8])[0];
				const short g = (c < 8 ? COLORS[c] : COLORS2[c - 8])[1];
				const short b = (c < 8 ? COLORS[c] : COLORS2[c - 8])[2];
				for (unsigned int y = ys; y < ye; y++) {
					for (unsigned int x = xs; x < xe; x++) {
						const int letterColorR = r - input[(x + y * outputWidth) * 3    ];
						const int letterColorG = g - input[(x + y * outputWidth) * 3 + 1];
						const int letterColorB = b - input[(x + y * outputWidth) * 3 + 2];
						lookup[i] = letterColorR * letterColorR + letterColorG * letterColorG + letterColorB * letterColorB;
						i++;
					}
				}
			}

			// Go through letters, bold and colors
			for (unsigned char c = 0; c < letters1.size(); c++) {
				for (unsigned char fg = 0; fg < 8; fg++) {
					for (unsigned char bg = 0; bg < 8; bg++) {
						for (unsigned char bold = 0; bold < 2; bold++) {
							// This can be skipped because one of the letters should be empty (space character)
							if (!bold && fg == bg) continue;

							// Optimize by calculating some values
							const short minc = bold ? min2 : min1;
							const short maxc = bold ? max2 : max1;
							const float t2 = 1.0f / (minc - maxc);
							const auto &letter = bold ? letters1b[c] : letters1[c];
							const auto &colors = bold ? COLORS2[fg] : COLORS[fg];

							const unsigned int letterArea_bg = letterArea * bg;
							const unsigned int letterArea_fg = letterArea * (bold ? fg + 8 : fg);

							const float c1 = ((short)COLORS[bg][0] - (short)colors[0]) * t2;
							const float c2 = ((short)COLORS[bg][1] - (short)colors[1]) * t2;
							const float c3 = ((short)COLORS[bg][2] - (short)colors[2]) * t2;

							// These values are used to skip some calculations if the pixel color of the letter remains unchanged in consequent pixels
							short letterColorPrev = std::numeric_limits<short>::max();
							int _letterColorR = 0, _letterColorG = 0, _letterColorB = 0;

							int sum1 = 0, sum2 = 0;

							// Dynamic threshold that is used to exit early if the color difference is growing too big
							const float threshold_delta = 1.0f / (ye - ys) / (xe - xs);
							float threshold = QUALITY_THRESHOLD;
							int threshold2 = 0; // threshold2 is just an optimization

							// Go through the current input image patch
							for (unsigned int y = ys; y < ye; y++) {
								const int letterY = letterWidth * (y - ys) - xs;

								for (unsigned int x = xs; x < xe; x++) {
									const unsigned int letterPos = x + letterY;
									const short letterColor = letter[letterPos];

									threshold += threshold_delta;
									threshold2 = threshold > 1.0f ? best : best * threshold;

									// Without underline
									int increase = -1; // this value will also be used for the underline-case if the underline doesn't affect this pixel
									if (sum1 < threshold2) {
										// Use lookup tables
										if (letterColor == minc) {
											increase = lookup[letterPos + letterArea_bg];
										}
										else if (letterColor == maxc) {
											increase = lookup[letterPos + letterArea_fg];
										}
										else {
											// Check if some calculations can be skipped
											if (letterColor != letterColorPrev) {
												letterColorPrev = letterColor;
												const short t1 = letterColorPrev - minc;
												_letterColorR = int(c1 * t1) + COLORS[bg][0];
												_letterColorG = int(c2 * t1) + COLORS[bg][1];
												_letterColorB = int(c3 * t1) + COLORS[bg][2];
											}

											const int letterColorR = _letterColorR - input[(x + y * outputWidth) * 3    ];
											const int letterColorG = _letterColorG - input[(x + y * outputWidth) * 3 + 1];
											const int letterColorB = _letterColorB - input[(x + y * outputWidth) * 3 + 2];
											increase = letterColorR * letterColorR + letterColorG * letterColorG + letterColorB * letterColorB;
										}
										sum1 += increase;
									}

									// With underline
									if (sum2 < threshold2) {
										short letterColor2 = (bold ? underline1b : underline1)[letterPos * 4];
										if (letterColor > letterColor2) letterColor2 = letterColor;

										// Check if all calculations can be skipped
										if (letterColor2 == letterColor && increase != -1) {
											sum2 += increase;
										}
										else {
											// Use lookup tables
											if (letterColor2 == minc) {
												sum2 += lookup[letterPos + letterArea_bg];
											}
											else if (letterColor2 == maxc) {
												sum2 += lookup[letterPos + letterArea_fg];
											}
											else {
												// Check if some calculations can be skipped
												if (letterColor2 != letterColorPrev) {
													letterColorPrev = letterColor2;
													const short t1 = letterColorPrev - minc;
													_letterColorR = int(c1 * t1) + COLORS[bg][0];
													_letterColorG = int(c2 * t1) + COLORS[bg][1];
													_letterColorB = int(c3 * t1) + COLORS[bg][2];
												}

												const int letterColorR = _letterColorR - input[(x + y * outputWidth) * 3    ];
												const int letterColorG = _letterColorG - input[(x + y * outputWidth) * 3 + 1];
												const int letterColorB = _letterColorB - input[(x + y * outputWidth) * 3 + 2];
												sum2 += letterColorR * letterColorR + letterColorG * letterColorG + letterColorB * letterColorB;
											}
										}
									}

									// Early exit
									else if (sum1 >= threshold2) {
										y = ye;
										x = xe;
									}
								}
							}

							// Update results
							if (sum1 < threshold2) {
								best = sum1;
								auto &result = results[x2 + y2 * RESULT_WIDTH];
								result.c = c;
								result.fg = fg;
								result.bg = bg;
								result.bold = bold;
								result.underline = false;
							}
							// can't use threshold2 anymore because best might be updated
							if (threshold > 1.0f) threshold = 1.0f;
							if (sum2 < int(best * threshold)) {
								best = sum2;
								auto &result = results[x2 + y2 * RESULT_WIDTH];
								result.c = c;
								result.fg = fg;
								result.bg = bg;
								result.bold = bold;
								result.underline = true;
							}

						}
					}
				}
			}

		}
	}

	// Print out the results
	#ifdef SHOW_RESULTS_IN_CONSOLE
		for (unsigned int y = 0; y < RESULT_HEIGHT; y++) {
			for (unsigned int x = 0; x < RESULT_WIDTH; x++) {
				const auto &result = results[x + (RESULT_HEIGHT - y - 1) * RESULT_WIDTH];
				std::cout << "\033[0;";
				if (result.bold) std::cout << "1;";
				if (result.underline) std::cout << "4;";
				std::cout << (result.fg + 30) << ";" << (result.bg + 40) << "m";
				// Figure out the character index
				unsigned int i = 0;
				unsigned int size = TEXT_SIZE[0];
				while (result.c >= size) {
					i++;
					size += TEXT_SIZE[i];
				}
				const unsigned int c = result.c + TEXT_SIZE[i] - size + TEXT_FIRST[i];
				// Print UTF-8 characters
				if (c < 128) std::cout << char(c);
				else if (c < 2048) std::cout << char(192 + (c >> 6)) << char(128 + (c & 63));
				else if (c < 65536) std::cout << char(224 + (c >> 12)) << char(128 + ((c >> 6) & 63)) << char(128 + (c & 63));
				else std::cout << char(240 + (c >> 18)) << char(128 + ((c >> 12) & 63)) << char(128 + ((c >> 6) & 63)) << char(128 + (c & 63));
			}
			std::cout << "\033[0m" << std::endl;
		}
		std::cout << "\033[0m";
	#endif

	// Create a BMP version of the results
	const std::unique_ptr<unsigned char[]> result(new unsigned char[outputWidth * outputHeight * 3]);
	#pragma omp parallel for
	for (unsigned int y = 0; y < RESULT_HEIGHT; y++) {
		const unsigned int posy = y * letterHeight;
		for (unsigned int x = 0; x < RESULT_WIDTH; x++) {
			const unsigned int posx = x * letterWidth;
			const auto &c = results[x + y * RESULT_WIDTH];
			const unsigned char minc = c.bold ? min2 : min1;
			const unsigned char maxc = c.bold ? max2 : max1;
			const auto &letter = c.bold ? letters1b[c.c] : letters1[c.c];
			const auto &colors = c.bold ? COLORS2[c.fg] : COLORS[c.fg];

			for (unsigned int y2 = 0; y2 < letterHeight; y2++) {
				for (unsigned int x2 = 0; x2 < letterWidth; x2++) {
					const unsigned int letterPos = x2 + y2 * letterWidth;
					unsigned char letterColor = letter[letterPos];
					if (c.underline) letterColor = std::max(letterColor, (c.bold ? underline1b : underline1)[letterPos * 3]);

					const unsigned int pos = (x2 + posx + (y2 + posy) * outputWidth) * 3;
					result[pos    ] = mix(minc, maxc, COLORS[c.bg][0], colors[0], letterColor);
					result[pos + 1] = mix(minc, maxc, COLORS[c.bg][1], colors[1], letterColor);
					result[pos + 2] = mix(minc, maxc, COLORS[c.bg][2], colors[2], letterColor);
				}
			}

		}
	}

	saveBMP(result.get(), "result.bmp", outputWidth, outputHeight);

	const auto end = std::chrono::high_resolution_clock::now();
	std::cout << std::endl << "Time taken: "
		<< ((std::chrono::duration_cast<std::chrono::nanoseconds>(end-benchmark).count() / 10000000) / 100.0)
		<< " seconds" << std::endl;
	return 0;
}
