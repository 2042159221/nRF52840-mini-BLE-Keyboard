#include <errno.h>
#include <stdio.h>

#include <lvgl.h>

#include <display/status_screen.h>

#define STATUS_SCREEN_W 320
#define STATUS_SCREEN_H 172

#define COLOR_BG lv_color_hex(0x0b0f14)
#define COLOR_SURFACE lv_color_hex(0x151b22)
#define COLOR_ELEVATED lv_color_hex(0x202833)
#define COLOR_TEXT lv_color_hex(0xf4f8fb)
#define COLOR_TEXT_MUTED lv_color_hex(0x93a1af)
#define COLOR_DIVIDER lv_color_hex(0x2b3642)
#define COLOR_CYAN lv_color_hex(0x23e6d2)
#define COLOR_MAGENTA lv_color_hex(0xff4fd8)
#define COLOR_GREEN lv_color_hex(0x7cff6b)
#define COLOR_AMBER lv_color_hex(0xf8ba4c)
#define COLOR_RED lv_color_hex(0xff5a68)
#define COLOR_BLE lv_color_hex(0x4da3ff)
#define COLOR_USB lv_color_hex(0xa78bfa)

struct home_widgets {
	lv_obj_t *mode_chip;
	lv_obj_t *battery_label;
	lv_obj_t *charging_dot;
	lv_obj_t *title;
	lv_obj_t *subtitle;
	lv_obj_t *battery_bar;
	lv_obj_t *num_status;
	lv_obj_t *rgb_status;
	lv_obj_t *knob_status;
	lv_obj_t *rgb_accent;
};

static struct home_widgets home;
static lv_obj_t *root;
static lv_obj_t *settings_page;
static lv_obj_t *edit_page;
static lv_obj_t *confirm_page;

static const char *mode_text(enum kb_mode mode)
{
	switch (mode) {
	case KB_MODE_USB:
		return "USB";
	case KB_MODE_BLE:
		return "BLE";
	case KB_MODE_24G_RESERVED:
	default:
		return "2.4G";
	}
}

static lv_color_t mode_color(enum kb_mode mode)
{
	switch (mode) {
	case KB_MODE_USB:
		return COLOR_USB;
	case KB_MODE_BLE:
		return COLOR_BLE;
	case KB_MODE_24G_RESERVED:
	default:
		return COLOR_AMBER;
	}
}

static const char *rgb_mode_text(uint8_t mode, bool enabled)
{
	if (!enabled || mode == APP_RGB_MODE_OFF) {
		return "RGB Off";
	}

	if (mode == APP_RGB_MODE_STATIC) {
		return "RGB Static";
	}

	return "RGB Reactive";
}

static const char *encoder_action_text(uint8_t action)
{
	switch (action) {
	case APP_ENCODER_ACTION_VOLUME_UP:
		return "Vol+";
	case APP_ENCODER_ACTION_VOLUME_DOWN:
		return "Vol-";
	case APP_ENCODER_ACTION_MUTE:
		return "Mute";
	case APP_ENCODER_ACTION_PLAY_PAUSE:
		return "Play";
	case APP_ENCODER_ACTION_NEXT_TRACK:
		return "Next";
	case APP_ENCODER_ACTION_PREV_TRACK:
		return "Prev";
	case APP_ENCODER_ACTION_RGB_MODE_NEXT:
		return "RGB";
	case APP_ENCODER_ACTION_NONE:
	default:
		return "None";
	}
}

static uint8_t battery_percent(const struct status_screen_snapshot *snapshot)
{
	if ((snapshot->power.valid_flags & POWER_STATE_VALID_PERCENT) == 0U) {
		return 100U;
	}

	return snapshot->power.estimated_percent;
}

static lv_obj_t *make_label(lv_obj_t *parent, const char *text, lv_color_t color,
			    lv_coord_t x, lv_coord_t y, lv_coord_t w)
{
	lv_obj_t *label = lv_label_create(parent);

	lv_label_set_text(label, text);
	lv_obj_set_style_text_color(label, color, LV_PART_MAIN);
	lv_obj_set_style_text_letter_space(label, 0, LV_PART_MAIN);
	lv_obj_set_style_text_line_space(label, 0, LV_PART_MAIN);
	lv_obj_set_style_bg_opa(label, LV_OPA_TRANSP, LV_PART_MAIN);
	lv_obj_set_pos(label, x, y);
	lv_obj_set_width(label, w);
	lv_label_set_long_mode(label, LV_LABEL_LONG_DOT);
	return label;
}

