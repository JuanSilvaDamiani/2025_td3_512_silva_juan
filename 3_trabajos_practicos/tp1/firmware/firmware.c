// TP1v2-correción
// Mensaje de la catedra: "Fijate que si esta es la solución que planteás
// para la versión dos, no se van a respetar los tiempos que se piden,
// se te van a empezar a defasar mientras mas pase el tiempo. Corregilo"
//La objeción se basa en que, siendo una tarea sucesoria de la otra, 
//es decir si el led se prende 1000 ms con on y se apaga 1500 ms con off, 
//y así continuamente, pero después de los vTaskDelay(),la ejecución del código
// con sus distintas llamadas, introducen algo de tiempo 
//(por más mínimo que sea), ese tiempo se acumula.
//Y a lo largo de muchas iteraciones se iría perdiendo la sincronización.
//Según vimos en clase una solución sería encender led por el total del 
//tiempo de 2500 seg y con una demora de 1000 seg apagar el led por también
//2500 seg, de esa forma se mantendría la sincronización porque las tareas
// se sincronizan con el ciclo total (2500 ms), en vez de simplemente
// esperar después de la acción de cada tarea evitando la desincronización.
//Tiempo (ms) →      0     500   1000   1500   2000   2500   3000   3500   4000   5000
//                  ───────────────────────────────────────────────────────────────────
//encender_led_task  ↑───── ENCIENDE LED ─────────────↑───── ENCIENDE LED ────────↑
//LED                ██ ENCENDIDO ██───── apagado ────██  ENCENDIDO ██───── apagado ───
//apagar_led_task                ↑────────── APAGA LED ─────────────↑─────── APAGA LED ─
//                      (desplazado 1000 ms)           (2500 ms después)
//
//Está encendido entre los milisegundos 0–1000, 2500–3500, etc.
//Está apagado entre los milisegundos 1000–2500, 3500–5000, etc


#include <stdio.h>
#include "pico/stdlib.h"
#include "FreeRTOS.h"
#include "task.h"

#define LED_PIN 25
#define on 1000   // Tiempo que el LED está encendido (ms)
#define off 1500  // Tiempo que el LED está apagado (ms)

void encender_led_task(void *pvParameters) { // Se crea la función encender_led_task.
    while (1) {
        gpio_put(LED_PIN, 1);  // Enciende el LED - gpio_put(25, 1); → pone en ALTO (enciende el LED).
        vTaskDelay(pdMS_TO_TICKS(on + off));  
        // Pone en espera la tarea actual durante una cantidad de ticks del sistema,
        // liberando el procesador para la ejecución de otras tareas.
        // Según la frecuencia del reloj de FreeRTOS (configTICK_RATE_HZ, normalmente 1000 Hz = 1 tick = 1 ms),
        // entonces 2500 ticks son los 2500ms pedidos. Termina el tiempo y la tarea se desbloquea.
    }
}

void apagar_led_task(void *pvParameters) { // Se crea la función apagar_led_task.
    vTaskDelay(pdMS_TO_TICKS(on));//espera 1000ms antes de empezar la tarea apagar el Led
    while (1) {
        gpio_put(LED_PIN, 0);  // Apaga el LED - gpio_put(25, 0); → pone en BAJO (apaga el LED).
        vTaskDelay(pdMS_TO_TICKS(on + off));  
        // Pone en espera la tarea actual durante una cantidad de ticks del sistema,
        // liberando el procesador para la ejecución de otras tareas.
        // Según la frecuencia del reloj de FreeRTOS (configTICK_RATE_HZ, normalmente 1000 Hz = 1 tick = 1 ms),
        // entonces 500 ticks son los 500ms pedidos. Termina el tiempo y la tarea se desbloquea.
    }
}

int main() {
    stdio_init_all();                             // Inicializa la entrada/salida estándar (UART)
    gpio_init(LED_PIN);                           // Inicializa el pin 25 para usarlo como GPIO.
    gpio_set_dir(LED_PIN, GPIO_OUT);              // Pone el pin 25 en modo salida.

    // Crea la tarea "EncenderLED" que ejecutará la función encender_led_task.
    // Esta tarea tendrá 256 bytes de pila, lo cual es suficiente para tareas simples.
    // No se pasan parámetros a la tarea (primer NULL).
    // La prioridad de la tarea es 1, es decir, una prioridad baja (0 sería la mayor prioridad).
    // No se guarda un manejador para la tarea, por eso el segundo NULL.
    xTaskCreate(encender_led_task, "EncenderLED", 256, NULL, 1, NULL);

    // Crea la tarea "ApagarLED" que ejecutará la función apagar_led_task.
    // Similar a la tarea de encender el LED, con 256 bytes de pila, sin parámetros, 
    // con prioridad 1 y sin un manejador de tarea.
    xTaskCreate(apagar_led_task, "ApagarLED", 256, NULL, 1, NULL);

    vTaskStartScheduler();  // Inicia el planificador de tareas FreeRTOS que permite
                            // la ejecución concurrente de tareas según sus prioridades y otros factores.
                            // El flujo del programa pasa de la función main() al planificador de FreeRTOS
                            // después de llamar a esta función. Sin esta función, las tareas creadas
                            // con xTaskCreate() no se ejecutan.

    while (1);  // Nunca debería llegar acá
}
