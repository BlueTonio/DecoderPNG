#include "zlib.h"
#include <stdio.h> 
#include <string.h> 
#include <stdlib.h> 
#define SIGNATURE_PNG "\x89PNG\r\n\x1a\n"
#define IMAGE "image/Figure_1.png"
#define BytePerPixel 4

typedef struct chunk_png{
    unsigned int chunk_lenght;
    char* chunk_type;
    char* chunk_data;
    unsigned int crc_chunk;
}chunk_png;

typedef struct png_file{
    unsigned char* signature_png;
    chunk_png* png_chunk[100];
    unsigned int crc_png;

}png_file;

typedef struct png_IHDR{
    unsigned int width;
    unsigned int height;
    unsigned char bit_depth;
    unsigned char color_type;
    unsigned char compression_method;
    unsigned char filter_method;
    unsigned char interlace_method;
}png_IHDR;

FILE* png_stream = NULL;
png_file* file_data_png = NULL;
unsigned int byteswap(unsigned int n)
{
    unsigned int m = (n << 24);
    m += (n << 8)&0x00ff0000;
    m += (n >> 8)&0x0000ff00;
    m += (n >> 24)&0x000000ff;
    return m;
}

errno_t PNG_open_signature_PNG(png_file* filePng)
{
    // read the file
    errno_t png_file_errno = fopen_s(&png_stream, IMAGE, "rb");

    // check if the file is a png
    if(png_file_errno != 0)
    {
        printf("Error: File is not a png");
        fclose(png_stream);
        return png_file_errno;
    }
    
    unsigned char* signature_png = malloc(8);
    unsigned char check_signature_png[8] = SIGNATURE_PNG;
    if(fread(signature_png,1,8,png_stream) != 8){
        printf("Error: signature isn't correct");
        free(signature_png);
        fclose(png_stream);
        return png_file_errno;
    }
    for (size_t i = 0; i < sizeof(check_signature_png); i++)
    {
        if(signature_png[i] != check_signature_png[i]){
            printf("Error: signature isn't correct");
            free(signature_png);
            fclose(png_stream);
            return png_file_errno;
        }
    }
    filePng->signature_png = signature_png; 
    return png_file_errno;
}

errno_t chunk_read_PNG(chunk_png* chunkPng)
{
    int file_lengh = 4;
    unsigned int lenght_chunk;
    fread(&lenght_chunk, 1, 4, png_stream);
    lenght_chunk = byteswap(lenght_chunk);
    chunkPng->chunk_lenght = lenght_chunk;

    char* data_chunk_for_crc = malloc(file_lengh + lenght_chunk);
    fread(data_chunk_for_crc,1, file_lengh + lenght_chunk, png_stream);
    chunkPng->chunk_type = data_chunk_for_crc;
    chunkPng->chunk_data = data_chunk_for_crc + file_lengh;

    unsigned int chunk_crc;
    fread(&chunk_crc,1,4,png_stream);
    chunk_crc = byteswap(chunk_crc);
    unsigned int calculated_crc = crc32(0, (const unsigned char*)data_chunk_for_crc, lenght_chunk + file_lengh);
    if(calculated_crc != chunk_crc){
        printf("crc32 calculated is %u\n", calculated_crc);
        return -1;
    }
    return 0;


}

errno_t check_IHDR(png_IHDR* IHDRPng, chunk_png* chunkPng)
{
    unsigned int swap_to_data;
    unsigned int* data_PTR;
    if(chunkPng->chunk_lenght != 13){
        printf("Error : IHDR is too long");
        return -1;
    }
    IHDRPng->bit_depth = chunkPng->chunk_data[8];
    IHDRPng->color_type = chunkPng->chunk_data[9];
    IHDRPng->compression_method = chunkPng->chunk_data[10];
    IHDRPng->filter_method = chunkPng->chunk_data[11];
    IHDRPng->interlace_method = chunkPng->chunk_data[12];
    
    data_PTR = (unsigned int*)chunkPng->chunk_data;
    swap_to_data = *data_PTR;
    IHDRPng->width = byteswap(swap_to_data);

    data_PTR += 1;
    swap_to_data = *data_PTR;
    IHDRPng->height= byteswap(swap_to_data);
    
    if(IHDRPng->compression_method != 0){
        printf("invalid compression method");
        return -1;
    }
    if(IHDRPng->bit_depth != 8){
        printf("invalid bit depth");
        return -1;
    }
    if(IHDRPng->color_type != 6){
        printf("invalid color type");
        return -1;
    }
    if(IHDRPng->filter_method != 0){
        printf("invalid filter method");
        return -1;
    }
    if (IHDRPng->interlace_method != 0){
        printf("invalid interlace method");
        return -1;
    }
    return 0;
}

char* check_IDAT_enqueue(char* pngImage, chunk_png* chunkPngIDAT, int displayChunk)
{
    if(pngImage == NULL || displayChunk == 0){
        pngImage = (char*)malloc(chunkPngIDAT->chunk_lenght);
        if(pngImage == NULL){
            return NULL;
        }
        pngImage = (char*)memcpy(pngImage,chunkPngIDAT->chunk_data, chunkPngIDAT->chunk_lenght);
        if(pngImage == NULL){
            return NULL;
        }
        displayChunk = chunkPngIDAT->chunk_lenght;
    }
    else
    {
        pngImage = (char*)realloc(pngImage, chunkPngIDAT->chunk_lenght);
        if(pngImage == NULL){
            return NULL;
        }
        pngImage = (char*)memcpy(pngImage + displayChunk,chunkPngIDAT->chunk_data, chunkPngIDAT->chunk_lenght);
        if(pngImage == NULL){
            return NULL;
        }
        displayChunk += chunkPngIDAT->chunk_lenght;
    }
    return pngImage;

}

