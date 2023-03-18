# Load_Image
A C library for loading images as textures to use with OpenGL. It isn't complete yet :D.

# Installation 
I thought of making a Cmake File but... It'll be easier if you just copy it to your project and include the header files.
## Dependencies :
  For Loading PNG Files You need **libpng 1.5+**
  For Loading JPEG Files You need **libjpeg-dev**
 
# Usage 
## Image Data :
To read data, this library uses the struct : 
```c
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
```
and 
```c
// Image Mip Data Container
typedef struct {
    GLsizei width; // WIDTH 
    GLsizei height; // HEIGHT 
    GLsizei depth; // DEPTH (For 3D) 
    GLsizeiptr mipStride; // Stride Between Mipmap Levles

    GLvoid * data; // DATA 
} vglImageMipData; 

```
It fills the available options and also recommends the texture's target, type, format and internalFormat.
It also supports multiple mipmaps and texture arrays (even tho I didn't work on a texture type that supports these)

## Image Reading Functions :
After you included the desired header file. You can use the `load{image_type}(vglImageData * data, char * filename)` function. It allocates a vglImageData struct 
and fills it with informations from the image.
A Zero-Return Value Means Success, A non-zero return value means an error
For Example :
```c
vglImageData * image = NULL; // Make Sure to initialize it to NULL.
int retval = loadpng(image, "./bg.png");
if(retval)
  exit(EXIT_FAILURE);
```
When You Finish with the image, just unload it using `unloadimage{image_type}(vglImageFata * data)`
For Example :
```c
unloadpng(image);
```

