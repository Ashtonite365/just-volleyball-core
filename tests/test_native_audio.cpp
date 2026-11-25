

#include "loader/native_audio_core.hpp"
#include <cstdio>
#include <string>
#include <vector>

using namespace jvm::native_audio;
static int fails = 0, passes = 0;
static void check(bool c, const char *what) {
	if (c) { passes++; std::printf("PASS %s\n", what); }
	else { fails++; std::printf("FAIL %s\n", what); }
}

static std::vector<std::string> sample_paths() {
	return {
		kBeachPath,
		kParkPath,
		kAudiencePath,
		"res://game/sfx/Announcer/ace.wav",
		"res://game/sfx/Crowd/crowd_cheer.wav",
		"res://game/ost/preview/volleyball menu.wav",
		"res://game/sfx/Volleyball_SFX/Volleyball_Hit_Hard_01.wav",
		"res://game/sfx/not_audio.txt",
		"res://game/sfx/Ambiance/717146__ghosttropic__short-sand-beach-waves-3.wav",
	};
}

int main() {
	check(is_audio_filename("waves.wav") && is_audio_filename("x.OGG") && is_audio_filename("a.mp3"),
		"audio extensions accepted");
	check(is_audio_filename("waves.wav.import") && is_audio_filename("x.ogg.remap"),
		"import/remap suffixes still count as audio");
	check(!is_audio_filename("readme.txt") && !is_audio_filename("script.gd"),
		"non-audio rejected");
	check(short_id_from_path(kBeachPath) == kBeachId, "beach short id");
	check(default_bus_from_path(kBeachPath) == "ambient", "beach default bus is ambient");
	check(default_bus_from_path("res://game/sfx/Announcer/ace.wav") == "announcer", "announcer bus");
	check(default_bus_from_path("res://game/sfx/Crowd/crowd_cheer.wav") == "crowd", "crowd bus");
	check(default_bus_from_path("res://game/ost/preview/volleyball menu.wav") == "music", "ost bus is music");
	check(default_bus_from_path("res://game/sfx/Volleyball_SFX/Volleyball_Hit_Hard_01.wav") == "sfx", "hit default bus is sfx");
	check(category_from_path(kBeachPath) == "ambiance", "beach category");

	{
		Session s;
		Stats st = s.stats();
		check(!st.started && !st.index_built && st.index_count == 0 && st.alias_count == 0
			&& st.cached_streams == 0 && st.players == 0 && st.nodes == 0
			&& s.allocation_count() == 0,
			"idle session allocates nothing");
		check(s.find("beach") == nullptr && s.play("beach", PlayOpts{}) == 0,
			"play before index fails without allocating");
		check(s.allocation_count() == 0 && !s.stats().started && s.stats().nodes == 0
			&& s.stats().players == 0 && s.stats().cached_streams == 0,
			"failed play leaves the system disabled");
	}

	{
		Session s;
		s.build_index(sample_paths());
		check(s.stats().index_built && s.stats().index_count == 7, "index has 7 unique streams");
		check(!s.stats().started && s.stats().players == 0 && s.stats().nodes == 0
			&& s.stats().cached_streams == 0,
			"building the index does not start the play system");
		check(s.find("beach") && s.find(kBeachPath) && s.find(kBeachId)
			&& s.find("beach")->path == kBeachPath,
			"beach resolved by alias, short id and path");
		check(s.find("park") && s.find("audience") && s.find("crowd_murmur"),
			"park and audience aliases");
		check(s.find("announcer/ace") && s.find("ost/preview/volleyball menu"),
			"announcer and ost short ids");
		int alloc_after_index = s.allocation_count();
		check(alloc_after_index > 0, "index accounts for entry allocations");

		PlayOpts one;
		int h = s.play("beach", one);
		check(h > 0 && s.stats().started && s.stats().nodes == 1
			&& s.stats().players == 1 && s.stats().cached_streams == 1,
			"first play starts the system and loads one stream + player + host node");
		check(s.voice(h) && s.voice(h)->bus == "ambient" && !s.voice(h)->loop,
			"one-shot beach uses the ambient bus");
		check(s.allocation_count() > alloc_after_index, "play accounts for stream/player/node");

		PlayOpts loop;
		loop.loop = true;
		loop.volume = 0.5f;
		int h2 = s.play("park", loop);
		check(h2 > 0 && h2 != h && s.stats().players == 2 && s.stats().cached_streams == 2,
			"second looped voice is a separate cached stream");
		check(s.set_volume(h2, 0.25f) && s.voice(h2)->volume == 0.25f, "set_volume");
		check(s.set_bus(h2, "sfx") && s.voice(h2)->bus == "sfx", "set_bus");

		check(s.stop(h) && s.stats().players == 1 && s.stats().cached_streams == 1
			&& s.stats().started && s.stats().nodes == 1,
			"stopping one voice frees its stream, system stays up");
		check(s.stop(h2) && s.stats().players == 0 && s.stats().cached_streams == 0
			&& !s.stats().started && s.stats().nodes == 0,
			"last stop frees the host node and disables the system");
		check(s.stats().index_count == 7, "path index survives going idle");
	}

	{
		Session s;
		s.build_index(sample_paths());
		PackAudio p = make_pack_audio(true, "beach", "ambient", 0.8f);
		check(p.present && p.ambience == "beach", "pack audio{} parses");
		int h = s.apply_pack(p);
		check(h > 0 && s.voice(h) && s.voice(h)->loop && s.voice(h)->bus == "ambient"
			&& s.stats().started,
			"pack apply plays looped beach ambience");
		PackAudio empty = make_pack_audio(false, "", "", 1.f);
		check(!empty.present, "missing audio{} block is not present");
		s.clear_pack();
		check(!s.stats().started && s.stats().players == 0 && s.stats().nodes == 0
			&& s.stats().cached_streams == 0,
			"clearing the pack returns to zero-cost idle");
		check(s.apply_pack(empty) == 0 && !s.stats().started && s.allocation_count() > 0
			&& s.stats().players == 0 && s.stats().nodes == 0,
			"applying an empty pack does not start the system");

		int alloc = s.allocation_count();
		PackAudio bad = make_pack_audio(true, "not-a-real-sound", "", 1.f);
		check(s.apply_pack(bad) == 0 && !s.stats().started && s.stats().nodes == 0
			&& s.allocation_count() == alloc,
			"unknown pack ambience does not start or allocate");

		for (const char *evil : { "res://mods/resources/packs/x/evil.tres", "user://evil.ogg", "../../evil.ogg",
				"res://game/sfx/../../mods/resources/packs/x/evil.ogg", "C:/Windows/evil.ogg", "/tmp/evil.ogg",
				"file:///tmp/evil.ogg", "res://game/sfx\\..\\evil.ogg", "res://game/sfx/x.tres", "res://game/sfx//x.ogg" }) {
			PackAudio e = make_pack_audio(true, evil, "", 1.f);
			check(!e.present && e.ambience.empty() && s.apply_pack(e) == 0, (std::string("pack audio path refused: ") + evil).c_str());
		}
		check(!native_audio_path_ok("res://mods/resources/packs/x/a.ogg") && !native_audio_path_ok("res://game/sfx/a.tres")
			&& !native_audio_path_ok("res://game/sfx/a.ogg.import") && native_audio_path_ok("res://game/sfx/Ambiance/a.wav") && native_audio_path_ok("res://game/ost/m.ogg"),
			"only res://game/sfx|ost audio files may be streamed");
		PackAudio pv = make_pack_audio(true, "res://game/sfx/Ambiance/717146__ghosttropic__short-sand-beach-waves-3.wav", "", 1e30f);
		check(pv.present && pv.volume == 4.0f, "native res:// path accepted, volume clamped");
		PackAudio pn = make_pack_audio(true, "beach", "", 0.0f / 0.0f);
		check(pn.volume == 0.0f, "NaN volume becomes 0");
	}

	{
		Session s;
		s.build_index(sample_paths());
		PlayOpts o; o.loop = true;
		int a = s.play("beach", o);
		int b = s.play("announcer/ace", o);
		(void)a;
		check(s.stop_id("announcer/ace") == 1 && s.stats().players == 1, "stop by id");
		s.stop_all();
		check(s.stats().players == 0 && !s.stats().started, "stop_all returns to idle");
		(void)b;
	}

	std::printf("RESULT %d passed, %d failed\n", passes, fails);
	return fails ? 1 : 0;
}
