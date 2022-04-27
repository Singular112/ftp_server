#pragma once

typedef struct ConvLetter
{
	char	win1251;
	int		unicode;
} Letter;

static Letter g_letters[] = {
	{ 0x82, 0x201A }, // SINGLE LOW-9 QUOTATION MARK
	{ 0x83, 0x0453 }, // CYRILLIC SMALL LETTER GJE
	{ 0x84, 0x201E }, // DOUBLE LOW-9 QUOTATION MARK
	{ 0x85, 0x2026 }, // HORIZONTAL ELLIPSIS
	{ 0x86, 0x2020 }, // DAGGER
	{ 0x87, 0x2021 }, // DOUBLE DAGGER
	{ 0x88, 0x20AC }, // EURO SIGN
	{ 0x89, 0x2030 }, // PER MILLE SIGN
	{ 0x8A, 0x0409 }, // CYRILLIC CAPITAL LETTER LJE
	{ 0x8B, 0x2039 }, // SINGLE LEFT-POINTING ANGLE QUOTATION MARK
	{ 0x8C, 0x040A }, // CYRILLIC CAPITAL LETTER NJE
	{ 0x8D, 0x040C }, // CYRILLIC CAPITAL LETTER KJE
	{ 0x8E, 0x040B }, // CYRILLIC CAPITAL LETTER TSHE
	{ 0x8F, 0x040F }, // CYRILLIC CAPITAL LETTER DZHE
	{ 0x90, 0x0452 }, // CYRILLIC SMALL LETTER DJE
	{ 0x91, 0x2018 }, // LEFT SINGLE QUOTATION MARK
	{ 0x92, 0x2019 }, // RIGHT SINGLE QUOTATION MARK
	{ 0x93, 0x201C }, // LEFT DOUBLE QUOTATION MARK
	{ 0x94, 0x201D }, // RIGHT DOUBLE QUOTATION MARK
	{ 0x95, 0x2022 }, // BULLET
	{ 0x96, 0x2013 }, // EN DASH
	{ 0x97, 0x2014 }, // EM DASH
	{ 0x99, 0x2122 }, // TRADE MARK SIGN
	{ 0x9A, 0x0459 }, // CYRILLIC SMALL LETTER LJE
	{ 0x9B, 0x203A }, // SINGLE RIGHT-POINTING ANGLE QUOTATION MARK
	{ 0x9C, 0x045A }, // CYRILLIC SMALL LETTER NJE
	{ 0x9D, 0x045C }, // CYRILLIC SMALL LETTER KJE
	{ 0x9E, 0x045B }, // CYRILLIC SMALL LETTER TSHE
	{ 0x9F, 0x045F }, // CYRILLIC SMALL LETTER DZHE
	{ 0xA0, 0x00A0 }, // NO-BREAK SPACE
	{ 0xA1, 0x040E }, // CYRILLIC CAPITAL LETTER SHORT U
	{ 0xA2, 0x045E }, // CYRILLIC SMALL LETTER SHORT U
	{ 0xA3, 0x0408 }, // CYRILLIC CAPITAL LETTER JE
	{ 0xA4, 0x00A4 }, // CURRENCY SIGN
	{ 0xA5, 0x0490 }, // CYRILLIC CAPITAL LETTER GHE WITH UPTURN
	{ 0xA6, 0x00A6 }, // BROKEN BAR
	{ 0xA7, 0x00A7 }, // SECTION SIGN
	{ 0xA8, 0x0401 }, // CYRILLIC CAPITAL LETTER IO
	{ 0xA9, 0x00A9 }, // COPYRIGHT SIGN
	{ 0xAA, 0x0404 }, // CYRILLIC CAPITAL LETTER UKRAINIAN IE
	{ 0xAB, 0x00AB }, // LEFT-POINTING DOUBLE ANGLE QUOTATION MARK
	{ 0xAC, 0x00AC }, // NOT SIGN
	{ 0xAD, 0x00AD }, // SOFT HYPHEN
	{ 0xAE, 0x00AE }, // REGISTERED SIGN
	{ 0xAF, 0x0407 }, // CYRILLIC CAPITAL LETTER YI
	{ 0xB0, 0x00B0 }, // DEGREE SIGN
	{ 0xB1, 0x00B1 }, // PLUS-MINUS SIGN
	{ 0xB2, 0x0406 }, // CYRILLIC CAPITAL LETTER BYELORUSSIAN-UKRAINIAN I
	{ 0xB3, 0x0456 }, // CYRILLIC SMALL LETTER BYELORUSSIAN-UKRAINIAN I
	{ 0xB4, 0x0491 }, // CYRILLIC SMALL LETTER GHE WITH UPTURN
	{ 0xB5, 0x00B5 }, // MICRO SIGN
	{ 0xB6, 0x00B6 }, // PILCROW SIGN
	{ 0xB7, 0x00B7 }, // MIDDLE DOT
	{ 0xB8, 0x0451 }, // CYRILLIC SMALL LETTER IO
	{ 0xB9, 0x2116 }, // NUMERO SIGN
	{ 0xBA, 0x0454 }, // CYRILLIC SMALL LETTER UKRAINIAN IE
	{ 0xBB, 0x00BB }, // RIGHT-POINTING DOUBLE ANGLE QUOTATION MARK
	{ 0xBC, 0x0458 }, // CYRILLIC SMALL LETTER JE
	{ 0xBD, 0x0405 }, // CYRILLIC CAPITAL LETTER DZE
	{ 0xBE, 0x0455 }, // CYRILLIC SMALL LETTER DZE
	{ 0xBF, 0x0457 } // CYRILLIC SMALL LETTER YI
};

