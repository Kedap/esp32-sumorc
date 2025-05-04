#include <Arduino.h>
#include <Bluepad32.h>
#include <TB6612_ESP32.h>

// Definición de pines para el driver TB6612FNG
#define AIN1 13 // ESP32 Pin D13 to TB6612FNG Pin AIN1
#define BIN1 12 // ESP32 Pin D12 to TB6612FNG Pin BIN1
#define AIN2 14 // ESP32 Pin D14 to TB6612FNG Pin AIN2
#define BIN2 27 // ESP32 Pin D27 to TB6612FNG Pin BIN2
#define PWMA 26 // ESP32 Pin D26 to TB6612FNG Pin PWMA
#define PWMB 25 // ESP32 Pin D25 to TB6612FNG Pin PWMB
#define STBY 33 // ESP32 Pin D33 to TB6612FNG Pin STBY

ControllerPtr
    myControllers[BP32_MAX_GAMEPADS]; // Array para controlar hasta 4 gamepads

// Inicialización de los motores usando los nuevos pines definidos
// Constructor: Motor(int In1pin, int In2pin, int PWMpin, int offset, int
// STBYpin, int freq, int resolution, int channel_pin);
Motor motor1(AIN1, AIN2, PWMA, 10, STBY, 2000, 8,
             0); // Motor 1 (Motor A): usa pines AIN1, AIN2, PWMA, y el pin STBY
                 // compartido
Motor motor2(BIN1, BIN2, PWMB, 11, STBY, 2000, 8,
             1); // Motor 2 (Motor B): usa pines BIN1, BIN2, PWMB, y el pin STBY
                 // compartido

void onConnectedController(ControllerPtr ctl) {
  bool foundEmptySlot = false;
  for (int i = 0; i < BP32_MAX_GAMEPADS; i++) {
    if (myControllers[i] == nullptr) {
      Serial.printf("CALLBACK: Controller is connected, index=%d\n", i);
      myControllers[i] = ctl;
      foundEmptySlot = true;
      break;
    }
  }
  if (!foundEmptySlot) {
    Serial.println(
        "CALLBACK: Controller connected, but could not find empty slot");
  }
}

void onDisconnectedController(ControllerPtr ctl) {
  bool foundController = false;
  for (int i = 0; i < BP32_MAX_GAMEPADS; i++) {
    if (myControllers[i] == ctl) {
      Serial.printf("CALLBACK: Controller disconnected from index=%d\n", i);
      myControllers[i] = nullptr;
      foundController = true;
      break;
    }
  }
  if (!foundController) {
    Serial.println(
        "CALLBACK: Controller disconnected, but not found in myControllers");
  }
}

