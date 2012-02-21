#include <cstdlib>
#include <fstream>
#include <iostream>
#include <memory>

#include <tiffio.h>

#ifdef __GNUC__
# define _malloc __attribute__((malloc))
#else
# define _malloc /* nothing */
#endif

// Reads the dimensions of the TIFF volume.  X and Y come from the dimensions
// of the first image in the stack: we assume that this stays constant
// throughout the volume.  Z comes from the number of images in the stack.
static void
tv_dimensions(TIFF *tif, size_t dims[3])
{
  uint32_t x,y;
  size_t z=0;

  TIFFSetDirectory(tif, 0);
  TIFFGetField(tif, TIFFTAG_IMAGEWIDTH, &x);
  // tiff calls the height "length" for some reason.
  TIFFGetField(tif, TIFFTAG_IMAGELENGTH, &y);
  do {
    uint32_t tmpx, tmpy;
    TIFFGetField(tif, TIFFTAG_IMAGEWIDTH, &tmpx);
    TIFFGetField(tif, TIFFTAG_IMAGELENGTH, &tmpy);
    if(tmpx != x) { std::clog << "TIFF x dimension changes in stack!\n"; }
    if(tmpy != y) { std::clog << "TIFF y dimension changes in stack!\n"; }
    ++z;
  } while(TIFFReadDirectory(tif));
  TIFFSetDirectory(tif, 0);

  dims[0] = x;
  dims[1] = y;
  dims[2] = z;
}

struct tiffclose_deleter {
  void operator()(TIFF* t) const {
    std::clog << "Closing tiff " << t << "\n";
    TIFFClose(t);
  }
};

std::string sampleformat(uint16_t sf) {
  switch(sf) {
    case SAMPLEFORMAT_UINT: return "unsigned integer";
    case SAMPLEFORMAT_INT: return "integer";
    case SAMPLEFORMAT_IEEEFP: return "floating point";
    case SAMPLEFORMAT_VOID: return "void";
    case SAMPLEFORMAT_COMPLEXINT: return "complex integer";
    case SAMPLEFORMAT_COMPLEXIEEEFP: return "complex floating point";
  }
  return "unknown!";
}

int main(int argc, char *argv[])
{
  if(argc != 3) {
    std::cerr << "Usage: " << argv[0] << " in.tiff out\n";
    return EXIT_FAILURE;
  }

  
  std::unique_ptr<TIFF, tiffclose_deleter> tif(TIFFOpen(argv[1], "r"));
  if(tif == NULL) {
    std::cerr << "cannot open tiff '" << argv[1] << "'\n";
    return EXIT_FAILURE;
  }

  size_t dims[3] = {42,42,42};
  tv_dimensions(tif.get(), dims);
  std::clog << dims[0] << "x" << dims[1] << "x" << dims[2] << " tiff.\n";

  uint16_t bits_sample = 42;
  TIFFGetField(tif.get(), TIFFTAG_BITSPERSAMPLE, &bits_sample);
  std::clog << bits_sample << " bits per sample.\n";

  uint16_t n_components = 42;
  TIFFGetField(tif.get(), TIFFTAG_SAMPLESPERPIXEL, &n_components);
  std::clog << n_components << "-component data.\n";

  uint16_t sf = 42;
  if(TIFFGetField(tif.get(), TIFFTAG_SAMPLEFORMAT, &sf) == 0) {
    std::clog << "Sample format not defined in file.  Assuming uint.\n";
    sf = 1;
  }
  std::clog << "data type: " << sampleformat(sf) << "(" << sf << ")\n";

  TIFFPrintDirectory(tif.get(), stdout, TIFFPRINT_CURVES | TIFFPRINT_COLORMAP);

  std::ofstream out(argv[2], std::ios::out | std::ios::binary);

  TIFFSetDirectory(tif.get(), 0);
  do {
    tsize_t sl_size = TIFFScanlineSize(tif.get());
    tdata_t buf = _TIFFmalloc(sl_size * bits_sample/8 * n_components);
    for(size_t row = 0; row < dims[1]; ++row) { // foreach scanline
      TIFFReadScanline(tif.get(), buf, row); // read it
      out.write(static_cast<const char*>(buf), sl_size);
    }
    _TIFFfree(buf);
  } while(TIFFReadDirectory(tif.get()));

  out.close();
}