int path_predictor(int a, int b, int c)
{
   int p = a + b - c;
   int pa = abs(p - a);
   int pb = abs(p - b);
   int pc = abs(p - c);
   int Pr;
   if (pa <= pb && pa <= pc){
    Pr = a;
   } 
   else if ( pb <= pc){
    Pr = b;
   }
   else{
    Pr = c;
   }
   return Pr;
}

int recon_a(int r, int c, int stride, char* recon)
{
    if (c < BytePerPixel) 
    {
        return 0;
    }

    return recon[r * stride + c - BytePerPixel];
}

int recon_b(int r, int c, int stride, char* recon)
{
    if (r == 0) 
    {
        return 0;
    }

    return recon[(r-1) * stride + c - BytePerPixel];
}

int recon_c(int r, int c, int stride, char* recon)
{
    if (r == 0 || c < BytePerPixel) 
    {
        return 0;
    }

    return recon[(r-1) * stride + c - BytePerPixel];
}

char* recon_func(png_IHDR *IHDRHeader , char* dataPNG)
{
    int i = 0;
    int index_array = 0;
    int stride = BytePerPixel*IHDRHeader->width;
    int type_filter,r,c;
    unsigned char filter_x, recon_x;
    char* recon = malloc(IHDRHeader->height * stride);
    for(r = 0; r< IHDRHeader->height; r++){
        type_filter = dataPNG[i++];
        for (c = 0; c < IHDRHeader->width; c++)
        {
            filter_x = dataPNG[i++];
            if(type_filter == 0){
                recon_x = filter_x;
            }
            else if(type_filter == 1){
                recon_x =filter_x + recon_a(r,c,stride, recon);
            }
            else if(type_filter == 2){
                recon_x =filter_x + recon_b(r,c,stride, recon);
            }
            else if (type_filter == 3){
                recon_x =filter_x + (recon_a(r,c,stride, recon) +recon_b(r,c,stride, recon)) / 2 ;
            }
            else if(type_filter == 4){
                recon_x =filter_x + path_predictor(recon_a(r,c,stride, recon),recon_b(r,c,stride, recon), recon_c(r,c,stride, recon));
            }
            else{
                printf("unknown filter type: " + str(type_filter));
                return NULL;
            }
            recon[index_array++] = recon_x;
        }
        
    }
    return recon;

}


int main(int argc, char **argv)
{
    char* png_image;
    int png_lenght;
    int i,j;
    png_IHDR *IHDR_png;
    chunk_png *chunk;
    file_data_png =(png_file*)malloc(sizeof(png_file));
    errno_t stream_png_errno = PNG_open_signature_PNG(file_data_png);
    char* recon= NULL;
    if(stream_png_errno != 0){
        printf("Error PNG convertor");
        return stream_png_errno;
    }
    for (i = 0; i < 101; i++)
    {
        chunk = malloc(sizeof(chunk_png));
        if(chunk_read_PNG(chunk) == -1){
            printf("Error Chunk type: %s number %d invalid crc: %u***\n", chunk->chunk_type, i, chunk->crc_chunk);
            for (j = 0; j < i; j++)
            {
                if(j == 0){
                    free(IHDR_png);
                }
            }
            free(chunk);
            free(file_data_png);
            return -1;
        }

        if(strstr(chunk->chunk_type, "IHDR") != NULL){
            IHDR_png = malloc(sizeof(png_IHDR));
            errno_t IHDR_errno = check_IHDR(IHDR_png,chunk);
            if(IHDR_errno != 0){
                printf("Error PNG IHDR not valid: %d", IHDR_errno);
                free(IHDR_png);
                free(chunk);
                free(IHDR_errno);
                return -1;
            }
        }
        if(strstr(chunk->chunk_type, "IEND") != NULL){
            break;
        }
        if(strstr(chunk->chunk_type, "IDAT") != NULL){
            png_image= malloc(chunk->chunk_lenght);
            png_lenght = chunk->chunk_lenght;
            memcpy(png_image, chunk->chunk_data, chunk->chunk_lenght);
        }
        file_data_png->png_chunk[i] = chunk;
        
    }
    for(j = 0; j <= i; j++){
        if(j == 0){
            free(IHDR_png);
        }
    }
    free(file_data_png);
      //uncompress PNG
    unsigned long uncompressed_size = 600000;
    unsigned char *uncompressed_data = malloc(uncompressed_size);
    int result = uncompress(uncompressed_data, &uncompressed_size, png_image, png_lenght);
    if(result != Z_OK)
    {
        free(uncompressed_data);
        free(png_image);
        printf("Error uncompressing the png file");
        return -1;
    }
    else
    {
        free(png_image);
    }
    recon = recon_func(IHDR_png, uncompressed_data);
    
    free(uncompressed_data);
    free(recon);
    return 0;     
}