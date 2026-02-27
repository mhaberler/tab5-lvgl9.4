<script setup lang="ts">
import { FwbBadge, FwbButton, FwbCard, FwbNavbar, FwbNavbarLogo } from 'flowbite-vue';
import { onMounted, onUnmounted, ref } from 'vue';
import { connectMqtt, disconnectMqtt, sendCommand } from '$lib/mqtt';

const uptime = ref(0);
const ledState = ref(false);
const error = ref('');
const connected = ref(false);

const images = [
	{ alt: 'ESP32 board', src: './gallery/esp32-1.webp' },
	{ alt: 'ESP32 setup', src: './gallery/esp32-2.jpg' },
	{ alt: 'ESP32 project', src: './gallery/esp32-3.webp' }
];

function toggleLed() {
	sendCommand('toggle');
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
			if (!conn) {
				error.value = 'MQTT disconnected. Reconnecting...';
			}
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

		<h5 class="mb-4 text-xl font-bold text-gray-900 dark:text-white">Gallery (for demo only)</h5>
		<div class="grid grid-cols-1 gap-4 md:grid-cols-3">
			<img
				v-for="image in images"
				:key="image.src"
				:src="image.src"
				:alt="image.alt"
				class="h-auto max-w-full rounded-lg"
			/>
		</div>
	</div>
</template>
