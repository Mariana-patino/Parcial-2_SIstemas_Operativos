#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>
#include <math.h>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

typedef struct {
    int ancho;       
    int alto;    
    int canales;     
    unsigned char*** pixeles; 
} ImagenInfo;

// UTILIDADES DE MEMORIA

unsigned char*** asignarPixeles(int alto, int ancho, int canales) {
    unsigned char*** pix = (unsigned char***)malloc(alto * sizeof(unsigned char**));
    if (!pix) return NULL;
    for (int y = 0; y < alto; y++) {
        pix[y] = (unsigned char**)malloc(ancho * sizeof(unsigned char*));
        if (!pix[y]) {
            for (int yy = 0; yy < y; yy++) free(pix[yy]);
            free(pix);
            return NULL;
        }
        for (int x = 0; x < ancho; x++) {
            pix[y][x] = (unsigned char*)malloc(canales * sizeof(unsigned char));
            if (!pix[y][x]) {
                for (int xx = 0; xx < x; xx++) free(pix[y][xx]);
                for (int yy = 0; yy <= y; yy++) free(pix[yy]);
                free(pix);
                return NULL;
            }
        }
    }
    return pix;
}

void liberarPixelesMem(unsigned char*** pix, int alto, int ancho) {
    if (!pix) return;
    for (int y = 0; y < alto; y++) {
        for (int x = 0; x < ancho; x++) {
            free(pix[y][x]);
        }
        free(pix[y]);
    }
    free(pix);
}


void liberarImagen(ImagenInfo* info) {
    if (info->pixeles) {
        liberarPixelesMem(info->pixeles, info->alto, info->ancho);
        info->pixeles = NULL;
    }
    info->ancho = 0;
    info->alto = 0;
    info->canales = 0;
}

//  CARGA Y GUARDADO 

int cargarImagen(const char* ruta, ImagenInfo* info) {
    int canales;
    unsigned char* datos = stbi_load(ruta, &info->ancho, &info->alto, &canales, 0);
    if (!datos) {
        fprintf(stderr, "Error al cargar imagen: %s\n", ruta);
        return 0;
    }
    info->canales = (canales == 1 || canales == 3) ? canales : 1;

    info->pixeles = (unsigned char***)malloc(info->alto * sizeof(unsigned char**));
    if (!info->pixeles) {
        fprintf(stderr, "Error de memoria al asignar filas\n");
        stbi_image_free(datos);
        return 0;
    }
    for (int y = 0; y < info->alto; y++) {
        info->pixeles[y] = (unsigned char**)malloc(info->ancho * sizeof(unsigned char*));
        if (!info->pixeles[y]) {
            fprintf(stderr, "Error de memoria al asignar columnas\n");
            liberarImagen(info);
            stbi_image_free(datos);
            return 0;
        }
        for (int x = 0; x < info->ancho; x++) {
            info->pixeles[y][x] = (unsigned char*)malloc(info->canales * sizeof(unsigned char));
            if (!info->pixeles[y][x]) {
                fprintf(stderr, "Error de memoria al asignar canales\n");
                liberarImagen(info);
                stbi_image_free(datos);
                return 0;
            }
            for (int c = 0; c < info->canales; c++) {
                info->pixeles[y][x][c] = datos[(y * info->ancho + x) * info->canales + c];
            }
        }
    }
    stbi_image_free(datos);
    printf("Imagen cargada: %dx%d, %d canales (%s)\n", info->ancho, info->alto,
           info->canales, info->canales == 1 ? "grises" : "RGB");
    return 1;
}

