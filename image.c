#include <stdio.h>
#include <stdint.h>
#include <time.h>
#include <string.h>
#include <pthread.h>
#include "image.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

enum KernelTypes type;

void *convolute_loop(void* image_args);

//An array of kernel matrices to be used for image convolution.  
//The indexes of these match the enumeration from the header file. ie. algorithms[BLUR] returns the kernel corresponding to a box blur.
Matrix algorithms[]={
    {{0,-1,0},{-1,4,-1},{0,-1,0}},
    {{0,-1,0},{-1,5,-1},{0,-1,0}},
    {{1/9.0,1/9.0,1/9.0},{1/9.0,1/9.0,1/9.0},{1/9.0,1/9.0,1/9.0}},
    {{1.0/16,1.0/8,1.0/16},{1.0/8,1.0/4,1.0/8},{1.0/16,1.0/8,1.0/16}},
    {{-2,-1,0},{-1,1,1},{0,1,2}},
    {{0,0,0},{0,1,0},{0,0,0}}
};


//getPixelValue - Computes the value of a specific pixel on a specific channel using the selected convolution kernel
//Paramters: srcImage:  An Image struct populated with the image being convoluted
//           x: The x coordinate of the pixel
//          y: The y coordinate of the pixel
//          bit: The color channel being manipulated
//          algorithm: The 3x3 kernel matrix to use for the convolution
//Returns: The new value for this x,y pixel and bit channel
uint8_t getPixelValue(Image* srcImage,int x,int y,int bit,Matrix algorithm){
    //printf("Entered getPixelValue\n");
    int px,mx,py,my,i,span;
    span=srcImage->width*srcImage->bpp;
    // for the edge pixes, just reuse the edge pixel
    px=x+1; py=y+1; mx=x-1; my=y-1;
    if (mx<0) mx=0;
    if (my<0) my=0;
    if (px>=srcImage->width) px=srcImage->width-1;
    if (py>=srcImage->height) py=srcImage->height-1;
    uint8_t result=
        algorithm[0][0]*srcImage->data[Index(mx,my,srcImage->width,bit,srcImage->bpp)]+
        algorithm[0][1]*srcImage->data[Index(x,my,srcImage->width,bit,srcImage->bpp)]+
        algorithm[0][2]*srcImage->data[Index(px,my,srcImage->width,bit,srcImage->bpp)]+
        algorithm[1][0]*srcImage->data[Index(mx,y,srcImage->width,bit,srcImage->bpp)]+
        algorithm[1][1]*srcImage->data[Index(x,y,srcImage->width,bit,srcImage->bpp)]+
        algorithm[1][2]*srcImage->data[Index(px,y,srcImage->width,bit,srcImage->bpp)]+
        algorithm[2][0]*srcImage->data[Index(mx,py,srcImage->width,bit,srcImage->bpp)]+
        algorithm[2][1]*srcImage->data[Index(x,py,srcImage->width,bit,srcImage->bpp)]+
        algorithm[2][2]*srcImage->data[Index(px,py,srcImage->width,bit,srcImage->bpp)];
    //printf("returning %d\n", result);
    return result;
}

//convolute:  Applies a kernel matrix to an image
//Parameters: srcImage: The image being convoluted
//            destImage: A pointer to a  pre-allocated (including space for the pixel array) structure to receive the convoluted image.  It should be the same size as srcImage
//            algorithm: The kernel matrix to use for the convolution
//Returns: Nothing
void convolute(Image* srcImage,Image* destImage,Matrix algorithm){
    int row,pix,bit,span;
    span=srcImage->bpp*srcImage->bpp;
    for (row=0;row<srcImage->height;row++){
        for (pix=0;pix<srcImage->width;pix++){
            for (bit=0;bit<srcImage->bpp;bit++){
                destImage->data[Index(pix,row,srcImage->width,bit,srcImage->bpp)]=getPixelValue(srcImage,pix,row,bit,algorithm);
            }
        }
    }
}


void convolute_bak(Image* srcImage,Image* destImage,Matrix algorithm){
    //int row,pix,bit,span;
    //span=srcImage->bpp*srcImage->bpp;



    /* ******************************************* */

    //printf("Image info: srcImage->height=%d srcImage->width=%d srcImage->bpp=%d\n", imgR, imgPix, imgBit);

    //printf("total rows = %d\n", imgR);

    int thread_count = 50;
    printf("total threads = %d\n", thread_count);
    int rows_per_thread = srcImage->height / thread_count;
    //printf("rows_per_thread = %d, leftover = %d\n", rows_per_thread, leftover);
    //printf("----------------------------------\n");


    struct ImageArgs *void_args = (struct ImageArgs*) malloc(sizeof(struct ImageArgs));
    void_args->srcImage = srcImage;
    void_args->destImage = destImage;
    void_args->total_threads = thread_count;
    void_args->rank = 0;
    //void_args->local_start = 0;
    //void_args->local_end = void_args->local_start + rows_per_thread;


    pthread_t *threads = (pthread_t* )malloc(thread_count * sizeof(pthread_t));
    pthread_t extra;

    struct ImageArgs *avoid_race;

    for (int i = 0; i < thread_count; i++){
        void_args->rank = i;
        avoid_race = (struct ImageArgs*) malloc(sizeof(struct ImageArgs));
        memcpy(avoid_race, void_args, sizeof(struct ImageArgs));
        //printf("Spawning thread %d local_start: %d local_end %d\n", i,void_args->local_start, void_args->local_end);
        //printf("Spawning thread %d local_start: %d local_end: %d\n", i, i * rows_per_thread, (i+1) * rows_per_thread);
        pthread_create(&threads[i], NULL, convolute_loop, (void*)avoid_race);

        
    }

    /*if (leftover > 0){
        printf("Reached here\n");
        printf("void_args->local_start = %d\n", void_args->local_start + leftover);
        void_args->local_start = void_args->local_start + leftover;
        printf("void_args->local_end = %d\n", void_args->local_end + leftover);
        void_args->local_end = void_args->local_end + leftover;
        printf("Spawning extra thread to handle the last %d rows\n", leftover);
        //pthread_create(&extra, NULL, convolute_loop, (void*)void_args); 

    }*/

    printf("Joining threads\n");
    for (int k = 0; k < thread_count; k++){

        pthread_join(threads[k], NULL);

    }
    /*if (leftover > 0){
        pthread_join(extra, NULL);
    }
    printf("Freeing threads\n");
    free(threads);*/
}


