#include <Bluepad32.h>
#include <TB6612_ESP32.h>

ControllerPtr myControllers[BP32_MAX_GAMEPADS];  // Array para controlar hasta 4 gamepads

Motor motor1(12, 13, 14, 10, 15, 2000, 8, 0);  // Motor 1: pin de control
Motor motor2(16, 17, 18, 11, 19, 2000, 8, 1);  // Motor 2: pin de control

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
        Serial.println("CALLBACK: Controller connected, but could not find empty slot");
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
        Serial.println("CALLBACK: Controller disconnected, but not found in myControllers");
    }
}

void processGamepad(ControllerPtr ctl) {
    int speedL = ctl->axisY();   // Eje L: adelante/atrás
    int speedR = ctl->axisRX();  // Eje R: izquierda/derecha

    const int deadZone = 20;

    // Eje L - Movimiento hacia adelante o atrás
    if (abs(speedL) > deadZone) {
        if (speedL > 0) {
            back(motor1, motor2, -speedL);
            Serial.println("Moviendo hacia atrás");
        } else {
            forward(motor1, motor2, speedL);
            Serial.println("Moviendo hacia adelante");
        }
    } else {
        // Si está en zona muerta, frenamos
        brake(motor1, motor2);
        Serial.println("En zona muerta (adelante/atrás) - frenando");
    }

    // Eje R - Giro a izquierda o derecha
    if (abs(speedR) > deadZone) {
        if (speedR > 0) {
            right(motor1, motor2, speedR);
            Serial.println("Girando hacia la derecha");
        } else {
            left(motor1, motor2, -speedR);
            Serial.println("Girando hacia la izquierda");
        }
    } else {
        Serial.println("En zona muerta (izquierda/derecha) - sin giro");
    }
}

void setup() {
    Serial.begin(115200);
    Serial.printf("Firmware: %s\n", BP32.firmwareVersion());
    const uint8_t* addr = BP32.localBdAddress();
    Serial.printf("BD Addr: %2X:%2X:%2X:%2X:%2X:%2X\n", addr[0], addr[1], addr[2], addr[3], addr[4], addr[5]);

    // Configura las funciones de conexión de controladores
    BP32.setup(&onConnectedController, &onDisconnectedController);
    BP32.forgetBluetoothKeys();  // Opcional: para evitar la reconexión automática de dispositivos previamente emparejados
    BP32.enableVirtualDevice(false);  // Deshabilitar el dispositivo virtual para evitar problemas con controladores de ratón/touchpad
}

void loop() {
    bool dataUpdated = BP32.update();  // Actualiza los datos de los controladores
    if (dataUpdated) {
        // Procesa los controladores conectados y su entrada
        for (auto myController : myControllers) {
            if (myController && myController->isConnected() && myController->hasData()) {
                processGamepad(myController);  // Procesa los datos del gamepad
            }
        }
    }
    delay(150);  // Para evitar el desbordamiento del watchdog timer
}
