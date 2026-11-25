


#include "gate/safe_content.hpp"
#include "gate/content_share.hpp"

#include <cstdio>
#include <cstring>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

namespace S = jvm::safe;
static int g_fail = 0, g_pass = 0;
#define CHECK(c) do { if (c) ++g_pass; else { ++g_fail; std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); } } while (0)

using Bytes = std::vector<uint8_t>;
static Bytes file(const char *p) { std::ifstream f(p, std::ios::binary); return Bytes(std::istreambuf_iterator<char>(f), {}); }
static Bytes str(const std::string &s) { return Bytes(s.begin(), s.end()); }
static void put32(Bytes &b, uint32_t v) { for (int i = 0; i < 4; i++) b.push_back(uint8_t(v >> (8 * i))); }
static void putbe32(Bytes &b, uint32_t v) { for (int i = 3; i >= 0; i--) b.push_back(uint8_t(v >> (8 * i))); }

static Bytes png_header(uint32_t w, uint32_t h) {
	Bytes b = { 0x89, 'P', 'N', 'G', 0x0d, 0x0a, 0x1a, 0x0a };
	putbe32(b, 13); b.insert(b.end(), { 'I', 'H', 'D', 'R' }); putbe32(b, w); putbe32(b, h);
	b.insert(b.end(), { 8, 6, 0, 0, 0 }); putbe32(b, 0);
	return b;
}
static Bytes glb(const std::string &json, const Bytes &bin = {}) {
	std::string j = json; while (j.size() % 4) j += ' ';
	Bytes bb = bin; while (bb.size() % 4) bb.push_back(0);
	Bytes b = { 'g', 'l', 'T', 'F' };
	put32(b, 2); put32(b, uint32_t(12 + 8 + j.size() + (bin.empty() ? 0 : 8 + bb.size())));
	put32(b, uint32_t(j.size())); put32(b, 0x4E4F534A); b.insert(b.end(), j.begin(), j.end());
	if (!bin.empty()) { put32(b, uint32_t(bb.size())); put32(b, 0x004E4942); b.insert(b.end(), bb.begin(), bb.end()); }
	return b;
}
static std::string glb_json(const Bytes &g) {
	uint32_t n = uint32_t(g[12]) | (uint32_t(g[13]) << 8) | (uint32_t(g[14]) << 16) | (uint32_t(g[15]) << 24);
	return std::string(g.begin() + 20, g.begin() + 20 + n);
}

