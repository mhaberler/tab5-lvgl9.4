<script setup lang="ts">
import { FwbBadge, FwbButton, FwbCard, FwbNavbar, FwbNavbarLogo } from 'flowbite-vue';
import { onMounted, onUnmounted, ref } from 'vue';
import { type NetworkEntry, connectMqtt, disconnectMqtt, sendCommand } from '$lib/mqtt';

const uptime = ref(0);
const ledState = ref(false);
const error = ref('');
const connected = ref(false);
const networks = ref<NetworkEntry[]>([]);
const scanning = ref(false);
let initialScan = false;

const images = [
	{ alt: 'ESP32 board', src: './gallery/esp32-1.webp' },
	{ alt: 'ESP32 setup', src: './gallery/esp32-2.jpg' },
	{ alt: 'ESP32 project', src: './gallery/esp32-3.webp' }
];

function toggleLed() {
	sendCommand('toggle');
}

function scanWifi() {
	scanning.value = true;
	sendCommand('wifiscan');
}

onMounted(() => {
	connectMqtt(
		(data) => {
			uptime.value = data.uptime;
			ledState.value = data.led;
			error.value = '';
		},
		(conn) => {
			connected.value = conn;
			if (conn && !initialScan) {
				initialScan = true;
				scanWifi();
			}
			if (!conn) {
				error.value = 'MQTT disconnected. Reconnecting...';
			}
		},
		(nets) => {
			networks.value = nets;
			scanning.value = false;
		}
	);
});

onUnmounted(() => {
	disconnectMqtt();
});
</script>

<template>
	<FwbNavbar>
		<FwbNavbarLogo link="/" :image-url="'favicon.png'" alt="ESP32 Logo">
			VueESP32
		</FwbNavbarLogo>
	</FwbNavbar>

	<div class="container mx-auto p-4 max-w-4xl">
		<div
			v-if="error"
			class="mb-4 rounded-lg bg-yellow-50 p-4 text-yellow-800 dark:bg-yellow-900 dark:text-yellow-300"
		>
			{{ error }}
		</div>

		<FwbCard class="mb-6 p-4">
			<h5 class="mb-2 text-2xl font-bold tracking-tight text-gray-900 dark:text-white">
				ESP32 Control
			</h5>
			<div class="flex items-center gap-4 mb-4">
				<span class="text-gray-700 dark:text-gray-300">Uptime:</span>
				<FwbBadge type="indigo">{{ uptime }}s</FwbBadge>
				<span class="text-gray-700 dark:text-gray-300">LED:</span>
				<FwbBadge :type="ledState ? 'green' : 'dark'">{{ ledState ? 'ON' : 'OFF' }}</FwbBadge>
			</div>
			<FwbButton @click="toggleLed" :disabled="!connected">
				Toggle LED
			</FwbButton>
		</FwbCard>


		<FwbCard class="mb-6 p-4">
			<h5 class="mb-4 text-2xl font-bold tracking-tight text-gray-900 dark:text-white">
				WiFi Networks
			</h5>
			<FwbButton @click="scanWifi" :disabled="!connected || scanning" class="mb-4">
				{{ scanning ? 'Scanning...' : 'Scan WiFi' }}
			</FwbButton>
			<div v-if="networks.length > 0" class="overflow-x-auto">
				<table class="w-full text-sm text-left text-gray-700 dark:text-gray-300">
					<thead class="text-xs uppercase bg-gray-100 dark:bg-gray-700">
						<tr>
							<th class="px-3 py-2">SSID</th>
								<th class="px-3 py-2">RSSI</th>
							<th class="px-3 py-2">Ch</th>
							<th class="px-3 py-2">Auth</th>
						</tr>
					</thead>
					<tbody>
						<tr v-for="net in networks" :key="net.bssid" class="border-b dark:border-gray-600" :class="net.connected ? 'bg-green-50 dark:bg-green-900/20' : ''">
							<td class="px-3 py-2 font-medium">{{ net.ssid }}</td>
								<td class="px-3 py-2">{{ net.rssi }} dBm</td>
							<td class="px-3 py-2">{{ net.channel }}</td>
							<td class="px-3 py-2">{{ net.auth }}</td>
						</tr>
					</tbody>
				</table>
			</div>
			<p v-else-if="!scanning" class="text-gray-500 dark:text-gray-400 text-sm">
				No networks found. Press Scan WiFi to start.
			</p>
		</FwbCard>
	</div>
</template>
