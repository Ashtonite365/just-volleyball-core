

#include "gate/content_share.hpp"
#include "gate/safe_content.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

static int fails = 0;
#define CHECK(cond) do { if (!(cond)) { std::cerr << "FAIL " << __FILE__ << ":" << __LINE__ << " " << #cond << "\n"; fails++; } } while (0)

namespace CS = jvm::content;

static std::vector<uint8_t> slurp(const std::string &p) {
	std::ifstream f(p, std::ios::binary);
	return std::vector<uint8_t>((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
}

static bool curl_get(const std::string &url, const std::string &out, int64_t cap) {
	if (!CS::url_allowed(url)) return false;
	std::string cmd = "curl -sS --fail --max-redirs 0 --max-filesize " + std::to_string(cap)
			+ " -o '" + out + "' -w '%{http_code}' -- '" + url + "'";
	FILE *fp = popen(cmd.c_str(), "r");
	if (!fp) return false;
	char buf[16] = {};
	if (!fgets(buf, sizeof buf, fp)) { pclose(fp); return false; }
	int st = pclose(fp);
	return st == 0 && std::string(buf) == "200";
}

static std::string hex_of(const std::vector<uint8_t> &b) {
	return CS::to_hex(CS::sha256(b.data(), b.size()));
}

static int unit_tests() {
	const std::string B = CS::kRawBase;
	CHECK(CS::url_allowed(B + "balls/1011_beach/skin.json"));
	CHECK(CS::url_allowed(std::string(CS::kRepoPrefix) + "0123456789abcdef0123456789abcdef01234567/balls/1011_beach/skin.json"));
	for (const char *u : {
		"https://evil.example/balls/1011_beach/skin.json",
		"http://raw.githubusercontent.com/Ashtonite365/just-volleyball-assets/main/balls/1011_beach/skin.json",
		"https://raw.githubusercontent.com/evil/just-volleyball-assets/main/balls/1011_beach/skin.json",
		"https://raw.githubusercontent.com/Ashtonite365/other/main/balls/1011_beach/skin.json",
		"https://github.com/Ashtonite365/just-volleyball-assets/raw/main/balls/1011_beach/skin.json",
		"https://raw.githubusercontent.com/Ashtonite365/just-volleyball-assets/main/balls/1011_beach/skin.json?token=1",
	}) CHECK(!CS::url_allowed(u));


	CS::CatalogItem ci;
	ci.folder = "1011_beach";
	const std::string json = "{\"id\":\"1011\",\"type\":\"ball\"}";
	auto jsha = CS::sha256(reinterpret_cast<const uint8_t *>(json.data()), json.size());
	ci.files.push_back({ "skin.json", jsha, uint64_t(json.size()), B + "balls/1011_beach/skin.json" });
	std::string dest = "/tmp/jv-fetch-unit";
	::system("rm -rf /tmp/jv-fetch-unit && mkdir -p /tmp/jv-fetch-unit");
	std::vector<uint8_t> body(json.begin(), json.end());
	CHECK(CS::install_verified_files(dest, ci, { { "skin.json", body } }).empty());

	std::vector<uint8_t> bad = body; bad[0] ^= 0xff;
	::system("rm -rf /tmp/jv-fetch-unit && mkdir -p /tmp/jv-fetch-unit");
	std::string err = CS::install_verified_files(dest, ci, { { "skin.json", bad } });
	CHECK(err.find("hash mismatch") != std::string::npos);

	std::vector<uint8_t> big(CS::kMaxFileBytes + 1, 'x');
	ci.files[0].size = 2;
	::system("rm -rf /tmp/jv-fetch-unit && mkdir -p /tmp/jv-fetch-unit");
	err = CS::install_verified_files(dest, ci, { { "skin.json", body } });
	CHECK(err.find("size mismatch") != std::string::npos);

	CHECK(!CS::url_allowed("https://evil.example/x.png"));
	if (fails) { std::cout << "RESULT failed (" << fails << ")\n"; return 1; }
	std::cout << "RESULT passed (unit)\n";
	return 0;
}

static int fetch_beach() {
	const std::string dest = "/tmp/jv-fetch-test/packs/1011_beach";
	::system("rm -rf /tmp/jv-fetch-test && mkdir -p /tmp/jv-fetch-test/packs/1011_beach /tmp/jv-fetch-test/dl");

	const char *files[][2] = {
		{ "skin.json", "https://raw.githubusercontent.com/Ashtonite365/just-volleyball-assets/main/balls/1011_beach/skin.json" },
		{ "ball_tex.png", "https://raw.githubusercontent.com/Ashtonite365/just-volleyball-assets/main/balls/1011_beach/ball_tex.png" },
		{ "preview.png", "https://raw.githubusercontent.com/Ashtonite365/just-volleyball-assets/main/balls/1011_beach/preview.png" },
	};
	CS::CatalogItem ci;
	ci.folder = "1011_beach";
	ci.id = 0x1011;
	std::vector<std::pair<std::string, std::vector<uint8_t>>> bodies;
	for (auto &f : files) {
		std::string url = f[1];
		if (!CS::url_allowed(url)) { std::cerr << "FAIL url_allowed " << url << "\n"; return 1; }
		std::string out = std::string("/tmp/jv-fetch-test/dl/") + f[0];
		if (!curl_get(url, out, int64_t(CS::kMaxFileBytes))) { std::cerr << "FAIL curl " << url << "\n"; return 1; }
		auto b = slurp(out);
		if (b.empty()) { std::cerr << "FAIL empty " << f[0] << "\n"; return 1; }
		auto sha = CS::sha256(b.data(), b.size());
		ci.files.push_back({ f[0], sha, uint64_t(b.size()), url });
		bodies.push_back({ f[0], b });
		std::cout << "fetched " << f[0] << " " << b.size() << " " << CS::to_hex(sha) << "\n";
	}
	std::string err = CS::install_verified_files(dest, ci, bodies);
	if (!err.empty()) { std::cerr << "FAIL install " << err << "\n"; return 1; }
	for (auto &f : files) {
		auto got = slurp(dest + "/" + f[0]);
		if (got != slurp(std::string("/tmp/jv-fetch-test/dl/") + f[0])) { std::cerr << "FAIL written " << f[0] << "\n"; return 1; }
		std::string bad = jvm::safe::check_content_file(f[0], got.data(), got.size());
		if (!bad.empty()) { std::cerr << "FAIL content " << f[0] << " " << bad << "\n"; return 1; }
	}


	CHECK(!CS::url_allowed("https://evil.example/balls/1011_beach/skin.json"));

	auto good = bodies;
	if (!good.empty()) good[0].second[0] ^= 0x5a;
	::system("rm -rf /tmp/jv-fetch-test/packs/bad && mkdir -p /tmp/jv-fetch-test/packs/bad");
	err = CS::install_verified_files("/tmp/jv-fetch-test/packs/bad", ci, good);
	CHECK(err.find("hash mismatch") != std::string::npos);

	auto bigci = ci;
	bigci.files[0].size = 1;
	::system("rm -rf /tmp/jv-fetch-test/packs/big && mkdir -p /tmp/jv-fetch-test/packs/big");
	err = CS::install_verified_files("/tmp/jv-fetch-test/packs/big", bigci, bodies);
	CHECK(err.find("size mismatch") != std::string::npos);

	if (fails) { std::cout << "RESULT failed (" << fails << ")\n"; return 1; }
	std::cout << "RESULT passed (fetch 1011_beach from GitHub into /tmp/jv-fetch-test/packs/1011_beach)\n";
	return 0;
}

int main(int argc, char **argv) {
	std::string mode = argc > 1 ? argv[1] : "all";
	int rc = 0;
	if (mode == "unit" || mode == "all") rc |= unit_tests();
	if (mode == "fetch" || mode == "all") rc |= fetch_beach();
	return rc;
}
