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

std::string nrrd_type(uint16_t bits_per_sample, uint16_t sampleformat)
{
  switch(sampleformat) {
    case SAMPLEFORMAT_UINT:
      switch(bits_per_sample) {
        case 8: return "uint8";
        case 16: return "uint16";
        case 32: return "uint32";
        case 64: return "uint64";
      }
      break;
    case SAMPLEFORMAT_INT:
      switch(bits_per_sample) {
        case 8: return "int8";
        case 16: return "int16";
        case 32: return "int32";
        case 64: return "int64";
      }
      break;
    case SAMPLEFORMAT_IEEEFP:
      switch(bits_per_sample) {
        case 32: return "float";
        case 64: return "double";
      }
      break;
    /* don't bother handling void, complex numbers, we don't know what to do
     * with them anyway. */
    case SAMPLEFORMAT_COMPLEXINT:
      break;
    case SAMPLEFORMAT_COMPLEXIEEEFP:
      break;
  }
  return "unknown";
}

int main(int argc, char *argv[])
{
  if(argc != 4) {
    std::cerr << "Usage: " << argv[0] << " in.tiff out nhdr\n";
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

#ifndef NDEBUG
  TIFFPrintDirectory(tif.get(), stdout, TIFFPRINT_CURVES | TIFFPRINT_COLORMAP);
#endif

  std::ofstream out(argv[2], std::ios::out | std::ios::binary);
  if(!out) {
    std::cerr << "Could not open " << argv[2] << " for writing.\n";
    return EXIT_FAILURE;
  }

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

  std::ofstream nhdr(argv[3], std::ios::out);
  if(!nhdr) {
    std::cerr << "Could not open '" << argv[3] << "' to create header.\n";
    return EXIT_FAILURE;
  }
  nhdr << "NRRD0002\n"
       << "dimension: 3\n"
       << "sizes: " << dims[0] << " " << dims[1] << " " << dims[2] << "\n"
       << "type: " << nrrd_type(bits_sample, sf) << "\n"
       << "encoding: raw\n"
       << "data file: " << argv[2] << "\n";
  nhdr.close();

  return EXIT_SUCCESS;
}
