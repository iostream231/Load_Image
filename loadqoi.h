# include <stdio.h>
# include <string.h>
# include <stdlib.h>
# include <stdint.h>


# ifndef _LITYPES 
    # include "ImageTypes.h"
# endif 

// structs
struct qoi_header {
    char magic[4]; // Magic Number QOIF 
    uint32_t width; // image width 
    uint32_t height; // image height 
    uint8_t channels; // 3 = RGB, 4 = RGBA
    uint8_t colorspace; // 0 = sRGB with Linear Alpha
                        // 1 = 1 all channels linear 
};

struct color_cnk_4 {
    uint8_t r;
    uint8_t g;
    uint8_t b;
    uint8_t a;
};
struct color_cnk_3 {
    uint8_t r;
    uint8_t g;
    uint8_t b;
};

// Functions
int loadqoi(vglImageData *image, char *filename);
int is_qoi(char *filename);
static uint8_t hash(uint8_t r, uint8_t g, uint8_t b, uint8_t a);


// Errors 
# define UNABLE_TO_OPEN_FILE 0x200
# define INSUFFISCIENT_FILE_SIZE 0x400
# define READ_FILE_ERROR 0x600
# define NOT_QOI_SUPPORTED_FILE 0x105
# define IMAGE_ALREADY_EXIST 0x1

// QOI Constants
# define MAGIC_QOIF "qoif" 
# define QOI_OP_RGB 0b11111110
# define QOI_OP_RGBA 0b11111111
# define QOI_OP_INDEX 0b00
# define QOI_OP_DIFF 0b01
# define QOI_OP_LUMA 0b10 
# define QOI_OP_RUN 0b11 

