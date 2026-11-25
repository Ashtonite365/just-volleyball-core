shader_type spatial;
render_mode unshaded;

uniform vec4 albedo : source_color = vec4(0.1, 0.8, 0.9, 1.0);
uniform vec4 sss_tint : source_color = vec4(0.6, 0.9, 1.0, 1.0);
uniform float flash = 0.0;
uniform vec4 flash_color : source_color = vec4(1.0);
void fragment() { ALBEDO = mix(albedo.rgb, flash_color.rgb, flash) + sss_tint.rgb * 0.0; }
