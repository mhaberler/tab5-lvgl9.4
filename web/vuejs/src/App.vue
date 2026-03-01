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
const restartRequired = ref(false);
const editingSsid = ref('');
const editingPw = ref('');
const manualSsid = ref('');
const manualPw = ref('');

function toggleLed() {
	sendCommand('toggle');
}

function scanWifi() {
	scanning.value = true;
	sendCommand('wifiscan');
}

function saveCred(ssid: string, pw: string) {
	sendCommand('wifi_save', { ssid, pw });
	editingSsid.value = '';
	editingPw.value = '';
}

function forgetCred(ssid: string) {
	sendCommand('wifi_forget', { ssid });
	restartRequired.value = true;
}

function addManual() {
	if (manualSsid.value.trim()) {
		sendCommand('wifi_save', { ssid: manualSsid.value.trim(), pw: manualPw.value });
		manualSsid.value = '';
		manualPw.value = '';
	}
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
		},
		() => {
			scanWifi();
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
			<div class="flex gap-3">
				<FwbButton @click="toggleLed" :disabled="!connected">
					Toggle LED
				</FwbButton>
				<FwbButton @click="() => sendCommand('restart')" :disabled="!connected" color="red">
					Restart
				</FwbButton>
			</div>
		</FwbCard>


		<FwbCard class="mb-6 p-4">
			<h5 class="mb-4 text-2xl font-bold tracking-tight text-gray-900 dark:text-white">
				WiFi Networks
			</h5>
			<div
				v-if="restartRequired"
				class="mb-4 rounded-lg bg-yellow-50 p-3 text-sm text-yellow-800 dark:bg-yellow-900 dark:text-yellow-300"
			>
				Credential removed. Restart required for changes to take effect.
			</div>
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
							<th class="px-3 py-2">Action</th>
						</tr>
					</thead>
					<tbody>
						<tr v-for="net in networks" :key="net.bssid" class="border-b dark:border-gray-600" :class="net.connected ? 'bg-green-50 dark:bg-green-900/20' : net.known ? 'bg-blue-50 dark:bg-blue-900/20' : ''">
							<td class="px-3 py-2 font-medium">{{ net.ssid }}</td>
							<td class="px-3 py-2">{{ net.rssi }} dBm</td>
							<td class="px-3 py-2">{{ net.channel }}</td>
							<td class="px-3 py-2">{{ net.auth }}</td>
							<td class="px-3 py-2">
								<div v-if="editingSsid === net.ssid" class="flex items-center gap-2">
									<input
										type="password"
										placeholder="Password"
										v-model="editingPw"
										class="w-32 rounded border border-gray-300 px-2 py-1 text-xs dark:border-gray-600 dark:bg-gray-700 dark:text-white"
									/>
									<button @click="saveCred(net.ssid, editingPw)" class="text-xs text-green-600 hover:underline dark:text-green-400">OK</button>
									<button @click="editingSsid = ''; editingPw = ''" class="text-xs text-gray-500 hover:underline dark:text-gray-400">Cancel</button>
								</div>
								<button v-else-if="!net.known" @click="editingSsid = net.ssid; editingPw = ''" class="text-xs text-blue-600 hover:underline dark:text-blue-400">Save</button>
								<button v-else-if="!net.connected" @click="forgetCred(net.ssid)" class="text-xs text-red-600 hover:underline dark:text-red-400">Forget</button>
							</td>
						</tr>
					</tbody>
				</table>
			</div>
			<p v-else-if="!scanning" class="text-gray-500 dark:text-gray-400 text-sm">
				No networks found. Press Scan WiFi to start.
			</p>
			<div class="mt-4 flex items-center gap-2">
				<input
					type="text"
					placeholder="SSID"
					v-model="manualSsid"
					class="rounded border border-gray-300 px-2 py-1 text-sm dark:border-gray-600 dark:bg-gray-700 dark:text-white"
				/>
				<input
					type="password"
					placeholder="Password"
					v-model="manualPw"
					class="rounded border border-gray-300 px-2 py-1 text-sm dark:border-gray-600 dark:bg-gray-700 dark:text-white"
				/>
				<FwbButton @click="addManual" :disabled="!connected || !manualSsid.trim()" size="xs">Add</FwbButton>
			</div>
		</FwbCard>
	</div>
</template>