int guardarPNG(const ImagenInfo* info, const char* rutaSalida) {
    if (!info->pixeles) {
        fprintf(stderr, "No hay imagen para guardar.\n");
        return 0;
    }
    unsigned char* datos1D = (unsigned char*)malloc(info->ancho * info->alto * info->canales);
    if (!datos1D) {
        fprintf(stderr, "Error de memoria al aplanar imagen\n");
        return 0;
    }
    for (int y = 0; y < info->alto; y++) {
        for (int x = 0; x < info->ancho; x++) {
            for (int c = 0; c < info->canales; c++) {
                datos1D[(y * info->ancho + x) * info->canales + c] = info->pixeles[y][x][c];
            }
        }
    }
    int resultado = stbi_write_png(rutaSalida, info->ancho, info->alto, info->canales,
                                   datos1D, info->ancho * info->canales);
    free(datos1D);
    if (resultado) {
        printf("Imagen guardada en: %s (%s)\n", rutaSalida,
               info->canales == 1 ? "grises" : "RGB");
        return 1;
    } else {
        fprintf(stderr, "Error al guardar PNG: %s\n", rutaSalida);
        return 0;
    }
}

//  MOSTRAR MATRIZ 

void mostrarMatriz(const ImagenInfo* info) {
    if (!info->pixeles) {
        printf("No hay imagen cargada.\n");
        return;
    }
    printf("Matriz de la imagen (primeras 10 filas):\n");
    for (int y = 0; y < info->alto && y < 10; y++) {
        for (int x = 0; x < info->ancho; x++) {
            if (info->canales == 1) {
                printf("%3u ", info->pixeles[y][x][0]);
            } else {
                printf("(%3u,%3u,%3u) ", info->pixeles[y][x][0], info->pixeles[y][x][1],
                       info->pixeles[y][x][2]);
            }
        }
        printf("\n");
    }
    if (info->alto > 10) printf("... (más filas)\n");
}

// BRILLO 

typedef struct {
    unsigned char*** pixeles;
    int inicio;
    int fin;
    int ancho;
    int canales;
    int delta;
} BrilloArgs;

void* ajustarBrilloHilo(void* args) {
    BrilloArgs* bArgs = (BrilloArgs*)args;
    for (int y = bArgs->inicio; y < bArgs->fin; y++) {
        for (int x = 0; x < bArgs->ancho; x++) {
            for (int c = 0; c < bArgs->canales; c++) {
                int nuevoValor = bArgs->pixeles[y][x][c] + bArgs->delta;
                bArgs->pixeles[y][x][c] = (unsigned char)(nuevoValor < 0 ? 0 :
                                                          (nuevoValor > 255 ? 255 : nuevoValor));
            }
        }
    }
    return NULL;
}

void ajustarBrilloConcurrente(ImagenInfo* info, int delta) {
    if (!info->pixeles) {
        printf("No hay imagen cargada.\n");
        return;
    }
    const int numHilos = 2;
    pthread_t hilos[numHilos];
    BrilloArgs args[numHilos];
    int filasPorHilo = (int)ceil((double)info->alto / numHilos);
    for (int i = 0; i < numHilos; i++) {
        args[i].pixeles = info->pixeles;
        args[i].inicio = i * filasPorHilo;
        args[i].fin = (i + 1) * filasPorHilo < info->alto ? (i + 1) * filasPorHilo : info->alto;
        args[i].ancho = info->ancho;
        args[i].canales = info->canales;
        args[i].delta = delta;
        if (pthread_create(&hilos[i], NULL, ajustarBrilloHilo, &args[i]) != 0) {
            fprintf(stderr, "Error al crear hilo %d\n", i);
            return;
        }
    }
    for (int i = 0; i < numHilos; i++) {
        pthread_join(hilos[i], NULL);
    }
    printf("Brillo ajustado concurrentemente con %d hilos (%s).\n", numHilos,
           info->canales == 1 ? "grises" : "RGB");
}

//  FUNCIONES NUEVAS CONCURRENTE

// Helper: clamp
static inline unsigned char clamp255(int v) {
    if (v < 0) return 0;
    if (v > 255) return 255;
    return (unsigned char)v;
}

//  CONVOLUCIÓN GAUSSIANA 


float* generarKernelGaussiano(int tamKernel, float sigma) {
    int centro = tamKernel / 2;
    float sum = 0.0f;
    float* kernel = (float*)malloc(tamKernel * tamKernel * sizeof(float));
    if (!kernel) return NULL;
    for (int y = 0; y < tamKernel; y++) {
        for (int x = 0; x < tamKernel; x++) {
            int dx = x - centro;
            int dy = y - centro;
            float val = expf(-(dx*dx + dy*dy) / (2.0f * sigma * sigma));
            kernel[y*tamKernel + x] = val;
            sum += val;
        }
    }
    if (sum != 0.0f) {
        for (int i = 0; i < tamKernel * tamKernel; i++) kernel[i] /= sum;
    }
    return kernel;
}


