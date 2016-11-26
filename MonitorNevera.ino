
#include <OneWire.h>
#include <DallasTemperature.h>
#include <ThingSpeak.h>
#include <ESP8266HTTPClient.h>
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

#define PERIODO 16000

OneWire oneWire(D2);
DallasTemperature ds18b20(&oneWire);
HTTPClient clienteHTTP; // Cliente para conectar a IFTTT
WiFiClient clienteWiFi; // Cliente para conectar a Thingspeak
uint numTermometros;


void enviar_dato_ifttt(float temperatura, const char *iftttEvent) {
	String url;
	int resultadoHTTP;

	url = String(iftttURL1) + iftttEvent + String(iftttURL2) + iftttToken + "?value1=" + String(temperatura); // +"&value2=" + String(presion);
		
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

void enviar_dato_thingspeak(float temp1, float temp2) {
	const long channelID = 190646;
	const char * ApiKey = "NPOQUJAEX5W4GHII";

	ThingSpeak.begin(clienteWiFi);

	ThingSpeak.setField(1, temp1);
	ThingSpeak.setField(2, temp2);

	int estado = ThingSpeak.writeFields(channelID,ApiKey);
	Serial.printf("Enviado dato a ThingSpeak. Resultado: %d\n\n", estado);
}

void imprimirDireccion(DeviceAddress direccion) {
	for (int i=0; i<8; i++) {
		if (direccion[i] < 16)
			Serial.print("0");
		Serial.print(direccion[i], HEX);
	}
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
	
	//Iniciar sensor DS18B20
	ds18b20.begin();
	numTermometros = ds18b20.getDeviceCount();
	Serial.printf("Encontrados %d termometros\n", numTermometros);
	ds18b20.setResolution(TEMP_12_BIT);
	
	
	// Iniciar entrada botón
	pinMode(BOTON, INPUT_PULLUP);
	pinMode(2, OUTPUT);
	digitalWrite(2, HIGH);
	
}

void loop() {
	float temp_congelador, temp_frigo;
	static unsigned long ultimaMedida = 0;

	DeviceAddress direccion;

	if ((millis()-ultimaMedida) > PERIODO) { // Si se ha pulsado el botón
		Serial.println((millis() - ultimaMedida));
		ultimaMedida = millis();
		ds18b20.requestTemperatures(); 
		temp_congelador = ds18b20.getTempCByIndex(0); // Obtener temperatura
		temp_frigo = ds18b20.getTempCByIndex(1); // Obtener temperatura
		
		Serial.printf("Temperatura congelador: %f\nDirección DS18B20: ", temp_congelador);
		if (ds18b20.getAddress(direccion, 0)) {
			imprimirDireccion(direccion);
		}
		else {
			Serial.print("No encontrado");
		}
		Serial.print("\n");
		Serial.printf("Temperatura frigo: %f\nDirección DS18B20: ", temp_frigo);
		if (ds18b20.getAddress(direccion, 1)) {
			imprimirDireccion(direccion);
		}
		else {
			Serial.print("No encontrado");
		}
		Serial.print("\n\n");

		digitalWrite(2, LOW);
		//enviar_dato_ifttt(temperatura, presion, "mail");
		//enviar_dato_ifttt(temperatura, "twitt"); // Enviar un Twitt
		enviar_dato_thingspeak(temp_congelador, temp_frigo); // Enviar dato para dibujar gráfica en ThingSpeak

		digitalWrite(2, HIGH);

	}

}
