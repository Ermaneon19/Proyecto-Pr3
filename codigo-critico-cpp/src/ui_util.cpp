#include "ui_util.h"

#include "mesh_util.h"
#include "util.h"

#include <godot_cpp/classes/center_container.hpp>
#include <godot_cpp/classes/color_rect.hpp>
#include <godot_cpp/classes/panel_container.hpp>
#include <godot_cpp/classes/style_box_empty.hpp>

using namespace godot;

namespace cc {
namespace ui {

const Color AMBER(0.95f, 0.65f, 0.15f);
const Color BLUE(0.22f, 0.45f, 0.95f);
const Color CYAN(0.3f, 0.85f, 1.0f);
const Color RED(1.0f, 0.35f, 0.3f);
const Color MUTED(0.68f, 0.73f, 0.78f);
const Color PANEL_BG(0.05f, 0.06f, 0.08f, 0.94f);

// Fondo de las pantallas: degradado con una rejilla que se desplaza y vineta.
static const char *SHADER_BACKDROP = R"(
shader_type canvas_item;
void fragment() {
	vec2 uv = UV;
	vec3 col = mix(vec3(0.02, 0.03, 0.045), vec3(0.08, 0.05, 0.03), uv.y);
	vec2 g = uv * vec2(32.0, 18.0);
	g.y += TIME * 0.25;
	vec2 f = abs(fract(g) - 0.5);
	float line = smoothstep(0.46, 0.5, max(f.x, f.y));
	col += vec3(0.95, 0.65, 0.15) * line * 0.07 * (0.3 + uv.y);
	vec2 cell = floor(g);
	float n = fract(sin(dot(cell, vec2(12.9898, 78.233))) * 43758.5453);
	float blink = step(0.965, n) * (0.5 + 0.5 * sin(TIME * 2.0 + n * 40.0));
	col += vec3(0.2, 0.7, 0.9) * blink * 0.10;
	float v = smoothstep(0.95, 0.25, length(uv - 0.5));
	COLOR = vec4(col * (0.35 + 0.65 * v), 1.0);
}
)";

Ref<StyleBoxFlat> box(const Color &bg, const Color &border, int border_width, int pad, int radius) {
	Ref<StyleBoxFlat> s;
	s.instantiate();
	s->set_bg_color(bg);
	s->set_border_color(border);
	s->set_border_width_all(border_width);
	s->set_corner_radius_all(radius);
	s->set_content_margin_all(pad);
	return s;
}

Ref<Theme> make_theme() {
	Ref<Theme> t;
	t.instantiate();
	t->set_default_font_size(18);
	t->set_color("font_outline_color", "Label", Color(0, 0, 0));
	t->set_constant("outline_size", "Label", 6);

	t->set_stylebox("normal", "Button", box(Color(0.11f, 0.13f, 0.17f), AMBER.darkened(0.5f), 2));
	t->set_stylebox("hover", "Button", box(Color(0.22f, 0.18f, 0.1f), AMBER, 2));
	t->set_stylebox("pressed", "Button", box(Color(0.34f, 0.24f, 0.08f), AMBER, 2));
	t->set_stylebox("disabled", "Button", box(Color(0.08f, 0.08f, 0.09f), Color(0.25f, 0.25f, 0.25f), 2));
	// Foco visible: permite navegar todos los menus con el teclado.
	t->set_stylebox("focus", "Button", box(Color(0, 0, 0, 0), Color(1.0f, 0.9f, 0.6f), 3));
	t->set_color("font_hover_color", "Button", Color(1.0f, 0.9f, 0.7f));
	t->set_color("font_focus_color", "Button", Color(1.0f, 0.95f, 0.8f));

	t->set_stylebox("panel", "PanelContainer", box(PANEL_BG, AMBER, 2, 26, 8));

	t->set_stylebox("normal", "LineEdit", box(Color(0.03f, 0.04f, 0.05f), AMBER.darkened(0.4f), 2, 12));
	t->set_stylebox("focus", "LineEdit", box(Color(0.05f, 0.06f, 0.08f), AMBER, 2, 12));
	t->set_color("font_placeholder_color", "LineEdit", Color(0.45f, 0.48f, 0.52f));
	t->set_color("caret_color", "LineEdit", AMBER);
	return t;
}

Label *label(const String &text, int size, const Color &color, bool centered) {
	Label *l = memnew(Label);
	l->set_text(text);
	l->add_theme_font_size_override("font_size", size);
	l->add_theme_color_override("font_color", color);
	l->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);
	if (centered) {
		l->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_CENTER);
	}
	return l;
}

Button *button(const String &text, int font_size, const Vector2 &min_size) {
	Button *b = memnew(Button);
	b->set_text(text);
	b->set_custom_minimum_size(min_size);
	b->add_theme_font_size_override("font_size", font_size);
	return b;
}

ProgressBar *bar(const Color &fill, const Color &border, const Vector2 &min_size) {
	ProgressBar *p = memnew(ProgressBar);
	p->set_show_percentage(false);
	p->set_custom_minimum_size(min_size);
	p->add_theme_stylebox_override("background", box(Color(0.02f, 0.02f, 0.03f, 0.85f), border, 2, 0, 3));
	p->add_theme_stylebox_override("fill", box(fill, Color(0, 0, 0, 0), 0, 0, 3));
	p->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);
	return p;
}

VBoxContainer *screen_panel(Control *root, float min_width, int separation) {
	root->set_anchors_and_offsets_preset(Control::PRESET_FULL_RECT);
	root->set_theme(make_theme());

	ColorRect *bg = memnew(ColorRect);
	bg->set_anchors_and_offsets_preset(Control::PRESET_FULL_RECT);
	bg->set_material(mu::shader_material(SHADER_BACKDROP));
	bg->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);
	root->add_child(bg);

	CenterContainer *center = memnew(CenterContainer);
	center->set_anchors_and_offsets_preset(Control::PRESET_FULL_RECT);
	root->add_child(center);

	PanelContainer *panel = memnew(PanelContainer);
	panel->set_custom_minimum_size(Vector2(min_width, 0));
	center->add_child(panel);

	VBoxContainer *v = memnew(VBoxContainer);
	v->add_theme_constant_override("separation", separation);
	panel->add_child(v);
	return v;
}

} // namespace ui
} // namespace cc
