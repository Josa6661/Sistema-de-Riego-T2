# Sistema de Riego Automático con ESP32-S3

Proyecto para el curso **Algoritmos y Estructuras de Datos I**  
Instituto Tecnológico de Costa Rica  
Autores: Josafat Solano, Mathias Calvo, Camila Navarro  
Fecha: Septiembre 2025

---

## Descripción

Este sistema controla el riego de **tres zonas independientes** utilizando sensores de humedad, bombas y consulta meteorológica en línea. El usuario puede operar el sistema en modo automático, manual o demo, configurando horarios y frecuencia de riego por zona. El sistema está desarrollado en Arduino utilizando programación orientada a objetos.

---

## Características principales

- **Riego automático** según humedad y horario configurado.
- **Consulta de clima en línea** (WeatherAPI) para suspender el riego si hay alta probabilidad de lluvia.
- **Modos de operación:**
  - **Automático**: Opera según sensores, clima y calendario.
  - **Manual**: Permite activar bombas individualmente desde el monitor serie.
  - **Demo**: Ignora el pronóstico de lluvia, útil para pruebas.
- **Configuración flexible** de horario, duración y frecuencia de riego para cada zona.
- **POO (Programación Orientada a Objetos)**: Implementación modular con la clase `Zona`.

---

## Pines usados

- **Sensores de humedad:**  
  - Zona 1: GPIO 4  
  - Zona 2: GPIO 5  
  - Zona 3: GPIO 6
- **Bombas:**  
  - Zona 1: GPIO 42  
  - Zona 2: GPIO 41  
  - Zona 3: GPIO 40

---

## Configuración por defecto (puedes modificar en el código)

- **Umbral de humedad:** 2500 (mayor = más seco)
- **Zona 1:** riego a las 07:00, duración 15 min, cada 1 día
- **Zona 2:** riego a las 21:00, duración 10 min, cada 3 días
- **Zona 3:** sin riego programado (valores en cero)

---

## ¿Cómo usar el sistema?

1. **Configura tu WiFi y claves en el código:**  
   Modifica los valores de `WIFI_SSID`, `WIFI_PASS`, `CLIMA_API_KEY`, `LATITUD` y `LONGITUD` según tu red y ubicación.
2. **Conecta los sensores y bombas** a los pines indicados.
3. **Carga el código** a tu ESP32-S3 desde Arduino IDE.
4. **Abre el monitor serie** (baud rate: 115200) para ver el estado y controlar el sistema.

---

## Comandos por Serial

- `D` — Activa/desactiva **Modo Demo** (ignora pronóstico de lluvia)
- `M` — Activa/desactiva **Modo Manual** (control manual de bombas)
- `1`, `2`, `3` — En modo manual, activa/desactiva la bomba de cada zona
- `T HH:MM:SS` — Configura la hora manualmente (ejemplo: `T 06:30:00`)

---

## Estructura del código

- **Clase `Zona`:**  
  Cada zona es un objeto que maneja su sensor, bomba, horario, duración y frecuencia.  
  Métodos principales:
  - `iniciar()`: Inicializa pines
  - `leerSensor()`: Lee humedad
  - `establecerBomba()`: Controla la bomba
  - `estadoBomba()`: Consulta el estado de la bomba

- **Lógica de riego:**  
  - Si la humedad está por debajo del umbral, se activa la bomba.
  - Si es la hora programada y corresponde la frecuencia, riega por la duración indicada.
  - Si el pronóstico de lluvia supera el 70%, el sistema suspende el riego (excepto en modo demo o manual).

- **Integración con WeatherAPI:**  
  Consulta diaria para obtener la probabilidad de lluvia y optimizar el uso de agua.

---

## Ejemplo de salida por Serial

```
------ Estado del sistema ------
Hora actual: 07:00:00 | Modo: NTP | Demo: NO | Manual: NO
🌦 Probabilidad de lluvia: 80% | Suspender riego: SI
Zona 1 -> Humedad: 3200 | Necesita riego: SI | Bomba: APAGADA
Zona 2 -> Humedad: 1800 | Necesita riego: NO | Bomba: APAGADA
Zona 3 -> Humedad: 2700 | Necesita riego: SI | Bomba: APAGADA
-------------------------------
```

---

## Recomendaciones y mejoras posibles

- Añadir una interfaz web o app móvil para control remoto.
- Guardar logs de riego y humedad.
- Agregar soporte para más zonas.
- Mejorar la seguridad de las credenciales (no dejar claves en texto plano).

---

## Créditos

Desarrollado por:
- Josafat Solano
- Mathias Calvo
- Camila Navarro

Instituto Tecnológico de Costa Rica  
Curso: Algoritmos y Estructuras de Datos I  
Septiembre 2025
