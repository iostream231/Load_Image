# include <GL/gl.h>
# include <string.h>
# include <stdint.h>
# include <stdlib.h>
# include <stdio.h>

#define _LITYPES 1

// Image Mip Data Container
typedef struct {
    GLsizei width; // WIDTH 
    GLsizei height; // HEIGHT 
    GLsizei depth; // DEPTH (For 3D) 
    GLsizeiptr mipStride; // Stride Between Mipmap Levles

    GLvoid * data; // DATA 
} vglImageMipData; 
# define  MAX_TEXTURE_MIPS 14 // The Guarrenteed Amount of supprted OpenGL 4.x Mipmaps. It supports up to 16Kx16x Images.

// Image Data Container
typedef struct {
    GLenum target; // Target.
    GLenum internalFormat; // Internal Format For the texture
    GLenum format; // Format Of The texture
    GLenum type; // Type Of Data
    GLsizeiptr mipmapCount; // Mipmap Count;
    GLsizei slices; // For Texture Arrays;
    GLsizeiptr SliceStride; // Stride Between each element of the array. (Might be imporatant for multisample/mipmaped textures);
    
    GLsizeiptr TextureTotalSize; // Total Size Of The Texture In Bytes;
    vglImageMipData mip[MAX_TEXTURE_MIPS];
    
} vglImageData;

// ImageData.
typedef void * ImageData;

// Linked List Definition
struct node_l {
    ImageData data;
    struct node_l * next;
    size_t data_size;
};

// Function Definitions 
struct node_l * append(struct node_l *, ImageData , size_t *);
void free_node(struct node_l *);
void quickSort(uint32_t array[], size_t left, size_t right);
uint32_t ReverseByteOrder_UINT32(uint32_t inp);
uint32_t ReverseHexOrder_UINT32(uint32_t inp);
uint16_t ReverseHexOrder_UINT16(uint16_t inp);

// Appends A Node To The Linked_List
struct node_l * append(struct node_l * root, ImageData data, size_t  * len) {
    if(root != NULL) 
        root->next = append(root->next, data, len);
    else {
        root = (struct node_l *) malloc(sizeof(*root));
        root->next = NULL;

        if(len == NULL || *len == 0) { 
            root->data = data;
            root->data_size = 0;
        } else {
            root->data = malloc(*len);
            memcpy(root->data, data, *len);
            root->data_size = *len;
        }
    }
    return root;
}
// Frees The Linkes list 
void free_node(struct node_l * root) {
    if(root->next != NULL)
        free_node(root->next);

    free(root->data);
    free(root);
}

static void swap(uint32_t array[], size_t a, size_t b) {
    uint32_t temp = array[a];
    array[a] = array[b];
    array[b] = temp;
}
void quickSort(uint32_t *array, size_t left, size_t right) {
    if(left >= right)
        return;

    swap(array, left ,(left + right) /2);
    size_t last = left;
    size_t i;
    for(i = left + 1; i <=right; ++i)
        if(array[i] < array[left])
            swap(array, ++last, i);


    swap(array, left, last);
    quickSort(array, left, last - 1);
    quickSort(array, last + 1, right);
}



// Reverses Byte Order Of an Unsigned 32-bit integer.
uint32_t ReverseByteOrder_UINT32(uint32_t inp) {
    uint32_t out = 0;
    uint32_t max = 0;
    max = ~max; // 1111 `1111 1111 1111 1111 1111 1111 1111
    for(size_t i = 0;i < 31 ; ++i) { 
        // It will easier if you try draw what I am doing here.
        out |= ( ((inp << (31 - i)) >> i) & ((max << 31) >> i));
        out |= ( ((inp >> (31 - i)) << i) & ((max >> 31) << i));
    }

    return out;
}

// Reversed Hexadecimal Order Of An Unsigned 32-Bit Integer 
uint32_t ReverseHexOrder_UINT32(uint32_t inp) {
    return ((inp & 0xff000000) >> 24 |
            (((inp & 0x00ff0000) << 8) >> 16) |
            (((inp & 0x0000ff00) >> 8) << 16) |
            ((inp & 0x000000ff) << 24));
    // TBF I just copied this code from the internet. IDK what it dows ~shrug~.
}

// Revers Hexadecimal Order Of An Unsigned 16-Bit Integer 
uint16_t ReverseHexOrder_UINT16(uint16_t inp) {
    return ((inp & 0xff00) >> 12 |
            ((inp & 0x00ff) << 12));
}