void processGamepad(ControllerPtr ctl) {
  int speedL = ctl->axisY(); // Eje L: adelante/atrás (valor entre -512 y 511)
  int speedR =
      ctl->axisRX();       // Eje R: izquierda/derecha (valor entre -512 y 511)
  const int deadZone = 20; // Zona muerta para evitar movimientos accidentales

  // Mapear speedL de [-512, 511] a [-255, 255]
  int motorSpeedL = map(speedL, -512, 511, -255, 255);
  // Mapear speedR de [-512, 511] a [-255, 255]
  int motorSpeedR = map(speedR, -512, 511, -255, 255);

  // --- Lógica de control de movimiento ---

  // Si el joystick de movimiento (izquierdo, eje Y) está fuera de la zona
  // muerta
  if (abs(motorSpeedL) > deadZone) {
    // Movimiento adelante/atrás
    if (motorSpeedL < 0) {
      // Joystick hacia arriba (speedL negativo), vamos a mapear esto a
      // "adelante" con velocidad positiva
      forward(motor1, motor2,
              abs(motorSpeedL)); // Usamos valor absoluto para la velocidad
      Serial.printf("Moviendo hacia adelante a velocidad: %d\n",
                    abs(motorSpeedL));

    } else { // Joystick hacia abajo (speedL positivo)
             // Vamos a mapear esto a "atrás" con velocidad positiva
      back(motor1, motor2,
           abs(motorSpeedL)); // Usamos valor absoluto para la velocidad
      Serial.printf("Moviendo hacia atrás a velocidad: %d\n", abs(motorSpeedL));
    }
  } else {
    // Si el joystick de movimiento está en zona muerta, frenamos
    // Solo frenamos si no estamos girando. Esto permite girar en el sitio.
    if (abs(motorSpeedR) <= deadZone) {
      brake(motor1, motor2);
      Serial.println(
          "En zona muerta (adelante/atrás) y (izquierda/derecha) - frenando");
    } else {
      // Si solo el eje de movimiento está en zona muerta, pero el eje de giro
      // no, no frenamos, permitimos el giro en el sitio.
      Serial.println("Eje adelante/atrás en zona muerta, pero girando.");
    }
  }

  // Si el joystick de giro (derecho, eje X) está fuera de la zona muerta
  if (abs(motorSpeedR) > deadZone) {
    // Giro a izquierda o derecha
    if (motorSpeedR > 0) { // Joystick hacia la derecha
      right(
          motor1, motor2,
          abs(motorSpeedR)); // Usamos valor absoluto para la velocidad de giro
      Serial.printf("Girando hacia la derecha a velocidad: %d\n",
                    abs(motorSpeedR));
    } else { // Joystick hacia la izquierda
      left(motor1, motor2,
           abs(motorSpeedR)); // Usamos valor absoluto para la velocidad de giro
      Serial.printf("Girando hacia la izquierda a velocidad: %d\n",
                    abs(motorSpeedR));
    }
  } else {
    // Si el eje de giro está en zona muerta y también lo estaba el eje de
    // movimiento, ya frenamos arriba. Si el eje de movimiento no estaba en zona
    // muerta, seguimos moviéndonos adelante/atrás sin girar.
    if (abs(motorSpeedL) > deadZone) {
      Serial.println("Eje izquierda/derecha en zona muerta, pero moviéndose "
                     "adelante/atrás.");
    }
  }
  // Si ambos ejes están en zona muerta, ya frenamos en el primer 'else'.
}

void setup() {
  Serial.begin(115200);
  Serial.printf("Firmware: %s\n", BP32.firmwareVersion());
  const uint8_t *addr = BP32.localBdAddress();
  Serial.printf("BD Addr: %2X:%2X:%2X:%2X:%2X:%2X\n", addr[0], addr[1], addr[2],
                addr[3], addr[4], addr[5]);

  // Configura las funciones de conexión de controladores
  BP32.setup(&onConnectedController, &onDisconnectedController);
  BP32.forgetBluetoothKeys(); // Opcional: para evitar la reconexión automática
                              // de dispositivos previamente emparejados
  BP32.enableVirtualDevice(
      false); // Deshabilitar el dispositivo virtual para evitar problemas con
              // controladores de ratón/touchpad

  // Nota: La librería TB6612_ESP32 probablemente inicializa los pines
  // y el STBY en el constructor o en las funciones drive/brake/standby.
  // No necesitas configurar pinMode para los pines del motor aquí.
}

void loop() {
  bool dataUpdated = BP32.update(); // Actualiza los datos de los controladores

  if (dataUpdated) {
    // Procesa los controladores conectados y su entrada
    for (auto myController : myControllers) {
      if (myController && myController->isConnected() &&
          myController->hasData()) {
        processGamepad(myController); // Procesa los datos del gamepad
      }
    }
  }

  // Añadir un pequeño delay para evitar que el loop corra demasiado rápido
  // y consuma excesivos recursos o active el watchdog timer prematuramente.
  // El delay original de 150ms es bastante largo, podrías reducirlo
  // si necesitas una respuesta más rápida del robot. Un valor entre 10-50ms
  // suele ser suficiente.
  delay(20); // Reducido el delay para una respuesta más ágil
}
