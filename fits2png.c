#include <string.h>
#include <stdio.h>
#include <math.h>
#include "fitsio.h"
#include <byteswap.h>

#include <png.h>


#define IDIM 2
#define max_memory 1024*1024*1024

#ifndef PNG_DEBUG
#  define PNG_DEBUG 2 
#endif

#if PNG_DEBUG > 1
#  define png_debug(m)        ((void)fprintf(stderr, m "\n"))
#  define png_debug1(m,p1)    ((void)fprintf(stderr, m "\n", p1))
#  define png_debug2(m,p1,p2) ((void)fprintf(stderr, m "\n", p1, p2))
#else
#  define png_debug(m)        ((void)0)
#  define png_debug1(m,p1)    ((void)0)
#  define png_debug2(m,p1,p2) ((void)0)
#endif


void *getImageToArray(fitsfile *fptr, int *dims, double *cens, 
		      int *odim1, int *odim2, int *bitpix, int *status);

int writeImage(char* filename, long width, long height, short *buffer, 
               char* title);

inline void setRGB(png_byte *ptr, float val);


int main(int argc, char *argv[])
{
   fitsfile *fptr;   /* FITS file pointer, defined in fitsio.h */
   int status = 0;   /* CFITSIO status value MUST be initialized to zero! */
   int bitpix, naxis, odim1, odim2;
   long naxes[2] = {1,1}, fpixel[2] = {1,1};
   double *pixels;
   char format[20], hdformat[20];
   void *buf;

   if (argc != 2) {
      printf("Usage: fitspng <fits file> \n");
      printf("\n");
      return(0);
    }

    if (!fits_open_file(&fptr, argv[1], READONLY, &status)) {
        if (!fits_get_img_param(fptr, 2, &bitpix, &naxis, naxes, &status) ){
          if (naxis > 2 || naxis == 0)
             printf("Error: only 1D or 2D images are supported\n");
          else {
			png_debug1("bitpix: %d", bitpix);
            buf = getImageToArray(fptr, /*dims*/ NULL, NULL, &odim1, &odim2, &bitpix,
                                  &status);
		    
			if (status) fits_report_error(stderr, status);
			int res;
			printf("bitpix: %d \n", bitpix);
			printf("odim1: %d  odim2: %d \n", odim1, odim2);
			res = writeImage("test2.png", odim1, odim2, buf,
				       "testimage");
          }
          fits_close_file(fptr, &status);
        } 
    }
    if (status) fits_report_error(stderr, status); 
    return(status);
} 

/*
 *
 *     Read a rectangular subimage (or the whole image) from the FITS data array. The fpixel and lpixel arrays give the coordinates of the first (lower left corner) and last (upper right corner) pixels to be read from the FITS image. Undefined FITS array elements will be returned with a value = *nullval, (note that this parameter gives the address of the null value, not the null value itself) unless nulval = 0 or *nulval = 0, in which case no checks for undefined pixels will be performed. 
 *
 *       int fits_read_subset / ffgsv
 *             (fitsfile *fptr, int  datatype, long *fpixel, long *lpixel, long *inc,
 *                    DTYPE *nulval, > DTYPE *array, int *anynul, int *status)
 *
 */

// getImageToArray: extract a sub-section from an image HDU, return array
// credit: Eric Mandel js9
void *getImageToArray(fitsfile *fptr, int *dims, double *cens, 
		      int *odim1, int *odim2, int *bitpix, int *status){
  int naxis=IDIM;
  int xcen, ycen, dim1, dim2, type;
  void *obuf;
  long totpix, totbytes;
  long fpixel[IDIM], lpixel[IDIM], inc[IDIM]={1,1};
  long naxes[IDIM];

  // get image dimensions and type
  fits_get_img_dim(fptr, &naxis, status);
  fits_get_img_size(fptr, IDIM, naxes, status);
  fits_get_img_type(fptr, bitpix, status);

  printf("naxes[0]: %ld naxes[1]: %ld\n");

  // get limits of extracted section
  if( dims && dims[0] && dims[1] ){
    png_debug("getting limits of extracted section\n");
    dim1 = fmin(dims[0], naxes[0]);
    dim2 = fmin(dims[1], naxes[1]);
    // read image section
    if( cens ){
      xcen = cens[0];
      ycen = cens[1];
    } else {
      xcen = dim1/2;
      ycen = dim2/2;
    }
    fpixel[0] = (int)(xcen - ((dim1+1)/2) + 1);
    fpixel[1] = (int)(ycen - ((dim2+1)/2) + 1);
    lpixel[0] = (int)(xcen + (dim1/2));
    lpixel[1] = (int)(ycen + (dim2/2));
  } else {
    // read entire image
    png_debug("reading entire image\n");
    fpixel[0] = 1;
    fpixel[1] = 1;
    lpixel[0] = naxes[0];
    lpixel[1] = naxes[1];
  }
  if( fpixel[0] < 1 ){
    fpixel[0] = 1;
  }
  if( fpixel[0] > naxes[0] ){
    fpixel[0] = naxes[0];
  }
  if( fpixel[1] < 1 ){
    fpixel[1] = 1;
  }
  if( fpixel[1] > naxes[1] ){
    fpixel[1] = naxes[1];
  }
  // section dimensions
  *odim1 = lpixel[0] - fpixel[0] + 1;
  *odim2 = lpixel[1] - fpixel[1] + 1;
  totpix = *odim1 * *odim2;
  // allocate space for the pixel array
  switch(*bitpix){
    case 8:
      type = TBYTE;
      totbytes = totpix * sizeof(char);
      break;
    case 16:
      type = TSHORT;
      totbytes = totpix * sizeof(short);
      break;
    case -16:
      type = TUSHORT;
      totbytes = totpix * sizeof(unsigned short);
      break;
    case 32:
      type = TINT;
      totbytes = totpix * sizeof(int);
      break;
    case 64:
      type = TLONGLONG;
      totbytes = totpix * sizeof(long long);
      break;
    case -32:
      type = TFLOAT;
      totbytes = totpix * sizeof(float);
      break;
    case -64:
      type = TDOUBLE;
      totbytes = totpix * sizeof(double);
      break;
  default:
    return NULL;
  }
  // sanity check on memory limits
  if( totbytes > max_memory ){
    return NULL;
  }
  // try to allocate that much memory
  if(!(obuf = (void *)malloc(totbytes))){
    return NULL;
  }
  /* read the image section */
  fits_read_subset(fptr, type, fpixel, lpixel, inc, 0, obuf, 0, status);
  // return pixel buffer (and section dimensions)
  return obuf;
}

