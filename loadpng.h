# include <stdio.h>
# include <stdlib.h>
# include <stdint.h>
# include "png.h" // This Also Includes The <zlib.h> function

# ifndef  _LITYPES 
    # include "image_types.h"
# endif

// Definitions
# define DEFAULT_BACKGROUND_COLOR 0xFFFF
# define BAD_SIG_ERROR 1
# define OUT_OF_MEM_ERROR 4
# define PNG_READ_ERROR 2
# define PNG_ILLEGAL_BIT_DEPTH_ERROR 9
# define PNG_ILLEGAL_COLOR_TYPE_ERROR 10
# define IMAGE_DOESNT_EXIST_ERROR 19


// Functions 
int loadpng(vglImageData *image, char *filename);
static int loadpng_file(vglImageData *image, FILE *file);
int unloadpng(vglImageData * image);
static int readpng_init(vglImageData *image, FILE *fp, png_structp , png_infop, int * color_type, int * bit_depth);
static int readpng_bgcolor(png_color_16p color, png_structp png_ptr, png_infop info_ptr);
static int readpng_image(vglImageData *image, png_structp png_ptr, png_infop info_ptr, const int color_type, const int bit_depth, png_color_16p background_color, int is_default_bg); 
static int readpng_cleanup(png_structp png_ptr, png_infop info_ptr);

static int log_error( char * message) {
    FILE * fp = stderr;
    fprintf(fp, "Compiled with libpng version %s; using libpng %s;\n", PNG_LIBPNG_VER_STRING, png_libpng_ver);
    fputs(message, fp);

    return 1;
}

int loadpng(vglImageData * image, char * filename) {
    FILE * fp = fopen(filename, "rb");
    return loadpng_file(image, fp);
    fclose(fp);
}

int loadpng_file(vglImageData * image, FILE * fp) {
    
    if( !image )
        return IMAGE_DOESNT_EXIST_ERROR;
    // + Checking For The 8 Byte PNG Signal 
    unsigned char sig[8];
    fread(sig, 8, 1, fp);
    if(!png_check_sig(sig, 8)) {
        log_error("Error: Loading PNG: Bad PNG SIG.\n");
        return BAD_SIG_ERROR;
    }

    // + Initializing PNG structs
    png_structp png_ptr; // png_structp is png_struct *
    png_infop info_ptr; // png_infop is png_info *
                        // there's a third struct ptr which commonly referred to as end_ptr. However we won't need it here so ...
                        // Creating A png_ptr struct. Which is basically used by `libpng` to keep track of the png image's data.
    
    png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    
    if(!png_ptr) {
        log_error("Error: Loading PNG: Out Of Memory. \n");
        return OUT_OF_MEM_ERROR;
    }
    // Creating a info_ptr struct. Which is used to indicate the state of a PNG image after performing user-requested operations.
    info_ptr = png_create_info_struct(png_ptr);
    if(!info_ptr) {
        png_destroy_read_struct(&png_ptr, NULL, NULL);
        log_error("Error: Loading PNG: Out Of Memory. \n");
        return OUT_OF_MEM_ERROR;
    } // Btw, we won't be accessing these structs directly as that can negatively impact the compability of this library in the future for multiple reasons (...).


    // + Error Checkin 
    // now for error checking, libpng uses a weird method which is setjmp() and longjmp(). basically :
    if ( setjmp(png_jmpbuf(png_ptr)) ) { // This here makes set_jmp() store the state of the program (registers...) into the jmpbuf in png_ptr. Which ideally now will return a success (0) avoiding this if statement
                                         // However, whenever PNG encounters a problem it will invoke longjmp on the same buffer which returns to this condition `by using a goto statement` and return a non-zero value
                                         // Which makes this condition re-evaluate to true and shuts down the program consequently.
        png_destroy_read_struct(&png_ptr, &info_ptr, NULL); 
        return PNG_READ_ERROR;
    } // The method specified in the book is to access the png_ptr->jmp_buf directly which is outdated now. It already specifies why it's a bad proactice wich damages the compability of our library in the future
      // Fortunately we won't have to worry about that here :).                   

    int color_type, bit_depth;
    int res;
    if((res =readpng_init(image, fp, png_ptr, info_ptr, &color_type, &bit_depth))) {
        log_error("Error: Loading PNG: Illegal Header Info \n");
        return res;
    }

    png_color_16 background_color;
    int is_def_bg = 0;
    if((is_def_bg = readpng_bgcolor(&background_color, png_ptr, info_ptr)))
        background_color.red = background_color.green = background_color.blue = background_color.gray = DEFAULT_BACKGROUND_COLOR; 
     
    if(readpng_image(image, png_ptr, info_ptr, color_type, bit_depth, &background_color, is_def_bg))
        return PNG_READ_ERROR;
    readpng_cleanup(png_ptr, info_ptr);

    # ifdef _LOAD_IMAGE_PNG_DEBUG
        printf("Width: %u. Height: %u \n", image->mip[0].width, image->mip[0].height); 
        printf("Texture Total Size: %lu \n", image->TextureTotalSize);
        printf("Color Type: %d. Bit Depth: %d \n", color_type, bit_depth);
        printf("%sUsing Default Backgtound: Red: %u. Green: %u. Blue: %u. Index[Palette]: %u. Gray[greyscale]: %u \n", is_def_bg ? " " : "Not ", background_color.red, background_color.green, background_color.blue, background_color.gray, background_color.index);
        
    # endif
    // printf("Width: %u, Height: %u.\n", image->mip[0].width, image->mip[0].height);

    return 0;
}
int unloadpng(vglImageData *image) {
    if( image ) {
        free(image->mip[0].data);
    }
    return 0;
}