void* convolute_loop(void* img_args){

    int row,pix,bit;//span;

    struct ImageArgs *args;
    args = (struct ImageArgs*) img_args;

    //span=args->srcImage->bpp*args->srcImage->bpp;

    int imgR = args->srcImage->height;
    int imgPix = args->srcImage->width;
    int imgBit = args->srcImage->bpp;

    /* Need to split up the work among each thread */    

    //printf("After Thread Launch\n");

    int rows_per_thread = imgR / args->total_threads; 
    int leftover = args->srcImage->height % args->total_threads;
    int local_start;
    int local_end;

    local_start = (args->rank * rows_per_thread); 
    if (args->rank == args->total_threads-1){
        printf("INSIDE CONDITION %d * %d + %d\n", args->rank+1, rows_per_thread, leftover);
        local_end = args->srcImage->height;
        printf("###########################################\n");
        printf("Setting last to %d\n", local_end);
        printf("###########################################\n");
    }
    else {
        local_end = (args->rank+1) * rows_per_thread;       
    }

    printf("thread: %d local_start=%d local_end=%d (exclusive)\n", args->rank, local_start, local_end);
    for (row=local_start;row<local_end;row++){
        //printf("Row = %d\n", row);
        for (pix=0;pix<imgPix;pix++){
            for (bit=0;bit<imgBit;bit++){
                //printf("bit= %d row= %d pix= %d\n", bit, row, pix);
                //printf("Working on pixel\n");

                args->destImage->data[Index(pix,row,imgPix,bit,imgBit)]=getPixelValue(args->srcImage,pix,row,bit,algorithms[type]);
            }
        }
    }
    return 0;
}


//Usage: Prints usage information for the program
//Returns: -1
int Usage(){
    printf("Usage: image <filename> <type>\n\twhere type is one of (edge,sharpen,blur,gauss,emboss,identity)\n");
    return -1;
}

//GetKernelType: Converts the string name of a convolution into a value from the KernelTypes enumeration
//Parameters: type: A string representation of the type
//Returns: an appropriate entry from the KernelTypes enumeration, defaults to IDENTITY, which does nothing but copy the image.
enum KernelTypes GetKernelType(char* type){
    if (!strcmp(type,"edge")) return EDGE;
    else if (!strcmp(type,"sharpen")) return SHARPEN;
    else if (!strcmp(type,"blur")) return BLUR;
    else if (!strcmp(type,"gauss")) return GAUSE_BLUR;
    else if (!strcmp(type,"emboss")) return EMBOSS;
    else return IDENTITY;
}

//main:
//argv is expected to take 2 arguments.  First is the source file name (can be jpg, png, bmp, tga).  Second is the lower case name of the algorithm.
int main(int argc,char** argv){
    long t1,t2;
    t1=time(NULL);

    stbi_set_flip_vertically_on_load(0); 
    if (argc!=3) return Usage();
    char* fileName=argv[1];
    if (!strcmp(argv[1],"pic4.jpg")&&!strcmp(argv[2],"gauss")){
        printf("You have applied a gaussian filter to Gauss which has caused a tear in the time-space continum.\n");
    }
    type=GetKernelType(argv[2]);

    Image srcImage,destImage,bwImage;   
    srcImage.data=stbi_load(fileName,&srcImage.width,&srcImage.height,&srcImage.bpp,0);
    if (!srcImage.data){
        printf("Error loading file %s.\n",fileName);
        return -1;
    }
    destImage.bpp=srcImage.bpp;
    destImage.height=srcImage.height;
    destImage.width=srcImage.width;
    destImage.data=malloc(sizeof(uint8_t)*destImage.width*destImage.bpp*destImage.height);

    /* */ 

 
    convolute_bak(&srcImage,&destImage,algorithms[type]);
    
    /* */
    stbi_write_png("output.png",destImage.width,destImage.height,destImage.bpp,destImage.data,destImage.bpp*destImage.width);
    stbi_image_free(srcImage.data);
    
    free(destImage.data);
    t2=time(NULL);
    printf("Took %ld seconds\n",t2-t1);
   return 0;
}