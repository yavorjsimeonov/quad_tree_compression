// Status of the project:
// - not comparing colour right resulting in not working
// todo: fix the colour comparing function

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <math.h>


#define RESERVED 0x0000
#define NO_COMPRESSION 0
#define DATA_OFFSET_OFFSET 0x000A
#define WIDTH_OFFSET 0x0012
#define HEIGHT_OFFSET 0x0016
#define BITS_PER_PIXEL_OFFSET 0x001C
#define HEADER_SIZE 14
#define INFO_HEADER_SIZE 40
#define MAX_NUMBER_OF_COLORS 0
#define ALL_COLORS_REQUIRED 0


typedef unsigned int int32;
typedef short int16;
typedef unsigned char byte;

struct quad_tree{
    int level;
    int colour;
    struct quad_tree* tl;
    struct quad_tree* tr;
    struct quad_tree* bl;
    struct quad_tree* br;
};

struct quad_tree* create_quad_tree_node(){
    struct quad_tree* new_quad_tree_node = calloc(1, sizeof(struct quad_tree));
    new_quad_tree_node->level = 0;
    new_quad_tree_node->colour = 0;
    new_quad_tree_node->tl = NULL;
    new_quad_tree_node->tr = NULL;
    new_quad_tree_node->bl = NULL;
    new_quad_tree_node->br = NULL;

    return new_quad_tree_node;
}

void free_quad_tree(struct quad_tree* node){
    if (node) {
        free_quad_tree(node->tl);
        free_quad_tree(node->tr);
        free_quad_tree(node->bl);
        free_quad_tree(node->br);
        free(node);
    }
}

void read_image(const char* fileName, int32** pixels, int32* width, int32* height, int32* bytesPerPixel){
    FILE* imageFile = fopen(fileName, "rb");
    if (!imageFile) {
        printf("Failed to open the image file.\n");
        return;
    }

    int32 dataOffset;
    fseek(imageFile, DATA_OFFSET_OFFSET, SEEK_SET);
    fread(&dataOffset, 4, 1, imageFile);
    fseek(imageFile, WIDTH_OFFSET, SEEK_SET);
    fread(width, 4, 1, imageFile);

    fseek(imageFile, HEIGHT_OFFSET, SEEK_SET);
    fread(height, 4, 1, imageFile);

    int16 bitsPerPixel;
    fseek(imageFile, BITS_PER_PIXEL_OFFSET, SEEK_SET);
    fread(&bitsPerPixel, 2, 1, imageFile);

    *bytesPerPixel = ((int32)bitsPerPixel) / 8;

    int paddedRowSize = (int)(4 * ceil((float)(*width) / 4.0f)) * (*bytesPerPixel);
    int unpaddedRowSize = (*width) * (*bytesPerPixel);
    int32 totalSize = unpaddedRowSize * (*height);
    byte* pixelBytes = (byte*)malloc(totalSize);

    if (!*pixelBytes) {
        printf("Failed to allocate memory for pixel data.\n");
        fclose(imageFile);
        return;
    }

    byte* currentRowPointer = pixelBytes + ((*height - 1) * unpaddedRowSize);

    for (int i = 0; i < *height; i++) {
        fseek(imageFile, dataOffset + (i * paddedRowSize), SEEK_SET);
        fread(currentRowPointer, 1, unpaddedRowSize, imageFile);
        currentRowPointer -= unpaddedRowSize;
    }

    *pixels = (int32*)malloc(totalSize / (*bytesPerPixel) * sizeof(int32));
    for(int i = 0; i < totalSize / (*bytesPerPixel); i++){
        (*pixels)[i] = (int)pixelBytes[i * 3] | pixelBytes[i * 3 + 1] << 8 | pixelBytes[i * 3 + 2] << 16;
    }

    fclose(imageFile);
}

int get_pixel_colour(int* image, int x, int y, int width) {
    return image[y * width + x];
}

