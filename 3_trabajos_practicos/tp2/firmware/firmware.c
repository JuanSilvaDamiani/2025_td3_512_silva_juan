#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/adc.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"

// Declaración de la cola para enviar temperatura desde una tarea a otra
QueueHandle_t temperatura_queue;

/**
 * Tarea que lee la temperatura del sensor interno del RP2040
 * y envía el valor en grados Celsius a través de una cola.
 */
void leotemp_task(void *pvParameters) {
    // Inicialización del módulo ADC
    adc_init();

    // Habilita el sensor de temperatura interno del RP2040.
    // Por defecto está deshabilitado para ahorrar energía.
    adc_set_temp_sensor_enabled(true);

    while (true) {
        // adc_select_input(4):
        // Función del SDK de Raspberry Pi Pico que selecciona cuál de los 
        //5 canales de entrada ADC (convertidor analógico a digital)
        // del Pico 2 voy a usar. Para activar el canal del sensor de
        // temperatura interno es el 4 y no está conectado a una pata externa del chip.
        // NB: antes de esto hay que activar el sensor de temperatura con el adc_set_temp_sensor_enabled(true);
        adc_select_input(4);

        // adc_read():  lee el valor analógico del canal ADC activo, en este caso el 4 
        // que es el sensor de temperatura interno del RP2040
        // que genera un voltaje que varía linealmente con la temperatura y a una 
        // temperatura de 27 °C, es de 0.706 V; y lo devuelve convertido
        // en un número digital mediante el convertidor ADC en  un número entre 0 y 4095.
        //Desde ahí vamos a usar la fórmula empírica 
        //   V(Volt)= valor_adc × (3.3 / 4095)
        //   V(Volt)= 0.706 V − 0.001721 × (T(°C) − 27 °C)  → despejando:
        //   T(°C) = 27 °C – ((V(Volt) - 0.706 V) / 0.001721)
        //
        // uint16_t :  Es un "entero sin signo de 16 bits" (rango: 0 a 65535), nuestro valor máximo será 4095,
        // porque el ADC del RP2040 tiene resolución de 12 bits y 2¹² = 4096
        // valor_adc : Nombre de la variable que guarda el valor de conversión leído desde el ADC.
        // Siendo que el 0 corresponde a 0.0 V y el 4095 corresponde a 3.3 V que es el voltaje de referencia,
        // y luego como 0.706 V corresponde a 27 °C porque es una referencia fija general del RP2040
        uint16_t valor_adc = adc_read();

        // Convierto valor digital a voltaje real
        float voltaje = valor_adc * 3.3f / 4095.0f;

        // Convierto voltaje a temperatura en grados Celsius
        float temperatura = 27.0f - (voltaje - 0.706f) / 0.001721f;

        // Envía el valor de la temperatura (tipo float) a la cola temperatura_queue. 
        //Luego, otra tarea puede recibirla usando xQueueReceive y hacer algo con ella
        /*
        Explicación:
        temperatura_queue
        Es una cola creada con xQueueCreate, donde se almacenan valores de tipo float (temperatura en grados Celsius en este caso). Las colas se usan para comunicar tareas entre sí en FreeRTOS.
        temperatura
        Es una variable float que contiene el valor de la temperatura calculada a partir de la lectura del sensor interno.
        &temperatura
        Se pasa la dirección de memoria de la variable temperatura. Esto se hace porque xQueueSend copia desde esa dirección el valor que se quiere poner en la cola.
        portMAX_DELAY
        Especifica cuánto tiempo debe esperar la función si la cola está llena. En este caso, portMAX_DELAY indica que debe esperar indefinidamente hasta que haya espacio disponible en la cola.
        */
        xQueueSend(temperatura_queue, &temperatura, portMAX_DELAY);

        // Espero 2 segundos antes de la siguiente lectura.
        // El retardo de 2 segundos se cuenta después de que se logró enviar a la cola. 
        // Si la cola está llena, la tarea se queda esperando indefinidamente en xQueueSend(...) 
        // y no empieza el vTaskDelay hasta que se libera espacio en la cola (es decir, 
        // cuando otra tarea recibe y saca un elemento con xQueueReceive), el xQueueSend termina con éxito.
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}

/**
 * Tarea que recibe la temperatura por cola y la muestra por consola.
 * Defino la tarea llamada printtemp_task. Como todas las tareas,
 *  recibe un parámetro genérico (void *pvParameters) que aquí no se usa.
 */
void printtemp_task(void *pvParameters) {
//Declara una variable local para almacenar la temperatura que va a recibir.
    float temp_recibida;
    while (true) {
        // Espero indefinidamente a recibir temperatura por cola
/*
•	xQueueReceive(temperatura_queue, &temp_recibida, portMAX_DELAY):
Aquí la tarea espera a recibir un dato (un valor de temperatura tipo float) 
desde la cola llamada temperatura_queue.
o	temperatura_queue es una estructura propia de FreeRTOS para
 comunicación entre tareas.
o	&temp_recibida es la dirección donde se almacenará el valor recibido.
o	portMAX_DELAY indica que la tarea esperará indefinidamente
 hasta que haya un dato disponible.
o	La función devuelve pdTRUE si recibió exitosamente un dato.
•	if (...) { printf(...); }:
Cuando recibe un dato, lo imprime por consola en formato de punto flotante
 con dos decimales, mostrando la temperatura en grados Celsius.
En resumen: Esta tarea está dedicada a recibir continuamente
 valores de temperatura que otra tarea genera y envía por cola,
  y a mostrarlos por consola.
  Está sincronizada con la tarea lectora a través de la cola,
   funcionando de forma cooperativa y sin bloquear el sistema.
 */

        if (xQueueReceive(temperatura_queue, &temp_recibida, portMAX_DELAY) == pdTRUE) {
            printf("Temperatura interna: %.2f °C\n", temp_recibida);
        }
    }
}

/**
 * Función principal que crea las tareas y arranca FreeRTOS.
 */
int main() {
    // Inicializo salida estándar (USB o UART según configuración de SDK)
    stdio_init_all();

    // Creo la cola para un elemento tipo float
    temperatura_queue = xQueueCreate(1, sizeof(float));

    // Creo la tarea que lee el ADC y manda por cola
    xTaskCreate(leotemp_task, "LecturaTemp", 256, NULL, 1, NULL);

    // Creo la tarea que recibe por cola e imprime por consola
    xTaskCreate(printtemp_task, "ImprimirTemp", 256, NULL, 1, NULL);

    // Inicio el scheduler de FreeRTOS
    vTaskStartScheduler();

    // Nunca debería llegar aquí
    while (1);
}