typedef struct {
    ImagenInfo* src;
    unsigned char*** dst;
    int inicio, fin;
    int tamKernel;
    float* kernel;
} ConvArgs;


static inline unsigned char getPixelReplicate(ImagenInfo* src, int y, int x, int c) {
    if (y < 0) y = 0;
    if (y >= src->alto) y = src->alto - 1;
    if (x < 0) x = 0;
    if (x >= src->ancho) x = src->ancho - 1;
    return src->pixeles[y][x][c];
}

void* hiloConvolucion(void* args) {
    ConvArgs* a = (ConvArgs*)args;
    int tam = a->tamKernel;
    int centro = tam / 2;
    for (int y = a->inicio; y < a->fin; y++) {
        for (int x = 0; x < a->src->ancho; x++) {
            for (int c = 0; c < a->src->canales; c++) {
                float acc = 0.0f;
                for (int ky = 0; ky < tam; ky++) {
                    for (int kx = 0; kx < tam; kx++) {
                        int yy = y + (ky - centro);
                        int xx = x + (kx - centro);
                        unsigned char val = getPixelReplicate(a->src, yy, xx, c);
                        acc += val * a->kernel[ky * tam + kx];
                    }
                }
                int v = (int)roundf(acc);
                a->dst[y][x][c] = clamp255(v);
            }
        }
    }
    return NULL;
}

void aplicarConvolucionGaussiana(ImagenInfo* info, int tamKernel, float sigma, int numHilos) {
    if (!info->pixeles) {
        printf("No hay imagen cargada.\n");
        return;
    }
    if (tamKernel % 2 == 0 || tamKernel < 3) {
        printf("Tamano de kernel invalido. Use un numero impar >= 3.\n");
        return;
    }
    if (sigma <= 0.0f) {
        printf("Sigma debe ser > 0.\n");
        return;
    }
    float* kernel = generarKernelGaussiano(tamKernel, sigma);
    if (!kernel) {
        fprintf(stderr, "No se pudo generar kernel Gaussiano.\n");
        return;
    }

 
    unsigned char*** dst = asignarPixeles(info->alto, info->ancho, info->canales);
    if (!dst) {
        fprintf(stderr, "Error al asignar memoria para imagen destino (convolucion).\n");
        free(kernel);
        return;
    }

    if (numHilos < 1) numHilos = 1;
    if (numHilos > info->alto) numHilos = info->alto;
    pthread_t* hilos = (pthread_t*)malloc(numHilos * sizeof(pthread_t));
    ConvArgs* args = (ConvArgs*)malloc(numHilos * sizeof(ConvArgs));
    int filasPor = (int)ceil((double)info->alto / numHilos);

    for (int i = 0; i < numHilos; i++) {
        args[i].src = info;
        args[i].dst = dst;
        args[i].inicio = i * filasPor;
        args[i].fin = (i + 1) * filasPor < info->alto ? (i + 1) * filasPor : info->alto;
        args[i].tamKernel = tamKernel;
        args[i].kernel = kernel;
        if (pthread_create(&hilos[i], NULL, hiloConvolucion, &args[i]) != 0) {
            fprintf(stderr, "Error al crear hilo de convolucion %d\n", i);
            
            for (int j = i; j < numHilos; j++) {
                hiloConvolucion(&args[j]);
            }
            break;
        }
    }
    for (int i = 0; i < numHilos; i++) {
        pthread_join(hilos[i], NULL);
    }

  
    liberarPixelesMem(info->pixeles, info->alto, info->ancho);
    info->pixeles = dst;

    free(hilos);
    free(args);
    free(kernel);
    printf("Convolucion Gaussiana aplicada (kernel=%d, sigma=%.2f) con %d hilos.\n", tamKernel, sigma, numHilos);
}

