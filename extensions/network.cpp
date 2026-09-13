#include "network.h"

HttpRequest::HttpRequest(
	const wchar_t* agentName,
	const wchar_t* serverName,
	const wchar_t* action,
	const wchar_t* objectName,
	std::string body,
	const wchar_t* headers,
	DWORD port,
	const wchar_t* referrer,
	DWORD requestFlags,
	const wchar_t* httpVersion,
	const wchar_t** acceptTypes
)
{
	static std::atomic<HINTERNET> internet = NULL;
	if (!internet) internet = WinHttpOpen(agentName, WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, NULL, NULL, 0);

	// Cache koneksi per-server (keep-alive) supaya request berikutnya ke server yang
	// sama TIDAK melakukan TCP+TLS handshake ulang (hemat ~200-400ms per baris).
	// WinHTTP mengizinkan handle koneksi dipakai bersama antar thread; kita hanya
	// menjaga peta koneksi dengan mutex. Handle request tetap dibuat per-panggilan.
	static std::mutex connMutex;
	static std::unordered_map<std::wstring, HINTERNET> connCache;
	auto GetConnection = [&](HINTERNET session) -> HINTERNET
	{
		std::wstring key = std::wstring(serverName) + L":" + std::to_wstring(port);
		std::lock_guard<std::mutex> lock(connMutex);
		auto it = connCache.find(key);
		if (it != connCache.end() && it->second) return it->second;
		HINTERNET c = WinHttpConnect(session, serverName, port, 0);
		if (c) connCache[key] = c;
		return c;
	};

	if (internet)
		if (HINTERNET connection = GetConnection(internet)) // koneksi dipakai ulang (keep-alive)
			if (InternetHandle request = WinHttpOpenRequest(connection, action, objectName, httpVersion, referrer, acceptTypes, requestFlags))
			{
				// Timeout ketat agar request yang lambat/tersangkut GAGAL CEPAT dan tidak
				// memblokir kalimat berikutnya. Terjemahan normal balik <1s, jadi nilai
				// ini masih longgar tapi memangkas waktu tunggu terburuk secara signifikan.
				// resolve 2.5s, connect 2.5s, send 2.5s, receive 4s.
				WinHttpSetTimeouts(request, 2500, 2500, 2500, 4000);
				if (WinHttpSendRequest(request, headers, -1UL, body.empty() ? NULL : body.data(), body.size(), body.size(), NULL))
				{
					WinHttpReceiveResponse(request, NULL);

					//DWORD size = 0;
					//WinHttpQueryHeaders(request, WINHTTP_QUERY_RAW_HEADERS_CRLF, WINHTTP_HEADER_NAME_BY_INDEX, NULL, &size, WINHTTP_NO_HEADER_INDEX);
					//this->headers.resize(size);
					//WinHttpQueryHeaders(request, WINHTTP_QUERY_RAW_HEADERS_CRLF, WINHTTP_HEADER_NAME_BY_INDEX, this->headers.data(), &size, WINHTTP_NO_HEADER_INDEX);
					std::string data;
					DWORD availableSize, downloadedSize;
					do
					{
						availableSize = 0;
						WinHttpQueryDataAvailable(request, &availableSize);
						if (!availableSize) break;
						std::vector<char> buffer(availableSize);
						WinHttpReadData(request, buffer.data(), availableSize, &downloadedSize);
						data.append(buffer.data(), downloadedSize);
					} while (availableSize > 0);
					response = StringToWideString(data);
					// Koneksi TIDAK dipindah ke this->connection: ia milik cache (keep-alive)
					// dan sengaja dibiarkan hidup untuk request berikutnya. Hanya request
					// handle yang kita miliki & tutup otomatis.
					this->request = std::move(request);
				}
				else
				{
					errorCode = GetLastError();
					// Koneksi keep-alive mungkin sudah basi/putus -> buang dari cache agar
					// panggilan berikutnya membuat koneksi baru.
					std::wstring key = std::wstring(serverName) + L":" + std::to_wstring(port);
					std::lock_guard<std::mutex> lock(connMutex);
					auto it = connCache.find(key);
					if (it != connCache.end()) { WinHttpCloseHandle(it->second); connCache.erase(it); }
				}
			}
			else errorCode = GetLastError();
		else errorCode = GetLastError();
	else errorCode = GetLastError();
}

std::wstring Escape(const std::wstring& text)
{
	std::wstring escaped;
	for (unsigned char ch : WideStringToString(text)) escaped += FormatString(L"%%%02X", (int)ch);
	return escaped;
}

std::string Escape(const std::string& text)
{
	std::string escaped;
	for (unsigned char ch : text) escaped += FormatString("%%%02X", (int)ch);
	return escaped;
}

TEST(assert(JSON::Parse<wchar_t>(LR"([{"string":"hello world","boolean":false,"number":1.67e+4,"null":null,"array":[]},"hello world"])")));
