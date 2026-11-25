



#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace jvm::pin {

struct Pin {
	std::string name;
	std::string version;
	std::string steam_buildid;
	std::string engine;
	int64_t pck_size = -1;
	std::string pck_sha256;
	int64_t exe_size = -1;
	std::string exe_sha256;
};



struct PinFile {
	std::vector<std::string> versions;
	std::vector<Pin> pins;
};

struct Observed {
	std::string game_version;
	std::string engine;
	int64_t pck_size = -1;
	std::string pck_sha256;
	int64_t exe_size = -1;
	std::string exe_sha256;
	std::string steam_buildid;
};

struct Result {
	bool ok = false;
	std::string matched;
	std::string reason;
	std::string expected_build;
	std::string found;
};

Result match_pins(const PinFile &file, const Observed &obs);
Result match_pins(const std::vector<Pin> &pins, const Observed &obs);
std::string lower_hex(std::string s);

std::string parse_acf_buildid(const std::string &acf_text);


std::string parse_source_const_string(const std::string &src, const std::string &name);

}