//  ROTACIÓN 

void bilinearInterpolate(ImagenInfo* src, double fy, double fx, unsigned char* out) {
    int x0 = (int)floor(fx);
    int y0 = (int)floor(fy);
    int x1 = x0 + 1;
    int y1 = y0 + 1;
    double wx = fx - x0;
    double wy = fy - y0;
    for (int c = 0; c < src->canales; c++) {
       
        int vx00 = getPixelReplicate(src, y0, x0, c);
        int vx10 = getPixelReplicate(src, y0, x1, c);
        int vx01 = getPixelReplicate(src, y1, x0, c);
        int vx11 = getPixelReplicate(src, y1, x1, c);
        double a = vx00 * (1 - wx) + vx10 * wx;
        double b = vx01 * (1 - wx) + vx11 * wx;
        double val = a * (1 - wy) + b * wy;
        out[c] = clamp255((int)round(val));
    }
}


typedef struct {
    ImagenInfo* src;
    unsigned char*** dst;
    int dstAncho;
    int dstAlto;
    double cx_src, cy_src; 
    double cx_dst, cy_dst;
    double angleRad; 
    int inicio, fin; 
} RotArgs;

void* hiloRotacion(void* arg) {
    RotArgs* a = (RotArgs*)arg;
    for (int y = a->inicio; y < a->fin; y++) {
        for (int x = 0; x < a->dstAncho; x++) {
          
            double dx = x - a->cx_dst;
            double dy = y - a->cy_dst;
           
            double srcx = dx * cos(a->angleRad) + dy * sin(a->angleRad) + a->cx_src;
            double srcy = -dx * sin(a->angleRad) + dy * cos(a->angleRad) + a->cy_src;
        
            bilinearInterpolate(a->src, srcy, srcx, a->dst[y][x]);
        }
    }
    return NULL;
}

void rotarImagen(ImagenInfo* info, double anguloGrados, int numHilos) {
    if (!info->pixeles) { printf("No hay imagen cargada.\n"); return; }

    double ang = fmod(anguloGrados, 360.0);
    double rad = ang * M_PI / 180.0;
    double cosA = fabs(cos(rad));
    double sinA = fabs(sin(rad));
    int newW = (int)ceil(info->ancho * cosA + info->alto * sinA);
    int newH = (int)ceil(info->ancho * sinA + info->alto * cosA);

    unsigned char*** dst = asignarPixeles(newH, newW, info->canales);
    if (!dst) { fprintf(stderr, "Error al asignar memoria para rotacion.\n"); return; }

    double cx_src = (info->ancho - 1) / 2.0;
    double cy_src = (info->alto - 1) / 2.0;
    double cx_dst = (newW - 1) / 2.0;
    double cy_dst = (newH - 1) / 2.0;

   
    double invRad = -rad;

    if (numHilos < 1) numHilos = 1;
    if (numHilos > newH) numHilos = newH;
    pthread_t* hilos = (pthread_t*)malloc(numHilos * sizeof(pthread_t));
    RotArgs* args = (RotArgs*)malloc(numHilos * sizeof(RotArgs));
    int filasPor = (int)ceil((double)newH / numHilos);

    for (int i = 0; i < numHilos; i++) {
        args[i].src = info;
        args[i].dst = dst;
        args[i].dstAncho = newW;
        args[i].dstAlto = newH;
        args[i].cx_src = cx_src;
        args[i].cy_src = cy_src;
        args[i].cx_dst = cx_dst;
        args[i].cy_dst = cy_dst;
        args[i].angleRad = invRad;
        args[i].inicio = i * filasPor;
        args[i].fin = (i + 1) * filasPor < newH ? (i + 1) * filasPor : newH;
        if (pthread_create(&hilos[i], NULL, hiloRotacion, &args[i]) != 0) {
            fprintf(stderr, "Error al crear hilo de rotacion %d\n", i);
            for (int j = i; j < numHilos; j++) hiloRotacion(&args[j]);
            break;
        }
    }
    for (int i = 0; i < numHilos; i++) pthread_join(hilos[i], NULL);


    liberarPixelesMem(info->pixeles, info->alto, info->ancho);
    info->pixeles = dst;
    info->ancho = newW;
    info->alto = newH;

    free(hilos);
    free(args);
    printf("Imagen rotada %.2f grados. Nuevo tamaño: %dx%d (hilos=%d)\n", anguloGrados, info->ancho, info->alto, numHilos);
}

