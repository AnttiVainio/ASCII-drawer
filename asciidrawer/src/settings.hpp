#define SHOW_RESULTS_IN_CONSOLE

// Width is in characters
#define RESULT_WIDTH 200

// This can be from 0 to 1 - smaller values produce images faster but with lower quality
#define QUALITY_THRESHOLD 0.15f

#define INPUT "example.bmp"

#define UNDERLINE1 "font/underline.bmp"
#define UNDERLINE1B "font/underline-bold.bmp"

#define TEXT_AMOUNT 4 // the amount of font images defined below

// Only one color channel is used, so the image should be gray scale
const char *TEXT[TEXT_AMOUNT] = {
	"font/unicode-32-126.bmp", "font/unicode-161-255.bmp", "font/unicode-2404-2417.bmp", "font/unicode-2534-2554.bmp",
};
const char *TEXTB[TEXT_AMOUNT] = {
	"font/unicode-32-126-bold.bmp", "font/unicode-161-255-bold.bmp", "font/unicode-2404-2417-bold.bmp", "font/unicode-2534-2554-bold.bmp",
};
// The Unicode index of the first character in the image
const unsigned int TEXT_FIRST[TEXT_AMOUNT] = {
	32, 161, 2404, 2534,
};
// The amount of characters in the image
const unsigned int TEXT_SIZE[TEXT_AMOUNT] = {
	95, 95, 14, 21,
};

const unsigned char COLORS[8][3] = {
	{  0,   0,   0},
	{170,   0,   0},
	{  0, 170,   0},
	{170,  85,   0},
	{  0,   0, 170},
	{170,   0, 170},
	{  0, 170, 170},
	{170, 170, 170},
};
const unsigned char COLORS2[8][3] = {
	{ 85,  85,  85},
	{255,  85,  85},
	{ 85, 255,  85},
	{255, 255,  85},
	{ 85,  85, 255},
	{255,  85, 255},
	{ 85, 255, 255},
	{255, 255, 255},
};

// Alternate color scheme

/*const unsigned char COLORS[8][3] = {
	{  0,   0,   0},
	{128,   0,   0},
	{  0, 128,   0},
	{128, 128,   0},
	{  0,   0, 128},
	{128,   0, 128},
	{  0, 128, 128},
	{192, 192, 192},
};

const unsigned char COLORS2[8][3] = {
	{128, 128, 128},
	{255,   0,   0},
	{  0, 255,   0},
	{255, 255,   0},
	{  0,   0, 255},
	{255,   0, 255},
	{  0, 255, 255},
	{255, 255, 255},
};*/
