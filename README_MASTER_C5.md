# 🛡️ ESP32 Marauder Master Guide - C5 Edition

Este repositorio contiene la versión estable y optimizada del firmware **ESP32 Marauder** adaptado para el **ESP32-C5 (Headless)**, junto con su ecosistema de control visual **Marauder UI Pro**.

---

## 🚀 Capacidades del Firmware (ESP32-C5)

El firmware ha sido ajustado para ofrecer máxima estabilidad en la arquitectura RISC-V del C5, incluyendo optimizaciones de radio y privacidad.

### 📡 Auditoría WiFi
*   **Packet Sniffing:** Captura de Beacons, Deauth, PMKID y marcos de datos raw.
*   **Scanning:** Detección en tiempo real de puntos de acceso (AP) y estaciones (clientes).
*   **Ataques de Desautenticación:** Flood de deauth dirigido o general.
*   **Beacon Spam:** Generación de redes falsas (SSID aleatorios o lista personalizada).
*   **PMKID Capture:** Captura de hashes para auditoría de WPA2 sin necesidad de clientes conectados.

### 🛠️ Ecosistema Evil Portal (Privacy Optimized)
*   **SHA-256 Hashing:** Todas las credenciales capturadas se hashean instantáneamente mediante hardware. Cumple con estándares de privacidad (no se guarda texto plano).
*   **Auto-Start:** Iniciación rápida mediante `evilportal -w <archivo.html>`.
*   **Captura Headless:** Diseñado para funcionar de forma autónoma con guardado en SD.

### 󱠔 Integración NFC (Exclusivo C5 Stabilized)
*   **Hardware:** Soporte para chip **NT3H2111 (NTAG I2C Plus)** en pines IO2/IO3.
*   **nfc scan:** Detección y diagnóstico del hardware NFC.
*   **nfc read:** Volcado dinámico de memoria (Hex/ASCII) para auditoría de etiquetas.
*   **nfc write:** Escritura de registros NDEF (URL, Plain Text, vCard/Contactos).
*   **Estabilidad:** Bus I2C optimizado a 100kHz para evitar ruidos en soldaduras manuales.

---

## 🖥️ Marauder UI Pro (Interfaz Web)

La interfaz profesional permite controlar el dispositivo de forma visual a través de la **Web Serial API**.

### 🎨 Características Visuales
*   **Estética Neobrutalista:** Modo oscuro profundo con acentos neón y efectos de cristal (Glassmorphism).
*   **Terminal Integrada:** Monitor serial en tiempo real con resaltado de sintaxis para comandos.
*   **Tablas Dinámicas:** Visualización de redes WiFi y dispositivos Bluetooth con indicadores de potencia (RSSI).

### ⚡ Panel NFC Pro (Nuevo)
*   **Dashboard Dedicado:** Interfaz gráfica para todas las funciones de la antena.
*   **Memory Viewer:** Visualización interactiva del contenido del tag.
*   **NDEF Builder:** Creador visual de contactos y enlaces sin comandos manuales.

### 🔄 Workflows Avanzados (Automatizaciones)
*   **Social Engineering Redirect:** Escribe una URL en el tag y activa el Evil Portal automáticamente.
*   **Stealth Contact Drop:** Crea un contacto de soporte técnico falso pero creíble en segundos.
*   **Audit & Wipe:** Analiza una etiqueta extraña y límpiala de inmediato.
*   **NFC Triggered Deauth:** Configura una advertencia en el tag y lanza un ataque WiFi global.

---

## 🛠️ Instalación y Uso

### 1. Firmware
Compila y flashea la carpeta `esp32_marauder` usando el IDE de Arduino habilitando la opción `HAS_NFC` en `configs.h`.

### 2. Interfaz UI
1. Navega a `Marauder ui pro/marauder-ui-pro`.
2. Ejecuta `npm install` y luego `npm run dev`.
3. Abre `http://localhost:3000/marauder-ui-pro/` y pulsa **Connect**.

---

> **⚠️ Nota Legal:** Esta herramienta es exclusiva para fines educativos y auditorías de seguridad autorizadas. El uso de estas técnicas en redes ajenas sin permiso es ilegal.
