These are not directly used when compiling the binary, but are here for re-use
or modification. Some files are missing!

This is how they are converted for binary use:
- Every 32-bit RGB pixel is converted to an 8-bit palette index value.
  This is not a fully automated process, as you need to use your eyes
  (and brain) to find out what RGB values are converted to what value. This is
  because the palette is not the same for all BMP files, to make it easier on
  the contrast and stuff like that. Some palette entries share the same color.
- These palette index bytes are given appropriate constant names
  (found in ft2_palette.h) and the array is converted to a .h file for
  inclusion in the code, and is then directly read by GUI functions.
 
This makes modification all but a smooth process, and I'm sorry for that.
I ought to have every single graphics file stored as BMP, then have a custom
program that is run on compile time to convert every BMPs to .h files
accordingly. Maybe one day, though this means that the custom program would
interfere with portability.
  
Please read LICENSE.txt if you plan on using these...