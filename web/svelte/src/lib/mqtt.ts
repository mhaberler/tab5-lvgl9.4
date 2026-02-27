/**
 * MQTT client for ESP32 communication over WebSocket.
 *
 * Connects to the PicoMQTT server's WebSocket transport.
 * In development, derives the URL from VITE_API_HOST.
 * In production, uses the current origin hostname.
 */

import mqtt from 'mqtt';

export type StatusData = { uptime: number; led: boolean };
export type StatusCallback = (data: StatusData) => void;
export type ConnectionCallback = (connected: boolean) => void;

const MQTTWS_PORT = 8883;

function getMqttUrl(): string {
	const apiHost: string = import.meta.env.VITE_API_HOST ?? '';
	if (apiHost) {
		const url = new URL(apiHost);
		return `ws://${url.hostname}:${MQTTWS_PORT}/mqtt`;
	}
	return `ws://${location.hostname}:${MQTTWS_PORT}/mqtt`;
}

let client: mqtt.MqttClient | null = null;

export function connectMqtt(onStatus: StatusCallback, onConnection?: ConnectionCallback) {
	const url = getMqttUrl();
	client = mqtt.connect(url, {
		clientId: `web_${Math.random().toString(36).slice(2, 8)}`,
		reconnectPeriod: 2000
	});

	client.on('connect', () => {
		onConnection?.(true);
		client!.subscribe('status');
	});

	client.on('close', () => {
		onConnection?.(false);
	});

	client.on('message', (_topic: string, payload: Buffer) => {
		try {
			const data = JSON.parse(payload.toString()) as StatusData;
			onStatus(data);
		} catch {
			// ignore malformed messages
		}
	});
}

export function sendCommand(action: string) {
	client?.publish('command', JSON.stringify({ action }));
}

export function disconnectMqtt() {
	client?.end();
	client = null;
}