int writeImage(char* filename, long width, long height, short *buffer, char* title)
{
   int code = 0;
   FILE *fp = NULL;
   png_structp png_ptr = NULL;
   png_infop info_ptr = NULL;
   png_bytep row = NULL;

// Open file for writing (binary mode)
   png_debug1("filename: %s\n", filename);
   fp = fopen(filename, "wb");
   if (fp == NULL) {
      fprintf(stderr, "Could not open file %s for writing\n", filename);
      code = 1;
      goto finalise;
   }



   // Initialize write structure
   png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
   if (png_ptr == NULL) {
      fprintf(stderr, "Could not allocate write struct\n");
      code = 1;
      goto finalise;
   }

   // Initialize info structure
   info_ptr = png_create_info_struct(png_ptr);
   if (info_ptr == NULL) {
      fprintf(stderr, "Could not allocate info struct\n");
      code = 1;
      goto finalise;
   }

  // Setup Exception handling
   if (setjmp(png_jmpbuf(png_ptr))) {
      fprintf(stderr, "Error during png creation\n");
      code = 1;
      goto finalise;
   }

   png_init_io(png_ptr, fp);
   // Write header (8 bit colour depth)
   printf("png_set_IHDR\n");
   printf("width=%d height=%d\n", width, height);
   png_set_IHDR(png_ptr, info_ptr, width, height,
         16, PNG_COLOR_TYPE_GRAY, PNG_INTERLACE_NONE,
         PNG_COMPRESSION_TYPE_BASE, PNG_FILTER_TYPE_BASE);

   // Set title
   printf("title\n");
   if (title != NULL) {
      png_text title_text;
      title_text.compression = PNG_TEXT_COMPRESSION_NONE;
      title_text.key = "Title";
      title_text.text = title;
      png_set_text(png_ptr, info_ptr, &title_text, 1);
   }

   printf("png_write_info\n");
   png_write_info(png_ptr, info_ptr);

   png_debug("Malloc\n");
// Allocate memory for one row (3 bytes per pixel - RGB)
   //row = (png_bytep) malloc(width * sizeof(png_byte));
   row = (png_bytep) calloc(width*2 , sizeof(png_byte));

	printf("write image data\n");
	printf("buffer[0]=%d\n", buffer[0]);
	printf("buffer[1]=%d\n", buffer[1]);
	printf("buffer[width:%d]=%d\n", width, buffer[width]);

   // Write image data
   long x, y, pos;
   for (y=height ; y>=0  ; y--) {
	  pos = 0;	
      for (x=0 ; x<width-2 ; x++) {
		//printf("x=%d ", x);
		//row[x]  = buffer[y*width + x];
		row[pos++]  = buffer[y*width+x] >> 8 ;
		row[pos++]  = buffer[y*width+x] & 0xFF ;
		if(row[x]==1701) { printf("row x = 1701 x=%d, y=%d, value=1701\n", x, y); }
        //setRGB(&(row[x]), buffer[y*width + x]);
		if(y==0) printf("row[x]:%d ",row[x]);
      }
	  printf("y:%d\n", y);
      png_write_row(png_ptr, row);
   }

	printf("png_write_end\n");
   // End write
   png_write_end(png_ptr, NULL);

   finalise:
   if (fp != NULL) fclose(fp);
   if (info_ptr != NULL) png_free_data(png_ptr, info_ptr, PNG_FREE_ALL, -1);
   if (png_ptr != NULL) png_destroy_write_struct(&png_ptr, (png_infopp)NULL);
   if (row != NULL) free(row);

   return code;
}

inline void setRGB(png_byte *ptr, float val)
{
	int v = (int)(val * 767);
	if (v < 0) v = 0;
	if (v > 767) v = 767;
	int offset = v % 256;

	if (v<256) {
		ptr[0] = 0; ptr[1] = 0; ptr[2] = offset;
	}
	else if (v<512) {
		ptr[0] = 0; ptr[1] = offset; ptr[2] = 255-offset;
	}
	else {
		ptr[0] = offset; ptr[1] = 255-offset; ptr[2] = 0;
	}
}

