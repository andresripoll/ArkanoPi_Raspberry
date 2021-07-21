
#include "arkanoPi.h"

int flags = 0;

TipoSistema sistema;

// Declaracion del objeto teclado
TipoTeclado teclado = {
	.columnas = {
			GPIO_KEYBOARD_COL_1,
			GPIO_KEYBOARD_COL_2,
			GPIO_KEYBOARD_COL_3,
			GPIO_KEYBOARD_COL_4,
	},
	.filas = {
			GPIO_KEYBOARD_ROW_1,
			GPIO_KEYBOARD_ROW_2,
			GPIO_KEYBOARD_ROW_3,
			GPIO_KEYBOARD_ROW_4,
	},
	.rutinas_ISR = {
			teclado_fila_1_isr,
			teclado_fila_2_isr,
			teclado_fila_3_isr,
			teclado_fila_4_isr,
	},
	//Inicialización del resto de variables:
	.flags = 0,
	.debounceTime = {0,0,0,0},
	.teclaPulsada = {-1,-1},
	.columna_actual = COLUMNA_1,
};

// Declaracion del objeto display
TipoLedDisplay led_display = {
	.pines_control_columnas = {
			GPIO_LED_DISPLAY_COL_1,
			GPIO_LED_DISPLAY_COL_2,
			GPIO_LED_DISPLAY_COL_3,
	},
	.filas = {
			GPIO_LED_DISPLAY_ROW_1,
			GPIO_LED_DISPLAY_ROW_2,
			GPIO_LED_DISPLAY_ROW_3,
			GPIO_LED_DISPLAY_ROW_4,
			GPIO_LED_DISPLAY_ROW_5,
			GPIO_LED_DISPLAY_ROW_6,
			GPIO_LED_DISPLAY_ROW_7,
	},
	//Inicialización del resto de variables:
	.flags = 0,
	.p_columna = 0,
};

//------------------------------------------------------
// FUNCIONES DE CONFIGURACION/INICIALIZACION
//------------------------------------------------------

// int ConfiguracionSistema (TipoSistema *p_sistema): procedimiento de configuracion
// e inicializacion del sistema.
// Realizará, entra otras, todas las operaciones necesarias para:
// configurar el uso de posibles librerías (e.g. Wiring Pi),
// configurar las interrupciones externas asociadas a los pines GPIO,
// configurar las interrupciones periódicas y sus correspondientes temporizadores,
// la inicializacion de los diferentes elementos de los que consta nuestro sistema,
// crear, si fuese necesario, los threads adicionales que pueda requerir el sistema
// como el thread de exploración del teclado del PC
int ConfiguraInicializaSistema (TipoSistema *p_sistema) {

	// Inicializamos wiringPi
	piLock(STD_IO_BUFFER_KEY);
	wiringPiSetupGpio();
	piUnlock(STD_IO_BUFFER_KEY);

	if(wiringPiSetupGpio() < 0){
		printf("Unable to setup wiringPi\n");

		return -1;
	}

	// Iniciamos el display de leds y el teclado
	InicializaLedDisplay(&led_display);
	InicializaTeclado(&teclado);

	// Arrancamos el timer de actualización de juego
	sistema.arkanoPi.tmr = tmr_new (tmr_actualizacion_juego_isr);

	return 1;
}

// wait until next_activation (absolute time)
void delay_until (unsigned int next) {
	unsigned int now = millis();
	if (next > now) {
		delay (next - now);
	}
}

int main () {
	unsigned int next;

	// Maquina de estados: lista de transiciones
	// {EstadoOrigen, CondicionDeDisparo, EstadoFinal, AccionesSiTransicion }
	fsm_trans_t arkanoPi[] = {
		{ WAIT_START, CompruebaBotonPulsado, WAIT_PUSH, InicializaJuego },
		{ WAIT_PUSH, CompruebaTimeoutActualizacionJuego, WAIT_PUSH, ActualizarJuego },
		{ WAIT_PUSH, CompruebaMovimientoIzquierda, WAIT_PUSH, MuevePalaIzquierda },
		{ WAIT_PUSH, CompruebaMovimientoDerecha, WAIT_PUSH, MuevePalaDerecha },
		// Añadidas las transiciones respectivas a la función de pausa
		{ WAIT_PUSH, CompruebaPausaJuego, WAIT_PAUSE, PausaJuego },
		{ WAIT_PAUSE, CompruebaPausaJuego, WAIT_PUSH, PausaJuego },
		{ WAIT_PUSH, CompruebaFinalJuego, WAIT_END, FinalJuego },
		{ WAIT_END,  CompruebaBotonPulsado, WAIT_START, ReseteaJuego },
		{-1, NULL, -1, NULL },
	};

	// Configuracion e incializacion del sistema
	ConfiguraInicializaSistema (&sistema);

	sistema.arkanoPi.p_pantalla = &(led_display.pantalla);

	fsm_t *arkanoPi_fsm = fsm_new (WAIT_START, arkanoPi, &sistema);
	fsm_t *excitacion_columnas_fsm = fsm_new (TECLADO_ESPERA_COLUMNA, fsm_trans_excitacion_columnas, &(teclado));
	fsm_t *pulsaciones_fms = fsm_new (TECLADO_ESPERA_TECLA, fsm_trans_deteccion_pulsaciones, &(teclado));
	fsm_t *excitacion_display_fsm = fsm_new (DISPLAY_ESPERA_COLUMNA, fsm_trans_excitacion_display, &(led_display));

	// Después de la configuración llamamos a resetea para empezar un nuevo juego
	ReseteaJuego(arkanoPi_fsm);

	next = millis();
	while (1) {
		fsm_fire (arkanoPi_fsm);

		fsm_fire (excitacion_columnas_fsm);
		fsm_fire (pulsaciones_fms);

		fsm_fire (excitacion_display_fsm);

		next += CLK_MS;
		delay_until (next);
	}

	tmr_destroy ((tmr_t*)(arkanoPi_fsm->user_data));
	fsm_destroy (arkanoPi_fsm);
	fsm_destroy (excitacion_columnas_fsm);
	fsm_destroy (pulsaciones_fms);
	fsm_destroy (excitacion_display_fsm);

}
