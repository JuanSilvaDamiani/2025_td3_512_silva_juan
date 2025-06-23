#include <stdio.h>
#include "pico/stdlib.h"
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "helper.h"  // <- Acá se incluye módulo con pwm_user_init()

#define GPIO_PWM_OUT   16  // PWM generado
#define GPIO_INPUT     15  // Señal leída
#define TEST_FREQ_HZ   1000  // Cambiar valor para probar distintas frecuencias 1000(1kHz),5000(5kHz),10000(10kHz) etc

SemaphoreHandle_t sem_frecuencia; //semáforo counting para contar los flancos ascendentes detectados
volatile bool last_state = false; //creo la variable global last_state para
//guardar el último estado lógico del pin de entrada 15 (false = bajo) y se inicia en False
//porque no hay señal para detectar si hubo un flanco

// --- Tarea que genera PWM en GPIO 16 usando helper.c
void task_generar_pwm(void *pvParameters) {
    pwm_user_init(GPIO_PWM_OUT, TEST_FREQ_HZ);  // Usamos la función de helper.c

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));  // Pausar la tarea actual de FreeRTOS durante N ticks 
        //dejándola "bloqueada" y así el scheduler permite que otras tareas corran.
        //Cuando pasa el tiempo, la tarea vuelve al estado "lista o Ready" para ejecutarse.
        //pdMS_TO_TICKS(1000) convierte milisegundos a ticks del sistema.
        //Si configTICK_RATE_HZ está configurado a 1000 (lo típico en la Pico),
        //entonces pdMS_TO_TICKS(1000) = 1000 ticks = 1 segundo.
    }
}

// --- Tarea que detecta flancos ascendentes por polling y da el semáforo
void task_poll_gpio(void *pvParameters) //función de tarea de FreeRTOS (no se usan parámetros)
{
    while (1) {
        bool current = gpio_get(GPIO_INPUT);  // Lee el estado actual del pin de entrada GPIO 15.
        // gpio_get(...) devuelve true si está en alto (1), false si está en bajo (0).

        if (!last_state && current) {
            // Si el último estado fue 0 (bajo) y el actual es 1 (alto), hay un flanco ascendente.
            xSemaphoreGive(sem_frecuencia); // "Da" el semáforo counting: cuenta un evento.
        }
        last_state = current;  // Guarda el estado actual como "último estado" para comparar luego.
        vTaskDelay(pdMS_TO_TICKS(1));  // Espera 1 ms antes de muestrear de nuevo.
    }
}

// --- Tarea que cada 1 segundo mide la frecuencia (cuenta los semáforos) de la señal que llega al GPIO 15.
// Para eso, cuenta cuántos flancos ascendentes ocurren en 1 segundo

void task_mostrar_frecuencia(void *pvParameters) {
    while (1) {
        uint32_t contador = 0;// Se crea la variable contador para contar los flancos ascendentes,
        // es decir, un ciclo de señal detectado por cada flanco ascendente.

        // Toma semáforos durante 1 segundo (1000 iteraciones × 1 ms = 1000 ms = 1 segundo total de medición)
        for (int i = 0; i < 1000; i++) {
            if (xSemaphoreTake(sem_frecuencia, pdMS_TO_TICKS(1)) == pdTRUE)//xSemaphoreTake(...), es una tarea
            // que intenta "tomar" el semáforo tipo counting sem_frecuencia si hay uno disponible (pdTRUE)
            // que significa que ocurrió un flanco, y entonces se incrementa contador;
            //si no hay semáforo disponible en ese momento, espera 1 ms y vuelve a intentar.
             {
                contador++;
            }
        }

        printf("Frecuencia medida: %lu Hz\n", contador);//Al terminar el for de 1 segundo, imprime cuántos eventos
        // (flancos ascendentes) ocurrieron y eso equivale a la frecuencia en Hertz.
        // Si recibe 1000 flancos entonces la frecuencia medida=1000 Hz, si recibe 5000 flancos frec=5000Hz.
    }
}

int main() {
    stdio_init_all();

    // GPIO de entrada
    gpio_init(GPIO_INPUT);
    gpio_set_dir(GPIO_INPUT, GPIO_IN);
    gpio_pull_down(GPIO_INPUT);  // Evita lecturas erráticas al inicio (ruido)

    // Crear semáforo counting con capacidad para 10.000 eventos por segundo
    // 10.000 es el valor máximo del semáforo para cubrir los 10 kHz del enuniciado,
    // 0 es el valor inicial del contador
    sem_frecuencia = xSemaphoreCreateCounting(10000, 0);//devuelve un puntero.
    // Si la creación del semáforo fue exitosa, devuelve un puntero válido (!= NULL).
    //Si falló, devuelve NULL.
    if (sem_frecuencia == NULL) //	Verifica si la creación del semáforo counting falló
    {
        printf("Error al crear el semáforo\n"); //muestra un mensaje de error por consola:
        while (1);//entra en ciclo ininito que detiene el programa inmediatamente, ya que sin ese semáforo
        // el sistema no podría contar los flancos, y seguir ejecutandose no tendría sentido.
    }

    // Crear tareas
    xTaskCreate(task_generar_pwm,        "PWMGen",   256, NULL, 1, NULL);//Crea la tarea que genera la señal PWM
    //256 Tamaño del stack asignado a la tarea (en words, no bytes)
    // NULL: No se le pasan parámetros.
    //1 Prioridad de ejecución (más alto = más prioridad)
    //NULL: No se guarda el handle (identificador) de la tarea
    xTaskCreate(task_poll_gpio,          "Poll",     256, NULL, 2, NULL);// "Poll" nombre de la Tarea
    //Tarea que detecta flancos en GPIO 15 por polling
    //Tiene prioridad 2, más alta que la tarea de PWM (1), para que reaccione más rápido al muestreo de flancos
    xTaskCreate(task_mostrar_frecuencia, "Display",  256, NULL, 1, NULL);//Tarea que mide cuántos flancos hubo en 1 segundo.

    // Iniciar FreeRTOS
    vTaskStartScheduler();

    while (1);  // Nunca se debe llegar aquí
}