static void convert_windows1251_to_utf8(const char* str, char* res)
{
	static const short utf[256] =
	{
		0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0xa, 0xb, 0xc, 0xd, 0xe, 0xf, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16,
		0x17, 0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f, 0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26,
		0x27, 0x28, 0x29, 0x2a, 0x2b, 0x2c, 0x2d, 0x2e, 0x2f, 0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36,
		0x37, 0x38, 0x39, 0x3a, 0x3b, 0x3c, 0x3d, 0x3e, 0x3f, 0x40, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46,
		0x47, 0x48, 0x49, 0x4a, 0x4b, 0x4c, 0x4d, 0x4e, 0x4f, 0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56,
		0x57, 0x58, 0x59, 0x5a, 0x5b, 0x5c, 0x5d, 0x5e, 0x5f, 0x60, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66,
		0x67, 0x68, 0x69, 0x6a, 0x6b, 0x6c, 0x6d, 0x6e, 0x6f, 0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76,
		0x77, 0x78, 0x79, 0x7a, 0x7b, 0x7c, 0x7d, 0x7e, 0x7f, 0x402, 0x403, 0x201a, 0x453, 0x201e,
		0x2026, 0x2020, 0x2021, 0x20ac, 0x2030, 0x409, 0x2039, 0x40a, 0x40c, 0x40b, 0x40f, 0x452,
		0x2018, 0x2019, 0x201c, 0x201d, 0x2022, 0x2013, 0x2014, 0, 0x2122, 0x459, 0x203a, 0x45a,
		0x45c, 0x45b, 0x45f, 0xa0, 0x40e, 0x45e, 0x408, 0xa4, 0x490, 0xa6, 0xa7, 0x401, 0xa9, 0x404,
		0xab, 0xac, 0xad, 0xae, 0x407, 0xb0, 0xb1, 0x406, 0x456, 0x491, 0xb5, 0xb6, 0xb7, 0x451,
		0x2116, 0x454, 0xbb, 0x458, 0x405, 0x455, 0x457, 0x410, 0x411, 0x412, 0x413, 0x414, 0x415,
		0x416, 0x417, 0x418, 0x419, 0x41a, 0x41b, 0x41c, 0x41d, 0x41e, 0x41f, 0x420, 0x421, 0x422,
		0x423, 0x424, 0x425, 0x426, 0x427, 0x428, 0x429, 0x42a, 0x42b, 0x42c, 0x42d, 0x42e, 0x42f,
		0x430, 0x431, 0x432, 0x433, 0x434, 0x435, 0x436, 0x437, 0x438, 0x439, 0x43a, 0x43b, 0x43c,
		0x43d, 0x43e, 0x43f, 0x440, 0x441, 0x442, 0x443, 0x444, 0x445, 0x446, 0x447, 0x448, 0x449,
		0x44a, 0x44b, 0x44c, 0x44d, 0x44e, 0x44f
	};

	int cnt = strlen(str),
		i = 0, j = 0;
	for (; i < cnt; ++i)
	{
		unsigned short c = utf[(unsigned char)str[i]];
		if (c < 0x80)
		{
			res[j++] = c;
		}
		else if (c < 0x800)
		{
			res[j++] = c >> 6 | 0xc0;
			res[j++] = ((c)& 0x3f) | 0x80;
		}
	}
	res[j] = '\0';
}


// https://github.com/svininykh/convert-utf8-to-cp1251
static int convert_utf8_to_windows1251(const char* utf8, char* windows1251, size_t n)
{
	int i = 0;
	int j = 0;
	for (; i < (int)n && utf8[i] != 0; ++i)
	{
		char prefix = utf8[i];
		char suffix = utf8[i + 1];
		if ((prefix & 0x80) == 0)
		{
			windows1251[j] = (char)prefix;
			++j;
		}
		else if ((~prefix) & 0x20)
		{
			int first5bit = prefix & 0x1F;
			first5bit <<= 6;
			int sec6bit = suffix & 0x3F;
			int unicode_char = first5bit + sec6bit;


			if (unicode_char >= 0x410 && unicode_char <= 0x44F)
			{
				windows1251[j] = (char)(unicode_char - 0x350);
			}
			else if (unicode_char >= 0x80 && unicode_char <= 0xFF)
			{
				windows1251[j] = (char)(unicode_char);
			}
			else if (unicode_char >= 0x402 && unicode_char <= 0x403)
			{
				windows1251[j] = (char)(unicode_char - 0x382);
			}
			else
			{
				int count = sizeof(g_letters) / sizeof(Letter);
				for (int k = 0; k < count; ++k)
				{
					if (unicode_char == g_letters[k].unicode)
					{
						windows1251[j] = g_letters[k].win1251;
						goto NEXT_LETTER;
					}
				}
				// can't convert this char
				return 0;
			}
		NEXT_LETTER:
			++i;
			++j;
		}
		else
		{
			// can't convert this chars
			return 0;
		}
	}
	windows1251[j] = 0;
	return 1;
}