int can_region_be_one_colour(int* image, int tl_x, int tl_y, int br_x, int br_y, int width) {
    int color = get_pixel_colour(image, tl_x, tl_y, width);

    for (int y = tl_y; y <= br_y; y++) {
        for (int x = tl_x; x <= br_x; x++) {
            if (get_pixel_colour(image, x, y, width) != color) {
                return 0;
            }
        }
    }
    return 1;
}

struct quad_tree* create_quad_tree(int* image, int tl_x, int tl_y, int br_x, int br_y, int level, int width) {
    struct quad_tree* node = create_quad_tree_node();

    if (can_region_be_one_colour(image, tl_x, tl_y, br_x, br_y, width)) {
        node->colour = get_pixel_colour(image, tl_x, tl_y, width);
    }
    else{

        int mid_x = (tl_x + br_x) / 2;
        int mid_y = (tl_y + br_y) / 2;

        node->tl = create_quad_tree(image, tl_x, tl_y, mid_x, mid_y, level + 1, width);
        node->tr = create_quad_tree(image, mid_x + 1, tl_y, br_x, mid_y, level + 1, width);
        node->bl = create_quad_tree(image, tl_x, mid_y + 1, mid_x, br_y, level + 1, width);
        node->br = create_quad_tree(image, mid_x + 1, mid_y + 1, br_x, br_y, level + 1, width);
    }

    node->level = level;
    return node;
}

bool is_color_similar(int color1, int color2, float similarity_threshold) {
    int r1, r2, g1, g2, b1, b2;

    r1 = color1 & 0xFF;
    r2 = color2 & 0xFF;
    g1 = (color1 >> 8) & 0xFF;
    g2 = (color2 >> 8) & 0xFF;
    b1 = (color1 >> 16) & 0xFF;
    b2 = (color2 >> 16) & 0xFF;
    double d =  sqrt(pow((double)r2 - (double)r1, 2) + pow((double)g2 - (double)g1, 2) + pow((double)b2 - (double)b1, 2));

    double p = d / sqrt((255) ^ 2 + (255) ^ 2 + (255) ^ 2);

    return p >= similarity_threshold;
}

void compress_quad_tree(struct quad_tree* node) {
    if (node->tl != NULL && node->tr != NULL && node->bl != NULL && node->br != NULL){

        if (is_color_similar(node->tl->colour, node->tr->colour, 10.0f) &&
            is_color_similar(node->tr->colour, node->bl->colour, 10.0f) &&
            is_color_similar(node->bl->colour, node->br->colour, 10.0f)){

            node->colour = (node->tl->colour + node->tr->colour + node->bl->colour + node->br->colour) /4;
            node->level = 0;
            free_quad_tree(node->tl);
            free_quad_tree(node->tr);
            free_quad_tree(node->bl);
            free_quad_tree(node->br);
            node->tl = NULL;
            node->tr = NULL;
            node->bl = NULL;
            node->br = NULL;
        }
        else {
            compress_quad_tree(node->tl);
            compress_quad_tree(node->tr);
            compress_quad_tree(node->bl);
            compress_quad_tree(node->br);
        }
    }
}

void decompress_quad_tree(struct quad_tree* node, int* decompressed_image, int tl_x, int tl_y, int br_x, int br_y, int width){
    if (node->tl == NULL && node->tr == NULL && node->bl == NULL && node->br == NULL) {
        int color = node->colour;
        for (int y = tl_y; y <= br_y; y++) {
            for (int x = tl_x; x <= br_x; x++) {
                decompressed_image[y * width + x] = color;
            }
        }
    }
    else {
        int mid_x = (tl_x + br_x) / 2;
        int mid_y = (tl_y + br_y) / 2;

        decompress_quad_tree(node->tl, decompressed_image, tl_x, tl_y, mid_x, mid_y, width);
        decompress_quad_tree(node->tr, decompressed_image, mid_x + 1, tl_y, br_x, mid_y, width);
        decompress_quad_tree(node->bl, decompressed_image, tl_x, mid_y + 1, mid_x, br_y, width);
        decompress_quad_tree(node->br, decompressed_image, mid_x + 1, mid_y + 1, br_x, br_y, width);
    }
}

