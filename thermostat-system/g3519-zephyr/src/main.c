/* Thermostat-shaped LVGL screen for a footprint count under Zephyr: title, presence badge, setpoint with plus and minus buttons. */
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/display.h>
#include <lvgl.h>

static lv_obj_t *set_lbl; static int setpoint = 215;
static void btn_cb(lv_event_t *e)
{
	setpoint += (int)(intptr_t)lv_event_get_user_data(e);
	lv_label_set_text_fmt(set_lbl, "%d.%d C", setpoint / 10, setpoint % 10);
}
int main(void)
{
	const struct device *d = DEVICE_DT_GET(DT_CHOSEN(zephyr_display));
	if (!device_is_ready(d)) return 0;
	lv_obj_t *s = lv_screen_active(), *o = lv_label_create(s);
	lv_obj_set_style_text_font(o, &lv_font_montserrat_20, 0);
	lv_label_set_text(o, "Thermostat"); lv_obj_align(o, LV_ALIGN_TOP_MID, 0, 6);
	o = lv_label_create(s); lv_label_set_text(o, "Room is empty"); lv_obj_set_pos(o, 14, 60);
	set_lbl = lv_label_create(s); lv_label_set_text(set_lbl, "21.5 C"); lv_obj_set_pos(set_lbl, 14, 120);
	for (int i = 0; i < 2; i++) {
		lv_obj_t *b = lv_button_create(s); lv_obj_set_size(b, 130, 76); lv_obj_set_pos(b, i ? 170 : 20, 200);
		lv_obj_add_event_cb(b, btn_cb, LV_EVENT_CLICKED, (void *)(intptr_t)(i ? 5 : -5));
		lv_obj_t *l = lv_label_create(b); lv_label_set_text(l, i ? "+" : "-"); lv_obj_center(l);
	}
	display_blanking_off(d);
	for (;;) { lv_timer_handler(); k_msleep(10); }
}