// Load QOI File 
int loadqoi(vglImageData * image, char * filename) {
   if ( image )
       return IMAGE_ALREADY_EXIST;

   image = (vglImageData *) malloc(sizeof(*image));


    // Read The Contents Of The File :
    FILE * infile;
    if(( infile = fopen(filename, "rb") ) == NULL)
        return UNABLE_TO_OPEN_FILE;

    // Read The Length Of The FIle 
    size_t len = 0;
    fseek(infile, 0, SEEK_END);
    len = ftell(infile);
    fseek(infile, 0, SEEK_SET);
    
    if(len < 18) // Header: 14 Bytes + Min One Pixel 4 Bytes 
        return INSUFFISCIENT_FILE_SIZE;
    
    // Reading Data 
    ImageData contents = malloc(len);
    if(fread(contents, 1, len, infile) != len)
        return READ_FILE_ERROR;
    
    fclose(infile);

    struct qoi_header hdr;

    char * char_img_ptr = (char *) contents;

    // Check For QOI Signal
    if(strncmp(char_img_ptr, MAGIC_QOIF, 4) != 0)
        return NOT_QOI_SUPPORTED_FILE;

    contents = (ImageData) ( char_img_ptr += 4 );
    
    // Fill In Default Values
    image->mipmapCount = 1;
    image->type = GL_UNSIGNED_BYTE; // Yep QOI Uses 8-bit depth or less for specifying colors;
    image->slices = 0; // No arrays 
    image->SliceStride = 0;
    image->target = GL_TEXTURE_2D; // Not 3D

    // Reading the rest of the eader
    uint32_t * uint32_img_ptr = (uint32_t *) contents;
    image->mip[0].width = hdr.width = ReverseHexOrder_UINT32(*uint32_img_ptr++);
    image->mip[0].height = hdr.height = ReverseHexOrder_UINT32(*uint32_img_ptr++);
    contents = (ImageData) uint32_img_ptr;

    uint8_t * uint8_img_ptr = (uint8_t *) contents;
    hdr.channels = *uint8_img_ptr++;
    hdr.colorspace = *uint8_img_ptr;

    if(hdr.channels == 3) {
        image->format = GL_RGB;
        image->internalFormat = GL_RGB8;
    }
    else if (hdr.channels == 4){
        image->format = GL_RGBA;
        image->internalFormat = GL_RGBA8;
    }
    else 
        return NOT_QOI_SUPPORTED_FILE;

    contents = (ImageData) uint8_img_ptr;

    // Very Cool, now we need to read the rest of the file. *sigh*
    size_t read_data_size = 0;
    if(hdr.channels == 3) {
        struct color_cnk_3 color_array[64] = { 0 };
        
        uint8_t data[hdr.width * hdr.height][3];
        for(size_t i = 0; i < hdr.width * hdr.height && read_data_size < hdr.width * hdr.height * hdr.channels; ++i){
            if(*uint8_img_ptr == QOI_OP_RGB) {
                // Saves Pixel Data 
                data[i][0] = *++uint8_img_ptr;
                data[i][1] = *++uint8_img_ptr;
                data[i][2] = *++uint8_img_ptr;

                continue;
            }
            switch ((*uint8_img_ptr) >> 6) {
                case QOI_OP_INDEX :
                    uint8_t ind_cl; 
                    ind_cl = ((*uint8_img_ptr++) << 2) >> 2;
                    if(ind_cl < 0 || ind_cl > 64)
                        return NOT_QOI_SUPPORTED_FILE;
                    data[i][0] = color_array[ind_cl].r;
                    data[i][1] = color_array[ind_cl].g;
                    data[i][2] = color_array[ind_cl].b;
                    break;
                case QOI_OP_DIFF :
                    if( i <= 0 )
                        return NOT_QOI_SUPPORTED_FILE;
                    int8_t dr, dg, db;
                    dr = ((*uint8_img_ptr) << 2) >> 6;
                    dg = ((*uint8_img_ptr) << 4) >> 6;
                    db = ((*uint8_img_ptr++) << 6) >> 6;

                    dr -= 2, dg -=2, db -= 2;
                    data[i][0] = data[i - 1][0] - dr;
                    data[i][1] = data[i - 1][1] - dg;
                    data[i][2] = data[i - 1][2] - db;

                    break;
                case QOI_OP_LUMA :
                    int8_t diffG = *uint8_img_ptr++ << 2;
                    int8_t dr_g, db_g;
                    
                    dr_g = *uint8_img_ptr >> 4;
                    db_g = (*uint8_img_ptr++ << 4) >> 4;

                    diffG -= 32;
                    dr_g -= 8, db_g -= 8;

                    data[i][1] = data[i - 1][1] - diffG;
                    data[i][0] = dr_g - diffG + data[i-1][0];
                    data[i][2] = db_g - diffG + data[i-1][2];
                    break;
                case QOI_OP_RUN :
                    uint8_t len = *++uint8_img_ptr >> 2;
                    if(len >= 63)
                        return NOT_QOI_SUPPORTED_FILE;

                    while(len-- > 0){
                        data[i++][0] = data[i - 2][0];
                        data[i++][1] = data[i - 2][1];
                        data[i++][2] = data[i - 2][2];
                    }
                    break;
            }
            // Saves To Color Array 
                uint8_t ind = hash(data[i][0], data[i][1], data[i][2], 255); // A: 255 Because the specification said the array should be init to {0, 0, 0, 255}.
                color_array[ind].r = data[i][0];
                color_array[ind].g = data[i][1];
                color_array[ind].b = data[i][2];
        }
        image->mip[0].data = (ImageData) data;
        return 0;
    }   
    // We Do Quite The Same Thing Again :)
    if(hdr.channels == 4) {
        struct color_cnk_4 color_array[64] = { 0 };

        uint8_t data[hdr.width * hdr.height][4];
        for(size_t i = 0; i < hdr.width * hdr.height && read_data_size < hdr.width * hdr.height * hdr.channels; ++i){
            data[i][0] = 0, data[i][1] = 0, data[i][2] = 0, data[i][3] = 255; // Init 
            if(*uint8_img_ptr == QOI_OP_RGB) {
                // Saves Pixel Data 
                data[i][0] = *++uint8_img_ptr;
                data[i][1] = *++uint8_img_ptr;
                data[i][2] = *++uint8_img_ptr;
                data[i][3] = data[i - 1][3];
                uint8_img_ptr++;

                continue;
            }
            if(*uint8_img_ptr == QOI_OP_RGBA) {
                
                data[i][0] = *++uint8_img_ptr;
                data[i][1] = *++uint8_img_ptr;
                data[i][2] = *++uint8_img_ptr;
                data[i][3] = *++uint8_img_ptr;
                uint8_img_ptr++;

                continue;
            }
            switch ((*uint8_img_ptr) >> 6) {
                case QOI_OP_INDEX :
                    uint8_t ind_cl; 
                    ind_cl = ((*uint8_img_ptr++) << 2) >> 2;
                    if(ind_cl < 0 || ind_cl > 64)
                        return NOT_QOI_SUPPORTED_FILE;
                    data[i][0] = color_array[ind_cl].r;
                    data[i][1] = color_array[ind_cl].g;
                    data[i][2] = color_array[ind_cl].b;
                    data[i][3] = color_array[ind_cl].a;
                    break;
                case QOI_OP_DIFF :
                    if( i <= 0 )
                        return NOT_QOI_SUPPORTED_FILE;
                    int8_t dr, dg, db;
                    dr = ((*uint8_img_ptr) << 2) >> 6;
                    dg = ((*uint8_img_ptr) << 4) >> 6;
                    db = ((*uint8_img_ptr++) << 6) >> 6;

                    dr -= 2, dg -=2, db -= 2;
                    data[i][0] = data[i - 1][0] - dr;
                    data[i][1] = data[i - 1][1] - dg;
                    data[i][2] = data[i - 1][2] - db;
                    data[i][3] = data[i - 1][3];

                    break;
                case QOI_OP_LUMA :
                    int8_t diffG = *uint8_img_ptr++ << 2;
                    int8_t dr_g, db_g;

                    dr_g = *uint8_img_ptr >> 4;
                    db_g = (*uint8_img_ptr++ << 4) >> 4;

                    diffG -= 32;
                    dr_g -= 8, db_g -= 8;

                    data[i][1] = data[i - 1][1] - diffG;
                    data[i][0] = dr_g - diffG + data[i-1][0];
                    data[i][2] = db_g - diffG + data[i-1][2];
                    data[i][3] = data[i - 1][3];
                    break;
                case QOI_OP_RUN :
                    uint8_t len = *++uint8_img_ptr >> 2;
                    if(len >= 63)
                        return NOT_QOI_SUPPORTED_FILE;

                    while(len-- > 0){
                        data[i++][0] = data[i - 2][0];
                        data[i++][1] = data[i - 2][1];
                        data[i++][2] = data[i - 2][2];
                        data[i++][3] = data[i - 2][3];
                    }
                    break;
                default:
                    return NOT_QOI_SUPPORTED_FILE;
            }
            // Saves To Color Array 
            uint8_t ind = hash(data[i][0], data[i][1], data[i][2], 255); // A: 255 Because the specification said the array should be init to {0, 0, 0, 255}.
            color_array[ind].r = data[i][0];
            color_array[ind].g = data[i][1];
            color_array[ind].b = data[i][2];
            color_array[ind].a = data[i][3];
        }
        image->mip[0].data = (ImageData) data;
        return 0;
    }
    
    
};


// Checks If A File Is QOI Or Not 
int is_qoi(char * filename) {
    FILE * infile;
    if(( infile = fopen(filename, "rb") ) != NULL) {
        char magic_sq[5];
        fread(magic_sq, 1, 4, infile);
        magic_sq[4] = '\0';
        fclose(infile);
        return (strncmp(magic_sq, MAGIC_QOIF, 4) == 0) ? 0 : NOT_QOI_SUPPORTED_FILE;
    }
    return UNABLE_TO_OPEN_FILE;
}

// Generates A Hash Of Color Data 
static uint8_t hash(uint8_t r, uint8_t g; uint8_t b; uint8_t a) {
    return (r * 3 + g * 5 + b * 7 + a * 11) % 64
}