void create_bmp_file_from_decompressed_image(int* decompressed_image, int32 width, int32 height, int32 bytesPerPixel){

    FILE *outputFile = fopen("d://decompressed.bmp", "wb");

    int32 file_size = HEADER_SIZE + INFO_HEADER_SIZE + (height * width * bytesPerPixel);
    int32 data_offset = HEADER_SIZE + INFO_HEADER_SIZE;
    int32 reserved = RESERVED;
    int32 image_size = height * width * bytesPerPixel;
    int32 header_size = INFO_HEADER_SIZE;
    int16 color_planes = 1;
    int32 horizontal_resolution = 0x0B6D;
    int32 vertical_resolution = 0x0B6D;
    int32 compression = NO_COMPRESSION;
    int32 colors_in_palette = MAX_NUMBER_OF_COLORS;
    int32 import_colors = ALL_COLORS_REQUIRED;
    int16 bitsPerPixel = bytesPerPixel * 8;

    int unpaddedRowSize = width * bytesPerPixel;
    int paddedRowSize = (int)(4 * ceil((float)width/4.0f))*bytesPerPixel;
    const char *BM = "BM";
    fwrite(&BM[0], 1, 1, outputFile);
    fwrite(&BM[1], 1, 1, outputFile);
    fwrite(&file_size, 4, 1, outputFile);
    fwrite(&reserved, 4, 1, outputFile);
    fwrite(&data_offset, 4, 1, outputFile);
    fwrite(&header_size, 4, 1, outputFile);
    fwrite(&width, 4, 1, outputFile);
    fwrite(&height, 4, 1, outputFile);
    fwrite(&color_planes, 2, 1, outputFile);
    fwrite(&bitsPerPixel, 2, 1, outputFile);
    fwrite(&compression, 4, 1, outputFile);
    fwrite(&image_size, 4, 1, outputFile);
    fwrite(&horizontal_resolution, 4, 1, outputFile);
    fwrite(&vertical_resolution, 4, 1, outputFile);
    fwrite(&colors_in_palette, 4, 1, outputFile);
    fwrite(&import_colors, 4, 1, outputFile);

    for(int i = 0; i < height * width; i++){
        fwrite(decompressed_image + i, bytesPerPixel, 1, outputFile);
    }


    fclose(outputFile);


}

int main(){
    int* pixels;
    int32 width;
    int32 height;
    int32 bytesPerPixel;
    int* decompressed_image;

    read_image("d://img.bmp", &pixels, &width, &height, &bytesPerPixel);
    printf("\n bytesPerPixel %d\n", bytesPerPixel);

    struct quad_tree* quadtree = create_quad_tree(pixels, 0, 0, width - 1, height - 1, 0, width);
    if (quadtree->tl) {
        printf("\n quadtree->tl not null\n");
    }

    compress_quad_tree(quadtree);
    if (quadtree->tl) {
            printf("\n quadtree->tl not null (1)\n");
    } else {
            printf("\n quadtree->tl is null (1)\n");

    }

    decompressed_image = (int*)malloc(width * height * 4);
    if (!decompressed_image) {
        printf("Failed to allocate memory for decompressed image.\n");
        free_quad_tree(quadtree);
        free(pixels);
        return 0;
    }

    decompress_quad_tree(quadtree, decompressed_image, 0, 0, width - 1, height - 1, width);

    create_bmp_file_from_decompressed_image(decompressed_image, width, height, bytesPerPixel);

    free_quad_tree(quadtree);
    free(pixels);
    free(decompressed_image);

    return 0;
}
