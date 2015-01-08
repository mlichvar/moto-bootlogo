#include <iomanip>
#include <iostream>
#include <png.h>
#include <sstream>
#include <stdio.h>
#include <string.h>

#include "BinImage.h"

#define PNG_DEBUG 3

BinImage::BinImage(char *data, size_t size)
{
    // check if the image data starts with 'MotoRun'
    if (strcmp(data, "MotoRun")) {
        cerr << "Could not find valid image data" << endl;
        return;
    }
    _data = new uint8_t[size];
    memcpy(_data, data, size);
    _size = size;
}

BinImage::~BinImage()
{
    delete[] _data;
    _data = NULL;
    if (_raw) {
        delete _raw;
        _raw = NULL;
    }
}

void BinImage::create_png(string filename)
{
    // skip 8 bytes (MotoRun )
    _g = 8;

    // read image dimensions
    _width = _read_dimension();
    _height = _read_dimension();

    if (!_width || !_height) {
        cerr << "Could not determine image size" << endl;
        return;
    }
    cout << "Image size: " << _width << " x " << _height << endl;

    _decode();
    if (!_raw) {
        cerr << "Failed decoding binary image" << endl;
        return;
    }

    FILE *fp = fopen(filename.c_str(), "wb");
    if (!fp) {
        cerr << "Could not open file for writing PNG image" << endl;
        return;
    }

    // allocate & initialize PNG struct
    png_structp pngStruct = png_create_write_struct(
            PNG_LIBPNG_VER_STRING, (png_voidp)NULL, NULL, NULL);
    if (!pngStruct) {
        cerr << "[PNG] Failed creating structure" << endl;
        return;
    }

    // allocate & initialize PNG info
    png_infop pngInfo = png_create_info_struct(pngStruct);
    if (!pngInfo) {
        cerr << "[PNG] Failed creating info" << endl;
        png_destroy_write_struct(&pngStruct, (png_infopp)NULL);
        fclose(fp);
        return;
    }

    // set-up error handling
    if (setjmp(png_jmpbuf(pngStruct))) {
        cerr << "[PNG] Failed writing file" << endl;
        png_destroy_write_struct(&pngStruct, &pngInfo);
        fclose(fp);
        return;
    }

    // initialize output
    png_init_io(pngStruct, fp);

    // write headers
    png_set_IHDR(pngStruct, pngInfo, _width, _height,
            8, PNG_COLOR_TYPE_RGB, PNG_INTERLACE_NONE,
            PNG_COMPRESSION_TYPE_BASE, PNG_FILTER_TYPE_BASE);

    // write data
    fseek(_raw, 0, SEEK_SET);
    png_byte buffer[3 * _width * _height];
    fread(buffer, sizeof(png_byte), 3 * _width * _height, _raw);
    png_byte *pngRows[_height];
    for (size_t k = 0; k < _height; k++) {
        pngRows[k] = &buffer[k * _width * 3];
    }
    png_set_rows(pngStruct, pngInfo, pngRows);
    png_write_png(pngStruct, pngInfo, PNG_TRANSFORM_BGR, NULL);

    // cleanup
    png_destroy_write_struct(&pngStruct, &pngInfo);
    fclose(fp);
    delete _raw;
}

void BinImage::_decode(void)
{
    _raw = tmpfile();
    while (_g < _size) {
        uint8_t byte = _data[_g++];
        if ((byte & 0x70) > 0) {
            // should not be reached
            cerr << "Unexpected run-length value: " << (int)byte << endl;
        } else {
            uint16_t runlength = 0;
            runlength += (byte & 0x0f) << 8;
            runlength += _data[_g++];

            // Repeat RGB value L times (1 color)
            // 0x8L LL R1 G1 B1
            //  (or)
            // Read L RGB values (L colors)
            // 0x0L LL R1 G1 B1 R2 G2 B2 R3 G3 B3 ...
            bool repeat = byte >> 7;

            for (uint16_t i = 0; i < runlength; i++) {
                fwrite(&_data[_g], sizeof(uint8_t), 3, _raw);
                if (!repeat) {
                    _g += 3;
                }
            }
            if (repeat) {
                _g += 3;
            }
        }
    }
    fflush(_raw);

    if (_g != _size) {
        cerr << "Incorrect file size (" << _g << "/" << _size << ")" << endl;
        delete _raw;
        _raw = NULL;
    }
}

size_t BinImage::_read_dimension(void)
{
    uint8_t bytes[2];
    bytes[0] = _data[_g++];
    bytes[1] = _data[_g++];
    uint16_t value = 0;
    for (int i=0; i<2; i++) {
        value += bytes[i] << 8 * (1 - i);
    }
    return value;
}
