<script lang="ts">
	import { Badge, Button, Card, Navbar, NavBrand } from 'flowbite-svelte';
	import { onDestroy, onMount } from 'svelte';

	import { connectMqtt, disconnectMqtt, type NetworkEntry, sendCommand } from '$lib/mqtt';

	let uptime = $state(0);
	let ledState = $state(false);
	let error = $state('');
	let connected = $state(false);
	let networks = $state<NetworkEntry[]>([]);
	let scanning = $state(false);
	let initialScan = false;
	let restartRequired = $state(false);
	let editingSsid = $state('');
	let editingPw = $state('');
	let manualSsid = $state('');
	let manualPw = $state('');

	function toggleLed() {
		sendCommand('toggle');
	}

	function scanWifi() {
		scanning = true;
		sendCommand('wifiscan');
	}

	function saveCred(ssid: string, pw: string) {
		sendCommand('wifi_save', { ssid, pw });
		editingSsid = '';
		editingPw = '';
	}

	function forgetCred(ssid: string) {
		sendCommand('wifi_forget', { ssid });
		restartRequired = true;
	}

	function addManual() {
		if (manualSsid.trim()) {
			sendCommand('wifi_save', { ssid: manualSsid.trim(), pw: manualPw });
			manualSsid = '';
			manualPw = '';
		}
	}

	onMount(() => {
		connectMqtt(
			(data) => {
				uptime = data.uptime;
				ledState = data.led;
				error = '';
			},
			(conn) => {
				connected = conn;
				if (conn && !initialScan) {
					initialScan = true;
					scanWifi();
				}
				if (!conn) {
					error = 'MQTT disconnected. Reconnecting...';
				}
			},
			(nets) => {
				networks = nets;
				scanning = false;
			}
		);
	});

	onDestroy(() => {
		disconnectMqtt();
	});
</script>

<Navbar>
	<NavBrand href="/">
		<img src="favicon.png" class="me-3 h-6 sm:h-9" alt="ESP32 Logo" />
		<span class="self-center whitespace-nowrap text-xl font-semibold dark:text-white"
			>SvelteESP32</span
		>
	</NavBrand>
</Navbar>

<div class="container mx-auto p-4 max-w-4xl">
	{#if error}
		<div
			class="mb-4 rounded-lg bg-yellow-50 p-4 text-yellow-800 dark:bg-yellow-900 dark:text-yellow-300"
		>
			{error}
		</div>
	{/if}

	<Card size="xl" class="mb-6 p-4">
		<h5 class="mb-2 text-2xl font-bold tracking-tight text-gray-900 dark:text-white">
			ESP32 Control
		</h5>
		<div class="flex items-center gap-4 mb-4">
			<span class="text-gray-700 dark:text-gray-300">Uptime:</span>
			<Badge color="blue">{uptime}s</Badge>
			<span class="text-gray-700 dark:text-gray-300">LED:</span>
			<Badge color={ledState ? 'green' : 'gray'}>{ledState ? 'ON' : 'OFF'}</Badge>
		</div>
		<div class="flex gap-3">
			<Button onclick={toggleLed} disabled={!connected} class="w-fit">Toggle LED</Button>
			<Button
				onclick={() => sendCommand('restart')}
				disabled={!connected}
				color="red"
				class="w-fit"
			>
				Restart
			</Button>
		</div>
	</Card>

	<Card size="xl" class="mb-6 p-4">
		<h5 class="mb-4 text-2xl font-bold tracking-tight text-gray-900 dark:text-white">
			WiFi Networks
		</h5>
		{#if restartRequired}
			<div
				class="mb-4 rounded-lg bg-yellow-50 p-3 text-sm text-yellow-800 dark:bg-yellow-900 dark:text-yellow-300"
			>
				Credential removed. Restart required for changes to take effect.
			</div>
		{/if}
		<Button onclick={scanWifi} disabled={!connected || scanning} class="mb-4 w-fit">
			{scanning ? 'Scanning...' : 'Scan WiFi'}
		</Button>
		{#if networks.length > 0}
			<div class="overflow-x-auto">
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
						{#each networks as net (net.bssid)}
							<tr
								class="border-b dark:border-gray-600 {net.connected
									? 'bg-green-50 dark:bg-green-900/20'
									: (net.known
										? 'bg-blue-50 dark:bg-blue-900/20'
										: '')}"
							>
								<td class="px-3 py-2 font-medium">{net.ssid}</td>
								<td class="px-3 py-2">{net.rssi} dBm</td>
								<td class="px-3 py-2">{net.channel}</td>
								<td class="px-3 py-2">{net.auth}</td>
								<td class="px-3 py-2">
									{#if editingSsid === net.ssid}
										<div class="flex items-center gap-2">
											<input
												type="password"
												placeholder="Password"
												bind:value={editingPw}
												class="w-32 rounded border border-gray-300 px-2 py-1 text-xs dark:border-gray-600 dark:bg-gray-700 dark:text-white"
											/>
											<button
												onclick={() => saveCred(net.ssid, editingPw)}
												class="text-xs text-green-600 hover:underline dark:text-green-400"
												>OK</button
											>
											<button
												onclick={() => {
													editingSsid = '';
													editingPw = '';
												}}
												class="text-xs text-gray-500 hover:underline dark:text-gray-400"
												>Cancel</button
											>
										</div>
									{:else if !net.known}
										<button
											onclick={() => {
												editingSsid = net.ssid;
												editingPw = '';
											}}
											class="text-xs text-blue-600 hover:underline dark:text-blue-400">Save</button
										>
									{:else if !net.connected}
										<button
											onclick={() => forgetCred(net.ssid)}
											class="text-xs text-red-600 hover:underline dark:text-red-400">Forget</button
										>
									{/if}
								</td>
							</tr>
						{/each}
					</tbody>
				</table>
			</div>
		{:else if !scanning}
			<p class="text-gray-500 dark:text-gray-400 text-sm">
				No networks found. Press Scan WiFi to start.
			</p>
		{/if}
		<div class="mt-4 flex items-center gap-2">
			<input
				type="text"
				placeholder="SSID"
				bind:value={manualSsid}
				class="rounded border border-gray-300 px-2 py-1 text-sm dark:border-gray-600 dark:bg-gray-700 dark:text-white"
			/>
			<input
				type="password"
				placeholder="Password"
				bind:value={manualPw}
				class="rounded border border-gray-300 px-2 py-1 text-sm dark:border-gray-600 dark:bg-gray-700 dark:text-white"
			/>
			<Button
				onclick={addManual}
				disabled={!connected || !manualSsid.trim()}
				size="xs"
				class="w-fit">Add</Button
			>
		</div>
	</Card>
</div>
