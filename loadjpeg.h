// Standard Libs
# include <stdio.h>
# include <stdlib.h>

// JPEG lib 
# include "jpeglib.h"

// Image Types Lib 
# ifndef  _LITYPES
    # include "ImageTypes.h"
# endif

# define JPEG_IMAGE_ALREADY_EXIST_ERROR 2

int loadjpeg_file(vglImageData *image, FILE *infile);
int loadjpeg(vglImageData *image, char *filename);


int loadjpeg(vglImageData * image, char * filename) {
    FILE * infile;
    if((infile = fopen(filename, "wb")) != NULL && !loadjpeg_file(image, infile)) {
        fclose(infile);
        return 0;
    }
    else 
        return -1;
}

// Load A Jpeg Image From A File 
int loadjpeg_file(vglImageData * image, FILE * infile) {
    if( image )
        return JPEG_IMAGE_ALREADY_EXIST_ERROR;
    else 
        image =(vglImageData *) malloc(sizeof(*image));

    // + Initializing The Structs 
    struct jpeg_decompress_struct cinfo;
    struct jpeg_error_mgr jerr;

    cinfo.err = jpeg_std_error(&jerr);
    jpeg_create_decompress(&cinfo);

    jpeg_stdio_src(&cinfo, infile);
    
    // Filling In Some Default Data 
    image->mipmapCount = 1;
    image->target = GL_TEXTURE_2D;
    image->slices = 1;
    image->SliceStride = 0;

    // Filling Int Recognized Data 
    image->mip[0].width = cinfo.image_width;
    image->mip[0].height = cinfo.image_height;
    image->mip[0].mipStride = 0;

    // TODO: Provide the user the option if he wants to read 16bit output 
    image->type = GL_UNSIGNED_BYTE;
    if(cinfo.out_color_components == 1) {
        image->format = GL_R;
        image->internalFormat = GL_R8;
    } else if(cinfo.out_color_components == 3) {
        image->format = GL_RGB;
        image->internalFormat = GL_RGB8;
    }

    // Initialize Pointers 
    JSAMPLE scanline_pointer[cinfo.image_width];
    JSAMPROW row_pointer[cinfo.image_height];
    for(size_t i = 0; i < cinfo.image_height; ++i) 
        row_pointer[i] = scanline_pointer;

    JSAMPARRAY arr_pointer = row_pointer;
    
    // Read Data One Scanline At A Time 
    while(cinfo.output_scanline < cinfo.image_height)
        jpeg_read_scanlines(&cinfo, arr_pointer, 1); // Reading One Line At a time 

    // Initialize Mip Data && Fill It 
    image->mip[0].data = malloc(cinfo.image_height * cinfo.image_width * cinfo.out_color_components + 1);
    memcpy(image->mip[0].data, arr_pointer, cinfo.image_height * cinfo.image_width * cinfo.out_color_components);
    jpeg_finish_decompress(&cinfo);

    // Dispose Of The JPEG read struct 
    jpeg_destroy_decompress(&cinfo);
    return 0;
}

// Unloads Image Data 
int unloadjpeg(vglImageData * image) {
    // Frees The Structs 
    free(image->mip[0].data);
    free(image);

    return 0;
    
}


int is_jpeg(char * filename) {

}
