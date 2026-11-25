#pragma once







#include <godot_cpp/classes/ref_counted.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/packed_byte_array.hpp>
#include <string>
#include <vector>

namespace jvm::gdc {
struct Token { uint32_t type; uint32_t line; uint8_t width; };
struct Buffer {
	uint32_t version = 0;
	std::vector<std::u32string> ids;
	uint32_t ncon = 0;
	std::vector<uint8_t> consts_blob;
	std::vector<double> const_num;
	std::vector<std::pair<uint32_t, uint32_t>> lines, cols;
	std::vector<Token> toks;
};

bool parse(const std::vector<uint8_t> &contents, uint32_t version, Buffer &out, std::string &why);
std::vector<uint8_t> build_contents(const Buffer &b);

bool consts_to_static(Buffer &b, const std::vector<std::pair<std::string, double>> &names, std::string &why);
}

namespace godot {
class JVPlayerConsts : public RefCounted {
	GDCLASS(JVPlayerConsts, RefCounted)
protected:
	static void _bind_methods();
public:

	Dictionary prepare_at_boot();

	static Dictionary verify();
	Dictionary check() { return verify(); }

	static Dictionary rewrite(const PackedByteArray &gdc);
};
}
