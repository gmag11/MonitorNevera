
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

#define LED 2
#define PIN_1WIRE D2

const char *iftttURL1 = "https://maker.ifttt.com/trigger/";
const char *iftttURL2 = "/with/key/";

const char *iftttFingerprint = "c0 5d 08 5e e1 3e e0 66 f3 79 27 1a ca 1f fc 09 24 11 61 62"; // Fingerprint para comprobar la dirección https de IFTTT

#define PERIODO 60 //Tiempo en segundos entre lecturas consecutivas de datos

OneWire oneWire(PIN_1WIRE); //Bus 1-Wire
DallasTemperature ds18b20(&oneWire); //Sensor de temperatura
HTTPClient clienteHTTP; // Cliente para conectar a IFTTT
WiFiClient clienteWiFi; // Cliente para conectar a Thingspeak

// Enviar temperaturas a IFTTT/Maker
void enviar_dato_ifttt(float congelador, float frigo, const char *iftttEvent) {
	String url;
	int resultadoHTTP;

	//Construimos la URL pasando los dos parámetros de temperatura
	url = String(iftttURL1) + iftttEvent + String(iftttURL2) + iftttToken + "?value1=" + String(congelador) +"&value2=" + String(frigo);
	Serial.println(url); // Debug

	if (!clienteHTTP.begin(url, iftttFingerprint)) { //Comprobar fingerprint
		Serial.println("Error de comprobación del servidor HTTPS");
	}
	resultadoHTTP = clienteHTTP.GET();
	if (resultadoHTTP > 0) {
		// Mostrar resultado de la petición enviado por el servidor HTTP
		Serial.printf("[HTTP] GET... code: %d\n", resultadoHTTP);

		if (resultadoHTTP == HTTP_CODE_OK) {
			String payload = clienteHTTP.getString();
			Serial.println(payload);
			Serial.println();
		}
	}
	else { // Error en la petición
		Serial.printf("[HTTP] GET... failed, error: %s\n\n", clienteHTTP.errorToString(resultadoHTTP).c_str());
	}

	clienteHTTP.end(); // Cerrar conexión
}

void enviar_dato_thingspeak(float congelador, float frigo) {
#ifndef WIFICONFIG
	const long channelID = ID_DE_TU_CANAL_EN_THINGSPEAK;
	const char * ApiKey = "API_KEY_DE_ESCRITURA";
#endif

	ThingSpeak.begin(clienteWiFi); // Iniciar petición Thingspeak

	ThingSpeak.setField(1, congelador); // Registrar primer dato
	ThingSpeak.setField(2, frigo);		// Registrar segundo dato

	// Los datos se envían realmente ahora
	int estado = ThingSpeak.writeFields(channelID,ApiKey); 
	Serial.printf("Enviado dato a ThingSpeak. Resultado: %d\n\n", estado);
}

// Función para imprimir la dirección de un termómetro DS18B20
void imprimirDireccion(DeviceAddress direccion) {
	for (int i=0; i<8; i++) {
		if (direccion[i] < 16) // La dirección son 16 bytes
			Serial.print("0");
		Serial.print(direccion[i], HEX);
	}
}

void setup()
{
#ifndef WIFICONFIG
	// Datos para configurar IP fija, sin DHCP. Si se usa DHCP no son necesarias estas variables
	/*
	const IPAddress ip(192, 168, 1, 10); // IP asignada al ESP8266
	const IPAddress gw(192, 168, 1, 1); // IP de tu router de Internet
	const IPAddress mask(255,255,255,0); // Máscara de subred
	const IPAddress dns(8,8,8,8); // Dirección del DNS
	*/
#endif


	// Iniciar puerto serie
	Serial.begin(115200);
	Serial.println();

	// Conectar WiFi, solo es necesario hacerlo una vez. El ESP8266 guarda la configuración en la flash
	/*WiFi.mode(WIFI_STA);
	WiFi.begin(wifiSSID, wifiPass)*/
	
	// Configurar IP fija para que la ocnexión sea más rápida
	WiFi.config(ip,gw,mask,dns);
	while (!WiFi.isConnected()) { // Esperar a que esté conectado
		delay(100);
		Serial.print(".");
	}
	Serial.println();
	Serial.printf("IP Address: %s\n\n", WiFi.localIP().toString().c_str());
	
	//Iniciar sensor DS18B20
	ds18b20.begin();
	uint8_t numTermometros = ds18b20.getDeviceCount();
	Serial.printf("Encontrados %d termometros\n", numTermometros);
	ds18b20.setResolution(TEMP_12_BIT); // Máxima resoulución, medida más lenta
	
	// Configurar LED de la placa
	pinMode(LED, OUTPUT);
	digitalWrite(LED, HIGH);
	
}

void loop() {
	float temp_congelador, temp_frigo;

	// Configuración de umbrales de alarma de temperatura
	// Hay que calcularlos de forma que estén por encima de los valores normales. Prueba y error
	const int umbralCongelador = -8;
	const int umbralFrigo = 12;

	DeviceAddress direccion;

	//Leer temperaturas
	ds18b20.requestTemperatures(); 
	temp_congelador = ds18b20.getTempCByIndex(0); // Obtener congelador congelador. 
												  // El índice depende de tu dispositivo.
												  // Se ordenan mediante la dirección
	temp_frigo = ds18b20.getTempCByIndex(1); // Obtener congelador frigorífico
		
	Serial.printf("Temperatura congelador: %f\nDireccion DS18B20: ", temp_congelador);
	if (ds18b20.getAddress(direccion, 0)) {
		imprimirDireccion(direccion);
	}
	else {
		Serial.print("No encontrado");
	}
	Serial.print("\n");
	Serial.printf("Temperatura frigo: %f\nDireccion DS18B20: ", temp_frigo);
	if (ds18b20.getAddress(direccion, 1)) {
		imprimirDireccion(direccion);
	}
	else {
		Serial.print("No encontrado");
	}
	Serial.print("\n\n");

	
	uint32_t alarmaIniciada; // Hay una alarma en curso? Si la hay no volver a enviar alarma

	ESP.rtcUserMemoryRead(64, &alarmaIniciada, sizeof(uint32_t)); // Leer valor de la memoria RTC
	Serial.printf("Memoria RTC = %d\n", alarmaIniciada);

	if (alarmaIniciada == 0) { // Si no había alarma
		// Si la temperatura es alta
		if (temp_congelador > umbralCongelador || temp_frigo > umbralFrigo) {

			enviar_dato_ifttt(temp_congelador, temp_frigo, "twitt"); // Enviar un Twitt
			alarmaIniciada = 1; // Alarma en curso
			ESP.rtcUserMemoryWrite(64, &alarmaIniciada, sizeof(uint32_t));
			Serial.print("Alarma enviada\n");
		}
	} else { // Ya había alarma en curso
		// Si la temperatura es normal otra vez
		if (temp_congelador <= umbralCongelador && temp_frigo <= umbralFrigo) {
			enviar_dato_ifttt(temp_congelador, temp_frigo, "twitt"); // Enviar un Twitt
			alarmaIniciada = 0; // La alarma ha finalizado
			ESP.rtcUserMemoryWrite(64, &alarmaIniciada, sizeof(uint32_t));
			Serial.print("Fin de alarma\n");
		}
	}
	
	enviar_dato_thingspeak(temp_congelador, temp_frigo); // Enviar dato para dibujar gráfica en ThingSpeak

	// Parpadear el LED
	digitalWrite(LED, LOW);
	delay(100);
	digitalWrite(LED, HIGH);

	ESP.deepSleep(PERIODO * 1000000); // Desconectar ESP hasta la siguiente medida

}