//  SOBEL 

unsigned char rgbToGrayPixel(unsigned char r, unsigned char g, unsigned char b) {
    return (unsigned char)round((r + g + b) / 3.0);
}


typedef struct {
    ImagenInfo* src;
    unsigned char*** dst; 
    int inicio, fin;
} SobelArgs;

void* hiloSobel(void* arg) {
    SobelArgs* a = (SobelArgs*)arg;
    
    int Gx[3][3] = {
        {-1, 0, 1},
        {-2, 0, 2},
        {-1, 0, 1}
    };
    int Gy[3][3] = {
        {-1, -2, -1},
        { 0,  0,  0},
        { 1,  2,  1}
    };
    for (int y = a->inicio; y < a->fin; y++) {
        for (int x = 0; x < a->src->ancho; x++) {
          
            float sumx = 0.0f, sumy = 0.0f;
            for (int ky = -1; ky <= 1; ky++) {
                for (int kx = -1; kx <= 1; kx++) {
                    int yy = y + ky;
                    int xx = x + kx;
                    
                    if (yy < 0) yy = 0;
                    if (yy >= a->src->alto) yy = a->src->alto - 1;
                    if (xx < 0) xx = 0;
                    if (xx >= a->src->ancho) xx = a->src->ancho - 1;
                    unsigned char gval;
                    if (a->src->canales == 1) {
                        gval = a->src->pixeles[yy][xx][0];
                    } else {
                        gval = rgbToGrayPixel(a->src->pixeles[yy][xx][0], a->src->pixeles[yy][xx][1], a->src->pixeles[yy][xx][2]);
                    }
                    sumx += Gx[ky+1][kx+1] * gval;
                    sumy += Gy[ky+1][kx+1] * gval;
                }
            }
            int mag = (int)round(sqrtf(sumx*sumx + sumy*sumy));
            unsigned char v = clamp255(mag);
           
            for (int c = 0; c < a->src->canales; c++) {
                a->dst[y][x][c] = v;
            }
        }
    }
    return NULL;
}

void detectarBordesSobel(ImagenInfo* info, int numHilos) {
    if (!info->pixeles) { printf("No hay imagen cargada.\n"); return; }
    unsigned char*** dst = asignarPixeles(info->alto, info->ancho, info->canales);
    if (!dst) { fprintf(stderr, "Error al asignar memoria para Sobel.\n"); return; }
    if (numHilos < 1) numHilos = 1;
    if (numHilos > info->alto) numHilos = info->alto;
    pthread_t* hilos = (pthread_t*)malloc(numHilos * sizeof(pthread_t));
    SobelArgs* args = (SobelArgs*)malloc(numHilos * sizeof(SobelArgs));
    int filasPor = (int)ceil((double)info->alto / numHilos);
    for (int i = 0; i < numHilos; i++) {
        args[i].src = info;
        args[i].dst = dst;
        args[i].inicio = i * filasPor;
        args[i].fin = (i + 1) * filasPor < info->alto ? (i + 1) * filasPor : info->alto;
        if (pthread_create(&hilos[i], NULL, hiloSobel, &args[i]) != 0) {
            fprintf(stderr, "Error al crear hilo Sobel %d\n", i);
            for (int j = i; j < numHilos; j++) hiloSobel(&args[j]);
            break;
        }
    }
    for (int i = 0; i < numHilos; i++) pthread_join(hilos[i], NULL);

    liberarPixelesMem(info->pixeles, info->alto, info->ancho);
    info->pixeles = dst;

    free(hilos);
    free(args);
    printf("Detector de bordes (Sobel) aplicado con %d hilos.\n", numHilos);
}

//REDIMENSIONAR 

