#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/adc.h"
#include "hardware/irq.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"

//Tp2 consigna 2

/* Declaración de una cola de FreeRTOS para enviar datos del ADC desde una interrupción a una tarea.  
QueueHandle_t es el "manejador de cola"
cola_datos_adc es una variable global que guarda el "handle" de la cola que va a transportar los valores leídos desde el ADC  */ 
QueueHandle_t cola_datos_adc;

/**
 * Tarea que dispara la conversión ADC periódicamente.
 */
void tarea_dispara_adc(void *pvParameters) {
    adc_init();
    adc_gpio_init(26); /* Habilita el GPIO26 que es uno de los 3 canales internos, 
    para funcionar como entrada analógica para el canal 0 del ADC*/
    adc_select_input(0); // Le indica al ADC que la próxima conversión se haga sobre el canal 0
    
  // Configura el FIFO del ADC
  /*El ADC tiene buffer de datos (FIFO) donde guarda los resultados de las conversiones. Asi se pueden leer los resultados más tarde que es bueno para trabajar con interrupciones.
La Sintaxis y parámetros son:
adc_fifo_setup(
    bool en,                // Habilita o deshabilita el FIFO
    bool dreq_en,           // Genera señal DREQ cuando FIFO no está vacío (para DMA)
    uint8_t dreq_thresh,    // Umbral de elementos en FIFO para activar DREQ
    bool err_in_fifo,       // Si true, errores van al FIFO también
    bool byte_shift         // Si true, resultado se alinea a 8 bits (descarta LSBs)
);
*/
    adc_fifo_setup(
        true,    // Habilitar FIFO
        false,   // No usar DREQ
        1,       // Umbral para DREQ (no importa aquí)
        false,   // No incluir errores en el FIFO
        false    // No desplazar bits (usar resolución completa)
    );

    adc_fifo_drain();         // Limpia cualquier dato previo en el FIFO
    adc_irq_clear();          // Limpia bandera de interrupción
    adc_irq_set_enabled(true); // Habilita la interrupción del ADC

    while (true) {
        adc_run(true);                      // Habilita el ADC
        adc_hw->cs |= ADC_CS_START_ONCE_BITS;  // Inicia una conversión
        /*Forma directa de manipular los registros del hardware del ADC del RP2040. 
        El Raspberry Pi Pico define esas estructuras y macros para facilitar el acceso directo al hardware.
        adc_hw : es un puntero a la estructura que mapea los registros del hardware del ADC del RP2040
        cs : es el registro de control y estado del ADC en el hardware del RP2040
        ADC_CS_START_ONCE_BITS : es una máscara con el bit que el micro RP2040 reconoce para iniciar la 
        conversión de forma puntual
        */
        vTaskDelay(pdMS_TO_TICKS(1000));   // Espera 1 segundo
    }
}

/**
 * Tarea que recibe datos del ADC por cola y los imprime.
 */
void tarea_lee_adc(void *pvParameters) {
    uint16_t valor_recibido;

    while (true) {
        if (xQueueReceive(cola_datos_adc, &valor_recibido, portMAX_DELAY) == pdTRUE) {
            printf("Valor ADC recibido: %u\n", valor_recibido);
        }
    }
}

/**
 * Manejadora de la interrupción (ISR) para el ADC. 
 * Es una función que se ejecuta automáticamente cuando ocurre la interrupción que genera el ADC 
 * cuando termina una conversión y pone un dato nuevo en su FIFO.
 * La ISR detiene el flujo normal del programa para atender el evento.
 */
void adc_isr() {
    BaseType_t xHigherPriorityTaskWoken = pdFALSE; 
    /* Intenta enviar un dato a la cola desde la ISR.
    Si con eso se desbloquea una tarea de mayor prioridad, xHigherPriorityTaskWoken se pone en pdTRUE.
    En definitiva le dice al sistema operativo si debe cambiar a otra tarea inmediatamente 
    después de salir de la interrupción.
    Luego, portYIELD_FROM_ISR() fuerza un cambio inmediato de contexto si esa bandera es pdTRUE.
    */

    if (adc_fifo_get_level() > 0) {
        uint16_t valor_adc = adc_fifo_get(); // Leo el valor del ADC desde el FIFO si es > 0
        xQueueSendFromISR(cola_datos_adc, &valor_adc, &xHigherPriorityTaskWoken);// Envío el valor a la cola desde la ISR
    }

    adc_irq_clear(); // Limpio la bandera de interrupción
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);// Cambia de contexto si hace falta
}

/**
 * Función principal.
 */
int main() {
    stdio_init_all();

    // Crear la cola
    cola_datos_adc = xQueueCreate(1, sizeof(uint16_t));

    // Configuro la interrupción del ADC
    irq_set_exclusive_handler(ADC_IRQ_FIFO, adc_isr);
    irq_set_enabled(ADC_IRQ_FIFO, true);

    // Creo tareas
    xTaskCreate(tarea_dispara_adc, "DisparoADC", 256, NULL, 1, NULL);
    xTaskCreate(tarea_lee_adc, "LecturaADC", 256, NULL, 1, NULL);

    // Inicio el scheduler
    vTaskStartScheduler();

    while (1); // Nunca debería llegar acá
}
