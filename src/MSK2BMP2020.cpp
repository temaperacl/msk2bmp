// 
#include <iostream>
#include <fstream>
#include <list>
#include <memory>



bool IsBMPFile(std::ifstream& file)
{
    // Super lazy - is the first character a "B"?
    // False positives for MSK files that have this as the first set of bits
    int firstChar = file.peek();
    return (0x42 == firstChar); // B
}

void ReadMskLines(std::ifstream& file, std::list<std::unique_ptr<char[]>>& vOutput)
{
    char linedata[44];
    file.seekg(std::ios_base::beg);
    do {
        // Each line in a Msk file is 44 bytes. Obviously, this isn't flexible if a different use case is needed
        // Although it is important to note that the MSK file doesn't have any header information and so any
        // line-by-line processing will have to figure out (or be told) the length of the line.
        file.read(linedata, 44);
        if (!file.eof())
        {
            // Push any read lines to end of the output list.
            vOutput.push_back(std::make_unique<char[]>(44));
            memcpy_s(vOutput.back().get(), 44, linedata, 44);
        }
    } while (!file.eof());
}

// Ensure Little Endian Interpretation - I think there is a built-in for this,
// But too lazy to look it up.
int BytesToInt(char* C, int numBytes = 4)
{
    int ReturnVal = 0;
    for (int rOff = 1; rOff <= numBytes; ++rOff)
    {
        ReturnVal *= 256;
        unsigned char subC = C[numBytes - rOff];
        
        ReturnVal += subC;
    }
    return ReturnVal;
}


bool ReadBmpLines(std::ifstream& file, std::list<std::unique_ptr<char[]>>& vOutput)
{
    // Note that this is a simple parsing. For more complicated work, maybe 
    // Hook into a proper image parsing library.
    char bmpHeader[18];
    file.read(bmpHeader, 18);
    if ('B' != bmpHeader[0] || 'M' != bmpHeader[1])
    {
        std::cout << "[ERROR] Input file Lacks Bitmap Signature";
        return false;
    }
    int ImageDataOffset = BytesToInt(bmpHeader + 0x0A);

    int HeaderSize = BytesToInt(bmpHeader + 0x0E);
    char bmpSubHeader[128]; // 
    file.read(bmpSubHeader, ((HeaderSize > 132) ? 128 : (HeaderSize - 4)));
    int BitmapWidth, BitmapHeight, BitsPerPixel;
    int Compression = 0;
    if (HeaderSize == 12)
    {
        BitmapWidth = BytesToInt(bmpSubHeader, 2);
        BitmapHeight = BytesToInt(bmpSubHeader + 2, 2);
        BitsPerPixel = BytesToInt(bmpSubHeader + 6, 2);
    }
    else {
        BitmapWidth = BytesToInt(bmpSubHeader);
        BitmapHeight = BytesToInt(bmpSubHeader + 4);
        BitsPerPixel = BytesToInt(bmpSubHeader + 10, 2);
        Compression = BytesToInt(bmpSubHeader + 12);
    }

    // Sanity Checks - All of these could be dealt with either
    // With better logic or with an image library.
    if (300 != BitmapHeight || 350 != BitmapWidth)
    {
        std::cout << "[ERROR] Expecting 350x300 Bitmap (" << BitmapWidth <<"x" <<BitmapHeight<<" seen)" << std::endl;
        return false;
    }
    if (0 != Compression && 3 != Compression && 0x0B != Compression)
    {
        std::cout << "[ERROR] Only uncompressed Bitmaps supported" << std::endl;
        return false;
    }
    if (1 != BitsPerPixel)
    {
        std::cout << "only 2-color bitmaps supported" << std::endl;
        return false;
    }
    char linedata[44];
    file.seekg(ImageDataOffset);
    do {
        // Each line in a Msk file (or uncompressed 350-pixel 1-bit-per-pixel bmp) is 44 bytes. Obviously, this isn't flexible if a different use case is needed
        // Although it is important to note that the MSK file doesn't have any header information and so any
        // line-by-line processing will have to figure out (or be told) the length of the line.
        file.read(linedata, 44);
        if (!file.eof())
        {
            // Push any read lines to end of the output list.
            vOutput.push_back(std::make_unique<char[]>(44));
            memcpy_s(vOutput.back().get(), 44, linedata, 44);
        }
    } while (!file.eof());

    return true;
}