typedef struct {
    ImagenInfo* src;
    unsigned char*** dst;
    int dstAncho;
    int dstAlto;
    int inicio, fin;
} ResizeArgs;

void* hiloResize(void* arg) {
    ResizeArgs* a = (ResizeArgs*)arg;
    double scaleX = (double)a->src->ancho / a->dstAncho;
    double scaleY = (double)a->src->alto / a->dstAlto;
    for (int y = a->inicio; y < a->fin; y++) {
        for (int x = 0; x < a->dstAncho; x++) {
            double srcx = (x + 0.5) * scaleX - 0.5;
            double srcy = (y + 0.5) * scaleY - 0.5;
            bilinearInterpolate(a->src, srcy, srcx, a->dst[y][x]);
        }
    }
    return NULL;
}

void redimensionarImagen(ImagenInfo* info, int nuevoAncho, int nuevoAlto, int numHilos) {
    if (!info->pixeles) { printf("No hay imagen cargada.\n"); return; }
    if (nuevoAncho <= 0 || nuevoAlto <= 0) { printf("Tamaño invalido.\n"); return; }
    unsigned char*** dst = asignarPixeles(nuevoAlto, nuevoAncho, info->canales);
    if (!dst) { fprintf(stderr, "Error al asignar memoria para resize.\n"); return; }
    if (numHilos < 1) numHilos = 1;
    if (numHilos > nuevoAlto) numHilos = nuevoAlto;
    pthread_t* hilos = (pthread_t*)malloc(numHilos * sizeof(pthread_t));
    ResizeArgs* args = (ResizeArgs*)malloc(numHilos * sizeof(ResizeArgs));
    int filasPor = (int)ceil((double)nuevoAlto / numHilos);
    for (int i = 0; i < numHilos; i++) {
        args[i].src = info;
        args[i].dst = dst;
        args[i].dstAncho = nuevoAncho;
        args[i].dstAlto = nuevoAlto;
        args[i].inicio = i * filasPor;
        args[i].fin = (i + 1) * filasPor < nuevoAlto ? (i + 1) * filasPor : nuevoAlto;
        if (pthread_create(&hilos[i], NULL, hiloResize, &args[i]) != 0) {
            fprintf(stderr, "Error al crear hilo Resize %d\n", i);
            for (int j = i; j < numHilos; j++) hiloResize(&args[j]);
            break;
        }
    }
    for (int i = 0; i < numHilos; i++) pthread_join(hilos[i], NULL);

    liberarPixelesMem(info->pixeles, info->alto, info->ancho);
    info->pixeles = dst;
    info->ancho = nuevoAncho;
    info->alto = nuevoAlto;

    free(hilos);
    free(args);
    printf("Imagen redimensionada a %dx%d con %d hilos.\n", info->ancho, info->alto, numHilos);
}

//  MENÚ E INTERFAZ

void mostrarMenu() {
    printf("\n--- Plataforma de Edición de Imágenes ---\n");
    printf("1. Cargar imagen PNG\n");
    printf("2. Mostrar matriz de píxeles\n");
    printf("3. Guardar como PNG\n");
    printf("4. Ajustar brillo (+/- valor) concurrentemente\n");
    printf("5. Aplicar convolucion Gaussiana (blur)\n");
    printf("6. Rotar imagen (grados, bilinear)\n");
    printf("7. Detectar bordes (Sobel)\n");
    printf("8. Redimensionar imagen (resize bilinear)\n");
    printf("9. Salir\n");
    printf("Opción: ");
}

int pedirInt(const char* mensaje) {
    int val;
    printf("%s", mensaje);
    if (scanf("%d", &val) != 1) {
        while (getchar() != '\n');
        return INT_MIN;
    }
    while (getchar() != '\n');
    return val;
}

double pedirDouble(const char* mensaje) {
    double v;
    printf("%s", mensaje);
    if (scanf("%lf", &v) != 1) {
        while (getchar() != '\n');
        return NAN;
    }
    while (getchar() != '\n');
    return v;
}

