# ESP32 Marauder - Edición Especial ESP32-C5 (Headless)

Este repositorio contiene una versión optimizada y estabilizada de **ESP32Marauder** específicamente adaptada para el nuevo chip **ESP32-C5 (Arquitectura RISC-V)**. Esta versión está configurada para funcionar en modo "Headless" (sin pantalla), enfocándose en la interacción mediante la interfaz de comandos (CLI) y sensores externos como NFC.

## 🚀 ¿Qué es ESP32 Marauder?
Es una suite de herramientas de código abierto diseñada para la auditoría de seguridad en redes inalámbricas (WiFi y Bluetooth). Permite realizar pruebas de penetración y análisis de tráfico de forma portátil.

## 🛠 Funcionalidades Principales

### 1. Auditoría WiFi (2.4GHz)
*   **Escaneo de APs**: Encuentra todos los puntos de acceso cercanos.
*   **Escaneo de Estaciones**: Identifica qué dispositivos (móviles, laptops) están conectados a qué redes.
*   **Sniffing**: Captura paquetes Raw y tráfico WiFi para su posterior análisis.
*   **Pruebas de Desautenticación**: Envía paquetes para desconectar dispositivos de una red (útil para capturar "handshakes" WPA).
*   **Beacon Spam**: Crea cientos de redes WiFi falsas con nombres personalizados.

### 2. Evil Portal
*   Crea un punto de acceso falso que, al conectarse, redirige al usuario a una página web personalizada (Portal Cautivo) para investigar vulnerabilidades de phishing o recolección de credenciales.

### 3. Herramientas NFC
*   Integración con el sensor **NT3H2111** para lectura y manipulación de etiquetas NFC mediante bus I2C (Pines 2 y 3).

## ⚙️ Configuración Específica para C5
Debido a las particularidades del chip ESP32-C5 ECO1, se han aplicado los siguientes ajustes críticos:
*   **Arquitectura**: Compilación optimizada para RISC-V.
*   **Flash**: Configurada a **40MHz** para máxima estabilidad.
*   **Memoria**: PSRAM desactivada para evitar errores de asignación de cache.
*   **Modo Headless**: Pantalla OLED desactivada para reducir el consumo y evitar fallos de inicialización.

## ⌨️ Guía de Comandos Rápidos
Escribe estos comandos en el monitor serie (115200 baudios):

| Comando | Acción |
| :--- | :--- |
| `help` | Muestra la lista completa de comandos disponibles. |
| `scanap` | Busca redes WiFi (Puntos de Acceso). |
| `list -ap` | Muestra la lista de redes encontradas tras el escaneo. |
| `selectap <id>` | Selecciona una red específica para trabajar. |
| `attack -t deauth` | Inicia un ataque de desautenticación a la red seleccionada. |
| `stopscan` | Detiene cualquier proceso de escaneo en curso. |
| `clearall` | Limpia la memoria de redes y estaciones encontradas. |

## ⚠️ Advertencia de Seguridad
Esta herramienta está diseñada exclusivamente para **fines educativos y auditorías de seguridad ética**. El uso de esta herramienta en redes ajenas sin autorización es ilegal y bajo tu propia responsabilidad.

---
**Desarrollado para la comunidad de ciberseguridad.**
