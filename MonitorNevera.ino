
#include <ThingSpeak.h>
#include <ESP8266HTTPClient.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BMP085_U.h>
#include <Wire.h>
#include <ESP8266WiFi.h>
#include "WiFiConfig.h"

#ifndef WIFICONFIG
char *wifiSSID = "SSID";
char *wifiPass = "PASSWORD";
char *iftttToken = "IFTTT_TOKEN";
#endif // !WIFICONFIG

const char *iftttURL1 = "https://maker.ifttt.com/trigger/";
const char *iftttURL2 = "/with/key/";

const char *iftttFingerprint = "c0 5d 08 5e e1 3e e0 66 f3 79 27 1a ca 1f fc 09 24 11 61 62"; // Fingerprint para comprobar la dirección https de IFTTT



#define SCA_PIN 4
#define SCL_PIN 5

#define BOTON 0

Adafruit_BMP085_Unified bmp180 = Adafruit_BMP085_Unified(10085); // Sensor BMP180 (presión y temperatura)
HTTPClient clienteHTTP; // Cliente para conectar a IFTTT
WiFiClient clienteWiFi; // Cliente para conectar a Thingspeak

void enviar_dato_ifttt(float temperatura, float presion, const char *iftttEvent) {
	String url;
	int resultadoHTTP;

	url = String(iftttURL1) + iftttEvent + String(iftttURL2) + iftttToken + "?value1=" + String(temperatura) + "&value2=" + String(presion);
		
	Serial.println(url);

	if (!clienteHTTP.begin(url, iftttFingerprint)) {
		Serial.println("Error de comprobación del servidor HTTPS");
	}
	resultadoHTTP = clienteHTTP.GET();
	if (resultadoHTTP > 0) {
		// HTTP header has been send and Server response header has been handled
		Serial.printf("[HTTP] GET... code: %d\n", resultadoHTTP);

		// file found at server
		if (resultadoHTTP == HTTP_CODE_OK) {
			String payload = clienteHTTP.getString();
			Serial.println(payload);
			Serial.println();
		}
	}
	else {
		Serial.printf("[HTTP] GET... failed, error: %s\n\n", clienteHTTP.errorToString(resultadoHTTP).c_str());
	}

	clienteHTTP.end();
}

void enviar_dato_thingspeak(float temperatura, float presion) {
	const long channelID = 190646;
	const char * ApiKey = "NPOQUJAEX5W4GHII";

	ThingSpeak.begin(clienteWiFi);

	ThingSpeak.setField(1, temperatura);
	ThingSpeak.setField(2, presion);

	int estado = ThingSpeak.writeFields(channelID,ApiKey);
	Serial.printf("Enviado dato a ThingSpeak. Resultado: %d\n", estado);
}

void setup()
{
	// Iniciar puerto serie
	Serial.begin(115200);
	Serial.println();

	// Conectar WiFi
	WiFi.mode(WIFI_STA);
	WiFi.begin(wifiSSID, wifiPass);
	while (!WiFi.isConnected()) { // Esperar a que esté conectado
		delay(100);
		Serial.print(".");
	}
	Serial.println();
	Serial.printf("IP Address: %s\n\n", WiFi.localIP().toString().c_str());
	
	// Iniciar sensor BMP180
	sensor_t sensor;
	bmp180.getSensor(&sensor);
	if (!bmp180.begin(SCA_PIN, SCL_PIN)) { 
		Serial.println("No se ha encontrado el sensor BMP180, comprueba el cableado");
		while (1) { delay(0); }
	}
	
	// Iniciar entrada botón
	pinMode(BOTON, INPUT_PULLUP);
	
}

void loop() {
	sensors_event_t evento;
	float temperatura;
	float presion;

	bmp180.getEvent(&evento); // Iniciar medida
	if (!digitalRead(0)) { // Si se ha pulsado el botón
		if (evento.pressure) { // Si se ha terminado la medida
			//bmp180.getPressure(&presion); // Obtener presión
			//presion = ((float)presion)/100.0; // La presión la da en pascales. Hay que calcular hPa
			presion = evento.pressure;
			bmp180.getTemperature(&temperatura); // Obtener temperatura
			Serial.printf("Temperatura: %f\n", temperatura);
			Serial.printf("Presion: %f\n\n", presion);
		}
		else {
			Serial.println("Error del sensor");
		}

		//enviar_dato_ifttt(temperatura, presion, "mail");
		enviar_dato_ifttt(temperatura, presion, "twitt"); // Enviar un Twitt
		enviar_dato_thingspeak(temperatura, presion); // Enviar dato para dibujar gráfica en ThingSpeak

		delay(1000); // Retardo para evitar procesar pulsaciones largas como varias pulsaciones
	}
	delay(50);

}