int main(int argc, char* argv[]) {
    ImagenInfo imagen = {0, 0, 0, NULL};
    char ruta[512] = {0};

    if (argc > 1) {
        strncpy(ruta, argv[1], sizeof(ruta) - 1);
        if (!cargarImagen(ruta, &imagen)) return EXIT_FAILURE;
    }

    int opcion;
    while (1) {
        mostrarMenu();
        if (scanf("%d", &opcion) != 1) {
            while (getchar() != '\n');
            printf("Entrada inválida.\n");
            continue;
        }
        while (getchar() != '\n');

        switch (opcion) {
            case 1: {
                printf("Ingresa la ruta del archivo PNG: ");
                if (fgets(ruta, sizeof(ruta), stdin) == NULL) { printf("Error al leer ruta.\n"); continue; }
                ruta[strcspn(ruta, "\n")] = 0;
                liberarImagen(&imagen);
                if (!cargarImagen(ruta, &imagen)) continue;
                break;
            }
            case 2:
                mostrarMatriz(&imagen);
                break;
            case 3: {
                char salida[512];
                printf("Nombre del archivo PNG de salida: ");
                if (fgets(salida, sizeof(salida), stdin) == NULL) { printf("Error al leer ruta.\n"); continue; }
                salida[strcspn(salida, "\n")] = 0;
                guardarPNG(&imagen, salida);
                break;
            }
            case 4: {
                int delta;
                printf("Valor de ajuste de brillo (+ para más claro, - para más oscuro): ");
                if (scanf("%d", &delta) != 1) { while (getchar() != '\n'); printf("Entrada inválida.\n"); continue; }
                while (getchar() != '\n');
                ajustarBrilloConcurrente(&imagen, delta);
                break;
            }
            case 5: { // Convolucion Gaussiana
                if (!imagen.pixeles) { printf("No hay imagen cargada.\n"); break; }
                int tam = pedirInt("Tamano del kernel (impar, e.g., 3 o 5): ");
                if (tam == INT_MIN) { printf("Entrada invalida.\n"); break; }
                double sigma = pedirDouble("Sigma para kernel Gaussiano (e.g., 1.0): ");
                if (isnan(sigma)) { printf("Entrada invalida.\n"); break; }
                int nh = pedirInt("Numero de hilos a usar: ");
                if (nh == INT_MIN) { printf("Entrada invalida.\n"); break; }
                aplicarConvolucionGaussiana(&imagen, tam, (float)sigma, nh);
                break;
            }
            case 6: { // Rotar
                if (!imagen.pixeles) { printf("No hay imagen cargada.\n"); break; }
                double ang = pedirDouble("Angulo en grados (e.g., 90, 45, -30): ");
                if (isnan(ang)) { printf("Entrada invalida.\n"); break; }
                int nh = pedirInt("Numero de hilos a usar: ");
                if (nh == INT_MIN) { printf("Entrada invalida.\n"); break; }
                rotarImagen(&imagen, ang, nh);
                break;
            }
            case 7: { // Sobel
                if (!imagen.pixeles) { printf("No hay imagen cargada.\n"); break; }
                int nh = pedirInt("Numero de hilos a usar: ");
                if (nh == INT_MIN) { printf("Entrada invalida.\n"); break; }
                detectarBordesSobel(&imagen, nh);
                break;
            }
            case 8: { // Resize
                if (!imagen.pixeles) { printf("No hay imagen cargada.\n"); break; }
                int nw = pedirInt("Nuevo ancho: ");
                if (nw == INT_MIN) { printf("Entrada invalida.\n"); break; }
                int nhgt = pedirInt("Nuevo alto: ");
                if (nhgt == INT_MIN) { printf("Entrada invalida.\n"); break; }
                int nh = pedirInt("Numero de hilos a usar: ");
                if (nh == INT_MIN) { printf("Entrada invalida.\n"); break; }
                redimensionarImagen(&imagen, nw, nhgt, nh);
                break;
            }
            case 9:
                liberarImagen(&imagen);
                printf("¡Adiós!\n");
                return EXIT_SUCCESS;
            default:
                printf("Opción inválida.\n");
        }
    }

    liberarImagen(&imagen);
    return EXIT_SUCCESS;
}