int main() {

	for (const char *ok : { "sky.png", "tex/floor.png", "./env.glb", "a/./b.png", "My Pack (2)/x.png" }) CHECK(S::pack_rel(ok).has_value());
	CHECK(*S::pack_rel("./tex//floor.png") == "tex/floor.png");
	for (const char *bad : { "", "../x.png", "a/../../x.png", "..", "a/..", "...", "/etc/passwd", "C:/Windows/x.png", "C:x.png", "\\\\host\\share\\x.png",
			"a\\..\\x.png", "res://game/x.tres", "user://evil.tres", "file:///c/x.png", "http://e/x.png", "~/x.png", ".hidden/x.png", "a/.godot/x.png",
			"a%2e%2e/x.png", "x.png\n", "x.png ", "x.png.", "a/b/c/d/e/f/g/h/i/x.png" })
		CHECK(!S::pack_rel(bad).has_value());
	CHECK(!S::pack_rel(std::string(201, 'a')).has_value());


	for (const char *e : { "png", "PNG", "jpg", "jpeg", "webp" }) CHECK(S::image_ext(e));
	for (const char *e : { "svg", "tga", "bmp", "exr", "hdr", "ktx", "dds", "tres", "res", "tscn", "scn", "gd", "gdc", "gdshader", "shader", "import", "dll", "so", "gdextension", "glb", "json", "" })
		CHECK(!S::image_ext(e));
	for (const char *e : { "glb", "json", "png" }) CHECK(S::content_ext(e));
	for (const char *e : { "gltf", "tres", "res", "tscn", "scn", "gd", "gdextension", "dll", "exe", "bin", "import", "cfg", "pck", "zip" }) CHECK(!S::content_ext(e));


	Bytes red = file("tests/fixtures/red.png");
	CHECK(red.size() > 33);
	S::ImageInfo info;
	CHECK(S::check_image(red.data(), red.size(), "png", &info).empty() && info.type == S::Type::Png && info.width == 4 && info.height == 4);
	CHECK(!S::check_image(red.data(), red.size(), "jpg").empty());
	CHECK(!S::check_image(red.data(), red.size(), "tres").empty());
	Bytes tres = str("[gd_resource type=\"Texture2D\" load_steps=2 format=3]\n[sub_resource type=\"GDScript\" id=\"1\"]\nscript/source = \"extends Resource\nfunc _init(): OS.execute('calc.exe', [])\"\n[resource]\nscript = SubResource(\"1\")\n");
	CHECK(!S::check_image(tres.data(), tres.size(), "png").empty());
	CHECK(S::sniff(tres.data(), tres.size()) == S::Type::None);
	Bytes res_bin = { 'R', 'S', 'R', 'C', 0, 0, 0, 0, 'G', 'D', 'S', 'c', 'r', 'i', 'p', 't' };
	CHECK(!S::check_image(res_bin.data(), res_bin.size(), "png").empty());
	Bytes mz = { 'M', 'Z', 0x90, 0, 3, 0, 0, 0, 4, 0, 0, 0 };
	CHECK(!S::check_image(mz.data(), mz.size(), "png").empty());
	Bytes big = png_header(20000, 20000);
	CHECK(S::check_image(big.data(), big.size(), "png").find("exceeds") != std::string::npos);
	Bytes wide = png_header(8192, 8192);
	CHECK(S::check_image(wide.data(), wide.size(), "png").find("pixel budget") != std::string::npos);
	Bytes okhdr = png_header(4096, 2048);
	CHECK(S::check_image(okhdr.data(), okhdr.size(), "png").empty());
	Bytes zero = png_header(0, 16);
	CHECK(!S::check_image(zero.data(), zero.size(), "png").empty());
	Bytes trunc(red.begin(), red.begin() + 20);
	CHECK(!S::check_image(trunc.data(), trunc.size(), "png").empty());
	Bytes jpg = { 0xff, 0xd8, 0xff, 0xe0, 0x00, 0x04, 0, 0, 0xff, 0xc0, 0x00, 0x11, 8, 0x01, 0x00, 0x02, 0x00, 3, 1, 0x22, 0, 2, 0x11, 1, 3, 0x11, 1, 0xff, 0xd9 };
	CHECK(S::check_image(jpg.data(), jpg.size(), "jpeg", &info).empty() && info.type == S::Type::Jpeg && info.width == 512 && info.height == 256);
	CHECK(!S::check_image(jpg.data(), jpg.size(), "png").empty());
	Bytes jbig = jpg; jbig[13] = 0xff; jbig[14] = 0xff; jbig[15] = 0xff; jbig[16] = 0xff;
	CHECK(!S::check_image(jbig.data(), jbig.size(), "jpg").empty());
	Bytes webp = { 'R', 'I', 'F', 'F', 0, 0, 0, 0, 'W', 'E', 'B', 'P', 'V', 'P', '8', 'L', 0, 0, 0, 0, 0x2f };
	uint32_t wl = (100 - 1) | ((50 - 1) << 14); put32(webp, wl); webp.insert(webp.end(), { 0, 0, 0, 0, 0, 0 });
	CHECK(S::check_image(webp.data(), webp.size(), "webp", &info).empty() && info.width == 100 && info.height == 50);
	Bytes huge(size_t(S::kMaxImageFileBytes) + 1, 0); std::memcpy(huge.data(), okhdr.data(), okhdr.size());
	CHECK(S::check_image(huge.data(), huge.size(), "png").find("larger than") != std::string::npos);


	const std::string asset = "\"asset\":{\"version\":\"2.0\"}";
	{
		Bytes g = glb("{" + asset + ",\"images\":[{\"uri\":\"evil.tres\"}]}");
		CHECK(S::sanitize_glb(g.data(), g.size()).error.find("uri") != std::string::npos);
		g = glb("{" + asset + ",\"images\":[{\"uri\":\"../../../../Users/x/evil.tres\",\"mimeType\":\"image/png\"}]}");
		CHECK(!S::sanitize_glb(g.data(), g.size()).error.empty());
		g = glb("{" + asset + ",\"images\":[{\"uri\":\"data:image/png;base64,AAAA\"}]}");
		CHECK(!S::sanitize_glb(g.data(), g.size()).error.empty());
		g = glb("{" + asset + ",\"buffers\":[{\"uri\":\"C:/Windows/win.ini\",\"byteLength\":4}]}");
		CHECK(S::sanitize_glb(g.data(), g.size()).error.find("uri") != std::string::npos);
		g = glb("{" + asset + ",\"buffers\":[{\"uri\":\"\\u002e\\u002e/x.bin\",\"byteLength\":4}]}");
		CHECK(!S::sanitize_glb(g.data(), g.size()).error.empty());
		g = glb("{" + asset + ",\"extensionsRequired\":[\"OMI_physics_body\"],\"extensionsUsed\":[\"OMI_physics_body\"]}");
		CHECK(S::sanitize_glb(g.data(), g.size()).error.find("OMI_physics_body") != std::string::npos);
		g = glb("{" + asset + ",\"extensionsUsed\":[\"KHR_materials_unlit\",\"GODOT_single_root\",\"OMI_physics_body\",\"EXT_evil\"],\"extras\":{\"script\":\"res://x.gd\"},"
				"\"nodes\":[{\"name\":\"n\",\"extras\":{\"a\":1},\"extensions\":{\"EXT_evil\":{\"code\":\"x\"}}}],"
				"\"materials\":[{\"name\":\"m\",\"extensions\":{\"KHR_materials_unlit\":{}}}]}");
		S::GlbResult r = S::sanitize_glb(g.data(), g.size());
		CHECK(r.error.empty() && r.stripped_extras == 2 && r.stripped_extensions == 3);
		std::string js = glb_json(r.glb);
		CHECK(js.find("EXT_evil") == std::string::npos && js.find("OMI_physics_body") == std::string::npos && js.find("extras") == std::string::npos && js.find("res://x.gd") == std::string::npos);
		CHECK(js.find("KHR_materials_unlit") != std::string::npos && js.find("GODOT_single_root") != std::string::npos);
		S::GlbResult again = S::sanitize_glb(r.glb.data(), r.glb.size());
		CHECK(again.error.empty() && again.glb == r.glb && again.stripped_extensions == 0);
		g = glb("{" + asset + ",\"buffers\":[{\"byteLength\":64}],\"bufferViews\":[{\"buffer\":0,\"byteLength\":64}]}", Bytes(8, 0));
		CHECK(!S::sanitize_glb(g.data(), g.size()).error.empty());
		g = glb("{" + asset + ",\"buffers\":[{\"byteLength\":8}],\"bufferViews\":[{\"buffer\":0,\"byteOffset\":4,\"byteLength\":8}]}", Bytes(8, 0));
		CHECK(S::sanitize_glb(g.data(), g.size()).error.find("out of bounds") != std::string::npos);
		Bytes spoofimg = tres; while (spoofimg.size() % 4) spoofimg.push_back(0);
		g = glb("{" + asset + ",\"buffers\":[{\"byteLength\":" + std::to_string(spoofimg.size()) + "}],\"bufferViews\":[{\"buffer\":0,\"byteLength\":" + std::to_string(spoofimg.size()) + "}],"
				"\"images\":[{\"bufferView\":0,\"mimeType\":\"image/png\"}]}", spoofimg);
		CHECK(S::sanitize_glb(g.data(), g.size()).error.find("images[0]") != std::string::npos);
		Bytes bomb = png_header(16384, 16384);
		g = glb("{" + asset + ",\"buffers\":[{\"byteLength\":" + std::to_string(bomb.size()) + "}],\"bufferViews\":[{\"buffer\":0,\"byteLength\":" + std::to_string(bomb.size()) + "}],"
				"\"images\":[{\"bufferView\":0,\"mimeType\":\"image/png\"}]}", bomb);
		CHECK(S::sanitize_glb(g.data(), g.size()).error.find("exceeds") != std::string::npos);
		g = glb("{" + asset + ",\"buffers\":[{\"byteLength\":" + std::to_string(red.size()) + "}],\"bufferViews\":[{\"buffer\":0,\"byteLength\":" + std::to_string(red.size()) + "}],"
				"\"images\":[{\"bufferView\":0,\"mimeType\":\"image/png\"}]}", red);
		r = S::sanitize_glb(g.data(), g.size());
		CHECK(r.error.empty() && r.images == 1);
		g = glb("{" + asset + ",\"images\":[{\"bufferView\":0,\"mimeType\":\"image/svg+xml\"}]}");
		CHECK(!S::sanitize_glb(g.data(), g.size()).error.empty());
		Bytes notglb = tres;
		CHECK(!S::sanitize_glb(notglb.data(), notglb.size()).error.empty());
		g = glb("{" + asset + "}"); g[8] ^= 1;
		CHECK(!S::sanitize_glb(g.data(), g.size()).error.empty());
		g = glb("{\"asset\":{\"version\":\"1.0\"}}");
		CHECK(!S::sanitize_glb(g.data(), g.size()).error.empty());
		std::string deep = "{" + asset + ",\"x\":" + std::string(200, '[') + std::string(200, ']') + "}";
		g = glb(deep);
		CHECK(!S::sanitize_glb(g.data(), g.size()).error.empty());
		g = glb("{" + asset + ",\"a\":1,\"a\":2}");
		CHECK(!S::sanitize_glb(g.data(), g.size()).error.empty());
	}

	for (const char *p : { "tests/fixtures/court_env/floor_deck.glb", "tests/fixtures/models/bean.glb", "tests/fixtures/models/badbones.glb",
			"dist/game_dir/mods/resources/packs/plump_bean/aero_bean.glb", "dist/game_dir/mods/resources/packs/02_champion_cap_hat/cap.glb" }) {
		Bytes b = file(p);
		if (b.empty()) { std::printf("SKIP %s (missing)\n", p); continue; }
		S::GlbResult r = S::sanitize_glb(b.data(), b.size());
		CHECK(r.error.empty());
		if (!r.error.empty()) std::printf("  %s: %s\n", p, r.error.c_str());
	}


	CHECK(S::check_content_file("sky.png", red.data(), red.size()).empty());
	CHECK(!S::check_content_file("sky.png", tres.data(), tres.size()).empty());
	CHECK(!S::check_content_file("evil.tres", tres.data(), tres.size()).empty());
	CHECK(!S::check_content_file("mod.gd", tres.data(), tres.size()).empty());
	CHECK(!S::check_content_file("bin/libx.dll", mz.data(), mz.size()).empty());
	Bytes sj = str("{\"id\":\"1011\",\"name\":\"x\"}");
	CHECK(S::check_content_file("skin.json", sj.data(), sj.size()).empty());
	Bytes sjbad = str("[gd_scene]");
	CHECK(!S::check_content_file("skin.json", sjbad.data(), sjbad.size()).empty());
	Bytes glbaspng = file("tests/fixtures/models/bean.glb");
	CHECK(!S::check_content_file("ball.png", glbaspng.data(), glbaspng.size()).empty());
	CHECK(!S::check_content_file("env.glb", red.data(), red.size()).empty());


	namespace CS = jvm::content;
	for (const char *bad : { "../evil.png", "a/../../evil.png", "/abs.png", "C:\\x.png", "..\\..\\mods\\bin\\x.dll", "mods/bin/libjvm.dll", "x.gdextension", "x.tres", ".hidden.png", "a/./b.png" })
		CHECK(!CS::safe_entry(bad));
	for (const char *bad : { "../x", "balls/..", "balls/../bin", "/abs", "balls/.x", "a//b", "balls/x y" }) CHECK(!CS::safe_repo_path(bad));
	CHECK(!CS::url_allowed("http://raw.githubusercontent.com/Ashtonite365/just-volleyball-assets/main/a/pack.zip"));


	CHECK(S::game_texture_target("res://game/ball/ball.png"));
	for (const char *bad : { "res://mods/resources/packs/evil/x.tres", "res://mods/x.png", "res://MODS/x.png", "user://x.png", "res://game/../mods/x.png",
			"res://game/x.tres", "res://game/x.gd", "res://game/x.tscn", "C:/x.png", "res:///abs.png", "res://.godot/imported/x.ctex", "res://game//x.png" })
		CHECK(!S::game_texture_target(bad));

	std::printf("RESULT %d passed, %d failed\n", g_pass, g_fail);
	return g_fail ? 1 : 0;
}