static lv_obj_t *make_box(lv_obj_t *parent, lv_coord_t x, lv_coord_t y,
			  lv_coord_t w, lv_coord_t h, lv_color_t color)
{
	lv_obj_t *box = lv_obj_create(parent);

	lv_obj_set_pos(box, x, y);
	lv_obj_set_size(box, w, h);
	lv_obj_set_style_bg_color(box, color, LV_PART_MAIN);
	lv_obj_set_style_bg_opa(box, LV_OPA_COVER, LV_PART_MAIN);
	lv_obj_set_style_border_width(box, 0, LV_PART_MAIN);
	lv_obj_set_style_radius(box, 3, LV_PART_MAIN);
	lv_obj_set_style_pad_all(box, 0, LV_PART_MAIN);
	lv_obj_clear_flag(box, LV_OBJ_FLAG_SCROLLABLE);
	return box;
}

static lv_obj_t *make_dot(lv_obj_t *parent, lv_coord_t x, lv_coord_t y, lv_color_t color)
{
	lv_obj_t *dot = make_box(parent, x, y, 7, 7, color);

	lv_obj_set_style_radius(dot, LV_RADIUS_CIRCLE, LV_PART_MAIN);
	return dot;
}

static void build_home(lv_obj_t *parent)
{
	lv_obj_t *scan;
	lv_obj_t *bar_bg;

	make_box(parent, 0, 0, 4, STATUS_SCREEN_H, COLOR_CYAN);
	scan = make_box(parent, 8, 29, 304, 1, COLOR_DIVIDER);
	lv_obj_set_style_bg_opa(scan, LV_OPA_70, LV_PART_MAIN);
	make_box(parent, 8, 132, 304, 1, COLOR_DIVIDER);

	home.mode_chip = make_label(parent, "USB", COLOR_TEXT, 14, 8, 42);
	make_label(parent, "Signal Neon", COLOR_TEXT_MUTED, 59, 8, 96);
	home.battery_label = make_label(parent, "100%", COLOR_TEXT_MUTED, 262, 8, 44);
	home.charging_dot = make_dot(parent, 248, 13, COLOR_GREEN);

	home.title = make_label(parent, "USB Ready", COLOR_TEXT, 14, 42, 196);
	lv_obj_set_style_text_font(home.title, &lv_font_montserrat_14, LV_PART_MAIN);
	home.subtitle = make_label(parent, "Local status and settings", COLOR_TEXT_MUTED,
				   16, 68, 202);

	bar_bg = make_box(parent, 230, 46, 70, 8, COLOR_ELEVATED);
	home.battery_bar = make_box(bar_bg, 0, 0, 70, 8, COLOR_GREEN);
	home.rgb_accent = make_box(parent, 230, 65, 70, 6, COLOR_MAGENTA);

	home.num_status = make_label(parent, "NUM Host", COLOR_TEXT, 16, 92, 86);
	home.rgb_status = make_label(parent, "RGB Reactive", COLOR_TEXT, 111, 92, 96);
	home.knob_status = make_label(parent, "Knob Vol-/Vol+/Mute", COLOR_TEXT_MUTED,
				      16, 112, 220);

	make_label(parent, "Hold: Settings", COLOR_CYAN, 186, 145, 116);
}

static lv_obj_t *build_template_page(lv_obj_t *parent, const char *title)
{
	lv_obj_t *page = make_box(parent, 0, 0, STATUS_SCREEN_W, STATUS_SCREEN_H, COLOR_BG);

	lv_obj_add_flag(page, LV_OBJ_FLAG_HIDDEN);
	make_box(page, 0, 0, 4, STATUS_SCREEN_H, COLOR_MAGENTA);
	make_label(page, title, COLOR_TEXT, 14, 8, 160);
	make_label(page, "Exit", COLOR_CYAN, 264, 8, 40);
	make_box(page, 8, 29, 304, 1, COLOR_DIVIDER);
	return page;
}

static void add_menu_row(lv_obj_t *page, uint8_t row, const char *label,
			 const char *value, bool focused)
{
	lv_coord_t y = 42 + (row * 24);
	lv_obj_t *focus = make_box(page, 12, y - 2, 4, 18, focused ? COLOR_CYAN : COLOR_DIVIDER);

	lv_obj_set_style_bg_opa(focus, focused ? LV_OPA_COVER : LV_OPA_50, LV_PART_MAIN);
	make_label(page, label, focused ? COLOR_TEXT : COLOR_TEXT_MUTED, 24, y, 96);
	make_label(page, value, focused ? COLOR_CYAN : COLOR_TEXT_MUTED, 146, y, 144);
}

static void build_settings_template(lv_obj_t *parent)
{
	settings_page = build_template_page(parent, "Settings");
	add_menu_row(settings_page, 0, "Mode", "Follow Hardware", true);
	add_menu_row(settings_page, 1, "RGB", "Brightness / Mode", false);
	add_menu_row(settings_page, 2, "System", "Save / Reset", false);
	add_menu_row(settings_page, 3, "Exit", "Back Home", false);
}

