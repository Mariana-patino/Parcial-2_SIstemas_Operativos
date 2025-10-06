#  Plataforma de Edición de Imágenes en C

Este proyecto implementa una plataforma de edición de imágenes en C que permite aplicar distintos filtros y transformaciones (brillo, desenfoque, rotación, detección de bordes, etc.) utilizando procesamiento concurrente.  

##  Integrantes:

- Mariana Patiño Arboleda
- Samuel Orozco

## Otros:

- Materia: Sistemas Operativos
- Reto: Edición de imágenes con concurrencia
- Fecha: 5 de Octubre 2025

##  Requisitos previos

Antes de compilar, tener instalado:

- **GCC** (compilador de C)
- **WSL** o entorno Linux
- Archivos de cabecera externos:
  - [`stb_image.h`](https://raw.githubusercontent.com/nothings/stb/master/stb_image.h)
  - [`stb_image_write.h`](https://raw.githubusercontent.com/nothings/stb/master/stb_image_write.h)

## Compilación

gcc -o img img_extendido.c -pthread -lm

## Ejecución
Para iniciar el programa:

./img
