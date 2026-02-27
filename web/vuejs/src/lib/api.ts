/**
 * Centralized API helper.
 *
 * During development set VITE_API_HOST in .env.development to point at
 * the ESP32, e.g.  VITE_API_HOST=http://192.168.1.42
 *
 * In production the variable is empty so all paths resolve against the
 * current origin (the ESP32 itself).
 */

const BASE_URL: string = import.meta.env.VITE_API_HOST ?? '';

/**
 * Wrapper around `fetch()` that prepends the configured API host.
 *
 * @param path  Absolute path starting with `/`, e.g. `/api/status`
 * @param init  Optional `RequestInit` (method, headers, body, …)
 * @returns     Parsed JSON response body
 * @throws      On network errors or non-ok HTTP status codes
 */
export async function apiFetch<T = unknown>(
	path: string,
	init?: RequestInit
): Promise<T> {
	const res = await fetch(`${BASE_URL}${path}`, init);
	if (!res.ok) {
		throw new Error(`API ${init?.method ?? 'GET'} ${path} -> ${res.status}`);
	}
	return res.json() as Promise<T>;
}
