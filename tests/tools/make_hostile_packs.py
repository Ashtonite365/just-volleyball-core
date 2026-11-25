#!/usr/bin/env python3



import json, os, shutil, struct, sys, zlib

mods, res_prefix, fx, which = sys.argv[1], sys.argv[2].rstrip('/'), sys.argv[3], sys.argv[4]
T = os.path.join(mods, 'resources', 'packs'); MD = os.path.join(mods, sys.argv[5] if len(sys.argv) > 5 and sys.argv[5] else 'models')

EVIL = '''[gd_resource type="ImageTexture" load_steps=2 format=3]

[sub_resource type="GDScript" id="GDScript_pwn"]
script/source = "extends ImageTexture
func _init():
	Engine.set_meta(\\"jvm_pwned\\", int(Engine.get_meta(\\"jvm_pwned\\", 0)) + 1)
	print(\\"JVM_PWNED canary script ran\\")
"

[resource]
script = SubResource("GDScript_pwn")
'''

def put(path, data):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, 'wb') as f: f.write(data if isinstance(data, bytes) else data.encode())

def pack(folder, manifest, files):
    d = os.path.join(T, folder)
    put(os.path.join(d, 'skin.json'), json.dumps(manifest))
    for k, v in files.items(): put(os.path.join(d, k), v)
    return d

def bomb_png(w, h):
    raw = zlib.compressobj(9)
    row = b'\x00' + b'\x00' * ((w + 7) // 8)
    body = b''.join(raw.compress(row) for _ in range(h)) + raw.flush()
    def chunk(t, d): return struct.pack('>I', len(d)) + t + d + struct.pack('>I', zlib.crc32(t + d) & 0xffffffff)
    return b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', w, h, 1, 0, 0, 0, 0)) + chunk(b'IDAT', body) + chunk(b'IEND', b'')

def glb_with_image_uri(src, uri):
    b = open(src, 'rb').read()
    jl = struct.unpack('<I', b[12:16])[0]
    j = json.loads(b[20:20 + jl]); rest = b[20 + jl:]
    j.setdefault('images', []).append({'uri': uri, 'mimeType': 'image/png'})
    j.setdefault('textures', []).append({'source': len(j['images']) - 1})
    js = json.dumps(j).encode(); js += b' ' * ((4 - len(js) % 4) % 4)
    body = struct.pack('<II', len(js), 0x4E4F534A) + js + rest
    return b'glTF' + struct.pack('<II', 2, 12 + len(body)) + body

red = open(os.path.join(fx, 'red.png'), 'rb').read()
if which == 'loader':
    me = res_prefix + '/resources/packs/hostile_tres/evil.tres'
    pack('hostile_tres', {'id': '1E01', 'name': 'Hostile Tres', 'type': 'stage', 'base_stage': 'grass', 'preview': 'preview.png',
                          'court': {'floor_tex': me, 'grass_tex': 'evil.tres', 'hill_tex': 'user://evil.tres'},
                          'sky': {'panorama': 'spoof.png'}, 'net': {'tape_tex': os.path.join(T, 'hostile_tres', 'evil.tres')}},
         {'evil.tres': EVIL, 'spoof.png': EVIL, 'preview.png': red})
    pack('hostile_global', {'id': '1E02', 'name': 'Hostile Global', 'kind': 'global'},
         {'evil.tres': EVIL, 'ok.png': red,
          'texture_map.json': json.dumps({res_prefix + '/resources/packs/hostile_global/evil.tres': 'ok.png',
                                          'res://game/../' + res_prefix[6:] + '/resources/packs/hostile_global/evil.tres': 'ok.png',
                                          'res://game/ball/ball.png': '../hostile_tres/evil.tres'})})
    pack('hostile_spoof', {'id': '1E03', 'name': 'Hostile Spoof', 'type': 'ball', 'preview': 'preview.png', 'ball': {'albedo_tex': 'ball_tex.png'}},
         {'ball_tex.png': EVIL, 'preview.png': red})
    pack('hostile_traversal', {'id': '1E04', 'name': 'Hostile Traversal', 'type': 'stage', 'base_stage': 'grass', 'preview': '../royal_court/preview.png',
                               'court': {'floor_tex': '../royal_court/court_floor.png', 'grass_tex': '..\\royal_court\\court_floor.png'},
                               'environment': {'mesh': '../env_ok_test/env.glb'}}, {})
    lights = [{'type': 'omni', 'position': [0, 5, 0], 'energy': 2, 'range': 20}] * 49
    lights_json = json.dumps({'id': '1E07', 'name': 'Hostile Lights', 'type': 'stage', 'base_stage': 'grass', 'preview': 'preview.png',
                              'environment': {'lights': lights}})
    lights_json = lights_json.replace('"lights": [', '"lights": [{"type": "omni", "energy": 1e999, "range": 1e999, "position": [1e999, 0, 0]}, ', 1)
    d = os.path.join(T, 'hostile_lights'); put(os.path.join(d, 'skin.json'), lights_json); put(os.path.join(d, 'preview.png'), red)
    ae = res_prefix + '/resources/packs/hostile_audio/evil.ogg'
    pack('hostile_audio', {'id': '1E08', 'name': 'Hostile Audio', 'type': 'stage', 'base_stage': 'grass', 'preview': 'preview.png',
                           'audio': {'ambience': ae, 'volume': 99}},
         {'evil.tres': EVIL, 'evil.ogg': EVIL, 'preview.png': red,
          'evil.ogg.import': '[remap]\nimporter="oggvorbisstr"\ntype="AudioStreamOggVorbis"\npath="' + res_prefix + '/resources/packs/hostile_audio/evil.tres"\n'})
    pack('hostile_bomb', {'id': '1E05', 'name': 'Hostile Bomb', 'type': 'stage', 'base_stage': 'grass', 'preview': 'preview.png', 'sky': {'panorama': 'bomb.png'}},
         {'bomb.png': bomb_png(20000, 20000), 'preview.png': red})
elif which == 'models':
    d = os.path.join(MD, 'hostile uri'); os.makedirs(d, exist_ok=True)
    put(os.path.join(d, 'skin.json'), json.dumps({'kind': 'model', 'id': 'E7B0', 'name': 'hostile uri', 'model': 'evil_uri.glb'}))
    put(os.path.join(d, 'evil.tres'), EVIL)
    put(os.path.join(d, 'evil_uri.glb'), glb_with_image_uri(os.path.join(fx, 'models', 'bean.glb'), 'evil.tres'))
