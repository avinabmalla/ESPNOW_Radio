#include <Arduino.h>
#include <esp_wifi.h>
#include <esp_now.h>
#include <WiFi.h>
#include <esp_wifi_internal.h>

#define CHANNEL 10
#define TEST 1

struct Message
{
	uint8_t Payload[248];
	uint8_t PacketLength;
	uint8_t PacketId;
};

uint8_t broadcastAddress[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

uint8_t SendPacketId;
uint8_t ReceivedPacketId;

Message toSend;
Message receivedMsg;

uint8_t readBuffer[248];

void OnDataRecv(const uint8_t *mac, const uint8_t *incomingData, int len)
{
	memcpy(&receivedMsg, incomingData, sizeof(receivedMsg));
	if (receivedMsg.PacketId != ReceivedPacketId)
	{
		Serial.write(receivedMsg.Payload, receivedMsg.PacketLength);
		ReceivedPacketId = receivedMsg.PacketId;
	}
}

void write(uint8_t *data, size_t length)
{
	size_t start = 0;
	while (start < length)
	{
		int packetLength = length - start;
		if (packetLength > 248)
			packetLength = 248;

		for (int i = 0; i < packetLength; i++)
		{
			toSend.Payload[i] = data[start + i];
		}

		toSend.PacketId = SendPacketId;
		toSend.PacketLength = packetLength;

		//repeat send 10 times
		for (int i = 0; i < 10; i++)
		{
			esp_now_send(broadcastAddress, (uint8_t *)&toSend, sizeof(toSend));
		}

		SendPacketId++;
		start += packetLength;
	}
}

void setup()
{
	// Init Serial Monitor
	Serial.begin(115200);
	Serial.setTimeout(0);
	Serial.setRxBufferSize(8192);

	WiFi.disconnect();
	WiFi.mode(WIFI_STA);

	/*Stop wifi to change config parameters*/
	esp_wifi_stop();
	esp_wifi_deinit();

	/*Disabling AMPDU is necessary to set a fix rate*/
	wifi_init_config_t my_config = WIFI_INIT_CONFIG_DEFAULT(); //We use the default config ...
	my_config.ampdu_tx_enable = 0;							   //... and modify only what we want.
	esp_wifi_init(&my_config);								   //set the new config

	esp_wifi_start();

	esp_wifi_set_protocol(WIFI_IF_STA, WIFI_PROTOCOL_LR);
	esp_wifi_internal_set_fix_rate(ESP_IF_WIFI_STA, true, WIFI_PHY_RATE_LORA_250K);
	esp_wifi_set_channel(CHANNEL, WIFI_SECOND_CHAN_NONE);

	// Init ESP-NOW
	if (esp_now_init() != ESP_OK)
	{
		Serial.println("Error initializing ESP-NOW");
		return;
	}

	// Register peer
	esp_now_peer_info_t peerInfo;
	memcpy(peerInfo.peer_addr, broadcastAddress, 6);
	peerInfo.channel = CHANNEL;
	peerInfo.encrypt = false;

	// Add peer
	if (esp_now_add_peer(&peerInfo) != ESP_OK)
	{
		Serial.println("Failed to add peer");
		return;
	}

	esp_now_register_recv_cb(OnDataRecv);

	pinMode(13, OUTPUT);
}

char testMsg[248];
void loop()
{
	if (TEST)
	{
		sprintf(testMsg,"Test Message ID %d\r\n", SendPacketId);
		write((uint8_t*)testMsg,strlen(testMsg));
		delay(1000);
	}
	else
	{
		int count = Serial.readBytes(readBuffer, 248);
		write(readBuffer, count);
	}
}