void WriteLinesToMsk(std::ostream& file, std::list<std::unique_ptr<char[]>>& vOutput)
{
    for (std::list<std::unique_ptr<char[]>>::iterator iLine = vOutput.begin(); vOutput.end() != iLine; ++iLine)
    {
        file.write(iLine->get(), 44);
    }
}
void WriteLinesToBmp(std::ostream& file, std::list<std::unique_ptr<char[]>>& vOutput)
{

//Disable the integer conversion warnings for this header definition
#pragma warning(push)
#pragma warning(disable: 4309)
#pragma warning(disable: 4838)
    // Windows BITMAPINFOHEADER format, for historical reasons
    char bmpHeader[62] = {
        // Offset 0x00000000 to 0x00000061
        0x42, 0x4D, // 'BM'
        0xCE, 0x33, 0x00, 0x00, //Size in bytes
        0x00, 0x00, 0x00, 0x00,
        0x3E, 0x00, 0x00, 0x00, // Starting Address of pixels
        // Windows BITMAPINFOHEADER as per original
        0x28, 0x00, 0x00, 0x00, // Size of header
        0x5E, 0x01, 0x00, 0x00, // Bitmap Width  (350px)
        0x2C, 0x01, 0x00, 0x00, // Bitmap Height (300px)
        0x01, 0x00,             // Number of color planes
        0x01, 0x00,             // Bits per pixel
        0x00, 0x00, 0x00, 0x00, // Compression/ (1 = none)
        0x90, 0x33, 0x00, 0x00, // Image size (raw bitmap data [300x44])
        0xC4, 0x0E, 0x00, 0x00, // Horizotal Resolution
        0xC4, 0x0E, 0x00, 0x00, // Vertical Resolution
        0x00, 0x00, 0x00, 0x00, // Number of colors (0 = 2^n)
        0x00, 0x00, 0x00, 0x00, // Number of important colors used
        // Color Table, RGBA
        0x00, 0x00, 0x00, 0x00, // 0
        0xFF, 0xFF, 0xFF, 0x00  // 1
    };
#pragma warning(pop)


    file.write(bmpHeader, 62);
    for (std::list<std::unique_ptr<char[]>>::iterator iLine = vOutput.begin(); vOutput.end() != iLine; ++iLine)
    {
        file.write(iLine->get(), 44);
    }
}



int main(int ArgC, const char ** ArgV)
{
    if ( ArgC < 2) {
        std::cout << "Input File Required";
    }
    bool bFlipVertical = true;

    const char* szFileName = ArgV[1];
    std::ifstream infile(szFileName, std::ios::in | std::ios::binary);
    if (!infile.is_open())
    {
        std::cout << "Unable to open file";
    }

    // Lines of bits. Essentially the raw MSK file.
    std::list<std::unique_ptr<char[]>> inputLines;
    // Extension for output file.
    std::string TargetExtension("");

    // Load Input File
    bool bBMP2MSK = IsBMPFile(infile);
    if (bBMP2MSK)
    {
        if (!ReadBmpLines(infile, inputLines))
        {
            std::cout << "Unable to read BMP file" << std::endl;
            infile.close();
            return 1;
        }
        TargetExtension = "MSK";
    }
    else {
        ReadMskLines(infile, inputLines);
        TargetExtension = "BMP";
    }

    infile.close();

    // Change format if needed
    // MSK files are top-left origin. BMP files are bottom-left origin.
    // (Some BMP files are top-left origin. These ones will end up
    //  flipping the image until they are addressed in ReadBmpLines)
    if (bFlipVertical)
    {
        inputLines.reverse();
    }

    // Open Output file
    std::string sOutputFileName(szFileName);
    sOutputFileName.replace(sOutputFileName.length() - 3, 3, TargetExtension);
    
    std::filebuf fb;
    fb.open(sOutputFileName.c_str(), std::ios::out | std::ios::binary | std::ios::trunc);
    std::ostream outfile(&fb);

    // Write Output File.
    if (bBMP2MSK)
    {
        WriteLinesToMsk(outfile, inputLines);
    }
    else {
        WriteLinesToBmp(outfile, inputLines);
    }
    fb.close();

}

/* ***************************
 * MSK2BMP2020
 * 2020-12-28 - v1 - Daniel Sears (Temaperacl)
 * 
 * This file is released into the public domain and with no claim of copyright
 * by the author(s).
 * 
 * 
 * If in a location where copright cannot be disclaimed or in case public domain
 * software is unusable (I've seen this in corporate settings, but if you are using
 * this in a corporate setting, you may want to consider if that is really the right
 * option), then license is granted under the following terms:
 * 
 * MIT License
 *
 * Copyright (c) 2020 Daniel Sears
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 * 
 *************************** */