// Creates The `png_ptr` and `info_ptr` structs. Also Reads The IHDR chunk in the PNG file.
static int readpng_init(vglImageData * image, FILE * fp, png_structp png_ptr, png_infop info_ptr, int * ct, int * bd) {



    // + Init Reading The PNG
    png_init_io(png_ptr, fp); // This here Initializes reading the png file and stores the data into png_ptr.
    png_set_sig_bytes(png_ptr, 8); // Lets libpng know we already checked the first 8 bytes of the png stream.
    png_read_info(png_ptr, info_ptr); // Reads up to the first IDAT chunk in the png file. Which includes 
                                      // any IHDR, PLTE chunks alongside iCCP, gAMA, cHRM, sRGB, tRNS, bKGD, tIME ...etc. You get the idea 

    int img_compressed, img_interlaced, img_filtered;
    int bit_depth, color_type;
    uint32_t width, height;
    png_get_IHDR(png_ptr, info_ptr, &width, &height, &bit_depth, &color_type, &img_interlaced, &img_compressed, &img_filtered); // Gets The PNG Imformation And Stroes It.

    // Stores Info ABout the texure
    image->slices = 0;
    image->target = GL_TEXTURE_2D;
    image->SliceStride = 0;
    image->mipmapCount = 1;
    image->mip[0].width = width;
    image->mip[0].height = height;


    if(color_type == 3) {
        image->format = GL_RGB; // A Palette Is A set of RGB values as defined by the W3C Standard Spec.
        image->type = GL_UNSIGNED_BYTE; // A Palette Has A Depth Value Of 8.
        image->internalFormat = GL_RGB8; // A Result Of The Previous Two
        bit_depth = 8; // Setting The Bit Depth.
    } else {
        bit_depth = bit_depth; // Specifying The Bit Depth 

        // The Rest Here Is Specified By The W3C PNG Standard Spec. Table 11.1 
        switch (color_type) {
            case 0x0 :
                image->format = GL_RED;
                switch (bit_depth) {
                    case 1: case 2: case 4: case 8:
                        image->type = GL_UNSIGNED_BYTE;
                        image->internalFormat = GL_R8;
                        break;
                    case 16 :
                        image->type = GL_UNSIGNED_SHORT;
                        image->internalFormat = GL_R16;
                        break;
                    default:
                        return PNG_ILLEGAL_BIT_DEPTH_ERROR;
                }
                image->TextureTotalSize = width * height * (bit_depth / 8);
                break;
            case 0x2: 
                image->format = GL_RGB;
                switch (bit_depth) {
                    case 8 : 
                        image->type = GL_UNSIGNED_BYTE;
                        image->internalFormat = GL_RGB8;
                        break;
                    case 16 :
                        image->type = GL_UNSIGNED_SHORT;
                        image->internalFormat = GL_RGB16;
                        break;
                    default:
                        return PNG_ILLEGAL_BIT_DEPTH_ERROR;
                }
                image->TextureTotalSize = width * height * (bit_depth / 8) * 3;
                break;
            case 4 :
                image->format = GL_RG;
                switch (bit_depth) {
                    case 8 : 
                        image->type = GL_UNSIGNED_BYTE;
                        image->internalFormat = GL_RG8;
                        break;
                    case 16 :
                        image->type = GL_UNSIGNED_SHORT;
                        image->internalFormat = GL_RG16;
                        break;
                    default:
                        return PNG_ILLEGAL_BIT_DEPTH_ERROR;
                }
                image->TextureTotalSize = width * height * (bit_depth / 4);
                break;
            case 6 :
                image->format = GL_RGBA;
                 switch (bit_depth) {
                    case 8 : 
                        image->type = GL_UNSIGNED_BYTE;
                        image->internalFormat = GL_RGBA8;
                        break;
                    case 16 :
                        image->type = GL_UNSIGNED_SHORT;
                        image->internalFormat = GL_RGBA16;
                        break;
                    default:
                        return PNG_ILLEGAL_BIT_DEPTH_ERROR;
                }
                break;
                image->TextureTotalSize = width * height * (bit_depth / 2);
            default: 
                 return PNG_ILLEGAL_COLOR_TYPE_ERROR;

        }

    }
    *ct = color_type;
    *bd = bit_depth;
    return 0; 
}

