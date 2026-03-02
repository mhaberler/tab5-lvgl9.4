/// <reference types="svelte" />
/// <reference types="vite/client" />

interface ImportMetaEnvironment {
	/** Base URL of the ESP32 API host, e.g. http://192.168.1.42 */
	readonly VITE_API_HOST?: string;
}

interface ImportMeta {
	readonly env: ImportMetaEnvironment;
}
