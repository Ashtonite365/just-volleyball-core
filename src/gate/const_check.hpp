













#pragma once
#include <map>
#include <set>
#include <string>
#include <vector>

namespace jvm {
namespace consts {

enum class Kind { Gameplay, Protocol, Visual, Unknown };
enum class Sev { Error, Warn };

struct Finding {
	Sev sev = Sev::Error;
	std::string code;
	std::string name;
	std::string detail;
	std::string source;
};

struct Spec {
	bool is_int = false;
	bool has_stock = false;
	double stock = 0.0;
	bool has_bounds = false;
	double min = 0.0, max = 0.0;
};


struct Live {
	bool available = false;
	std::string reason;
	std::map<std::string, double> statics;
	std::set<std::string> still_const;
	std::map<std::string, Spec> specs;
};

struct Context {
	bool cap_open = true;
	std::string mode;
	bool synced_lobby = false;
	bool is_host = true;
	bool match_active = false;
};

struct Write {
	std::string name;
	std::string source;
	bool is_number = true;
	bool is_integral = false;
	bool finite = true;
	double value = 0.0;
};


struct Expect {
	std::map<std::string, double> approved;
	bool start_valid = false;
	std::map<std::string, double> start_set;
	bool host_valid = false;
	std::map<std::string, double> host_set;
};

const std::vector<std::string> &gameplay_names();
const std::vector<std::string> &protocol_names();
bool is_protocol(const std::string &name);
Kind classify(const std::string &name);
const char *kind_name(Kind k);

const std::vector<std::string> &folded_readers(const std::string &name);


bool default_bounds(const Spec &s, double &lo, double &hi);

bool same(double a, double b);

std::vector<Finding> check_write(const Write &w, const Context &c, const Live &l);
std::vector<Finding> audit(const Live &l, const Context &c, const Expect &e);
bool has_error(const std::vector<Finding> &f);

}
}