// Reads The background of an png image 
static int readpng_bgcolor(png_color_16p  background_color, png_structp png_ptr, png_infop info_ptr) {
    if(!png_get_valid(png_ptr,  info_ptr, PNG_INFO_bKGD))
        return PNG_READ_ERROR;

    png_get_bKGD(png_ptr,info_ptr, &background_color); // reads The bKGD structs and gets us some info 
  

    // pBackground is a pointer to a png_color_16 struct which is defined as : 
    // typedef struct png_color_16_struct
    // {
    //      png_byte index;
    //      png_uint_16 red;
    //      png_uint_16 green;
    //      png_uint_16 blue;
    //       png_uint_16 gray;
    // } png_color_16;
    // As suggested by the member's names. Some of these aren't always valid :
    //      + `index` is only valid for palette images.
    //      + `gray` is only valid for greyscale images.
    //  However, an undocumented useful liitle feature of libpng is that red, green and blue are always valid. Which are what we are seaching for :D.

    // Another thing to note is that we receive the data in a uint16 format, which means we need to do some bitwise operations to cast them into the uint8 format we specified. 
    
    return 0;
}

// Reads Image data
static int readpng_image(vglImageData * image, png_structp png_ptr, png_infop info_ptr, const int color_type, const int bit_depth, png_color_16p background_color, int is_default_bg){
    // + Error Detection 
    if( setjmp(png_jmpbuf(png_ptr)) ) {
        png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
        return PNG_READ_ERROR;
    }

    // + Gamma correction : 
    double LUT_exponent = 1.0; // Most PCs forego the LUT exponent which makes it by default 1.0
    double CRT_exponent = 2.2; // Most Displays nowadays used 2.2 as a default gamma  
    double default_display_exponent;

    // For Some cases the LUT exponent may differ that's why : 
    # if defined (NeXT) 
        LUT_exponent = 1.0 / 2.2;
        /*
        if(some_next_function_that_returns_gamma(&next_gamma))
            LUT_exponent = 1.0 / next_gamma;

        */
    # elif defined(sgi) 
        LUT_exponent = 1.0 / 1.7;
        /* there doesn't seem to be any documented function to get the "gamma" value, so we do it the hard way */
        FILE * infile = fopen("/etc/config/system.glGammaVal", "r");
        if(infile) {
            double sgi_gamma;
            char fooline[81];
            fgets(fooline, 80, infile);
            fooline[80] = '\0';
            fclose(infile);
            sgi_gamma = atof(fooline);
            if(sgi_gamma > 0.0)
                LUT_exponent = 1.0 / sgi_gamma;
        }
    # elif defined (Macintosh)
        LUT_exponent = 1.8 / 2.61;
        /* if (some_mac_function_that_returns_gamma(&mac_gamma))
            LUT_exponent = mac_gamma / 2.61;
        */
    # endif

    default_display_exponent = LUT_exponent * CRT_exponent;

    double display_component;
    char * p;
    if((p = getenv("SCREEN_GAMMA")) != NULL)
        display_component = atof(p);
    else 
        display_component = default_display_exponent;
    // TODO: Provide the user with an option to alter gamma.
    double gamma;
    if(png_get_gAMA(png_ptr, info_ptr, &gamma))
        png_set_gamma(png_ptr, display_component, gamma); 

    // TODO: Do some color correction I guess.

    // + Reading The Image : 
    // Do Some Color Transformations Here And There ;
    if(color_type == PNG_COLOR_TYPE_PALETTE)
        png_set_palette_to_rgb(png_ptr); // Expands The Palette Into RGB8 
    else if(color_type == PNG_COLOR_TYPE_GRAY && bit_depth < 8) 
        png_set_expand_gray_1_2_4_to_8(png_ptr); // Expands 1, 2 and 4 bit Values Into 8 Bits (GrayScale). This is done because there's nothing such GL_R4 or GL_R2.
    else if(png_get_valid(png_ptr, info_ptr, PNG_INFO_tRNS))
        png_set_tRNS_to_alpha(png_ptr); // Expands transparency inforamtions into an alpha channel 

    uint32_t i, rowbytes;
    png_bytep row_pointers[image->mip[0].height];

    if(((color_type == PNG_COLOR_TYPE_GRAY && bit_depth < 8) || (color_type == PNG_COLOR_TYPE_PALETTE)) && !is_default_bg)
        png_set_background(png_ptr, background_color, PNG_BACKGROUND_GAMMA_FILE, 1, gamma);
    else 
        png_set_background(png_ptr, background_color, PNG_BACKGROUND_GAMMA_FILE, 0, gamma);

    png_read_update_info(png_ptr, info_ptr);  
    

    rowbytes = png_get_rowbytes(png_ptr, info_ptr);

    if((image->mip[0].data = malloc(rowbytes * image->mip[0].height)) == NULL) {
        png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
        return OUT_OF_MEM_ERROR; 
    }

    image->TextureTotalSize = rowbytes * image->mip[0].height;
    for( i = 0; i < image->mip[0].height; ++i) 
        row_pointers[i] = ((png_bytep) image->mip[0].data) + i * rowbytes; 
   

    png_read_image(png_ptr, row_pointers);

    png_read_end(png_ptr, NULL);

    return 0;
}

static int readpng_cleanup(png_structp png_ptr, png_infop info_ptr) {
    if(png_ptr && info_ptr) {
        png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
        png_ptr = NULL;
        info_ptr = NULL;
    }
    return 0;
}