static void build_edit_template(lv_obj_t *parent)
{
	lv_obj_t *bar_bg;
	lv_obj_t *bar;

	edit_page = build_template_page(parent, "Edit");
	make_label(edit_page, "RGB Brightness", COLOR_TEXT, 18, 46, 160);
	make_label(edit_page, "Rotate to adjust, press to apply", COLOR_TEXT_MUTED,
		   18, 70, 230);
	bar_bg = make_box(edit_page, 20, 103, 196, 10, COLOR_ELEVATED);
	bar = make_box(bar_bg, 0, 0, 120, 10, COLOR_CYAN);
	lv_obj_set_style_bg_color(bar, COLOR_GREEN, LV_PART_MAIN);
	make_label(edit_page, "Exit", COLOR_CYAN, 230, 99, 52);
}

static void build_confirm_template(lv_obj_t *parent)
{
	confirm_page = build_template_page(parent, "Confirm");
	make_label(confirm_page, "Factory Reset", COLOR_RED, 18, 50, 160);
	make_label(confirm_page, "Default focus stays on Cancel.", COLOR_TEXT_MUTED,
		   18, 76, 236);
	add_menu_row(confirm_page, 2, "Cancel", "Selected", true);
	add_menu_row(confirm_page, 3, "Confirm", "Hold intent", false);
}

int status_screen_lvgl_init(const struct status_screen_snapshot *snapshot)
{
	if (snapshot == NULL) {
		return -EINVAL;
	}

	root = lv_screen_active();
	if (root == NULL) {
		return -ENODEV;
	}

	lv_obj_clean(root);
	lv_obj_set_size(root, STATUS_SCREEN_W, STATUS_SCREEN_H);
	lv_obj_set_style_bg_color(root, COLOR_BG, LV_PART_MAIN);
	lv_obj_set_style_bg_opa(root, LV_OPA_COVER, LV_PART_MAIN);
	lv_obj_clear_flag(root, LV_OBJ_FLAG_SCROLLABLE);

	build_home(root);
	build_settings_template(root);
	build_edit_template(root);
	build_confirm_template(root);
	status_screen_lvgl_update(snapshot);
	return 0;
}

void status_screen_lvgl_update(const struct status_screen_snapshot *snapshot)
{
	char battery[8];
	char knob[48];
	uint8_t percent;
	lv_coord_t bar_width;

	if (snapshot == NULL || root == NULL) {
		return;
	}

	percent = battery_percent(snapshot);
	if (percent > 100U) {
		percent = 100U;
	}

	(void)snprintf(battery, sizeof(battery), "%u%%", percent);
	(void)snprintf(knob, sizeof(knob), "Knob %s/%s/%s",
		       encoder_action_text(snapshot->config.encoder_ccw_action),
		       encoder_action_text(snapshot->config.encoder_cw_action),
		       encoder_action_text(snapshot->config.encoder_press_action));

	lv_label_set_text(home.mode_chip, mode_text(snapshot->mode));
	lv_obj_set_style_text_color(home.mode_chip, mode_color(snapshot->mode), LV_PART_MAIN);
	lv_label_set_text(home.title, snapshot->message != NULL ? snapshot->message : "Ready");
	lv_label_set_text(home.subtitle,
			  snapshot->power.usb_present ? "USB power present" : "Battery power");
	lv_label_set_text(home.battery_label, battery);
	lv_label_set_text(home.num_status, snapshot->num_lock ? "NUM On" : "NUM Host");
	lv_label_set_text(home.rgb_status,
			  rgb_mode_text(snapshot->config.rgb_mode, snapshot->config.rgb_enable));
	lv_label_set_text(home.knob_status, knob);

	bar_width = (lv_coord_t)((70U * percent) / 100U);
	if (bar_width < 3) {
		bar_width = 3;
	}
	lv_obj_set_width(home.battery_bar, bar_width);
	lv_obj_set_style_bg_color(home.battery_bar,
				  percent <= 15U ? COLOR_RED :
				  (percent <= 35U ? COLOR_AMBER : COLOR_GREEN),
				  LV_PART_MAIN);
	lv_obj_set_style_bg_color(home.charging_dot,
				  snapshot->power.charging_state == POWER_CHARGING_STATE_CHARGING ?
				  COLOR_GREEN : COLOR_DIVIDER,
				  LV_PART_MAIN);
	lv_obj_set_style_bg_color(home.rgb_accent,
				  snapshot->config.rgb_enable ? COLOR_MAGENTA : COLOR_DIVIDER,
				  LV_PART_MAIN);
}
