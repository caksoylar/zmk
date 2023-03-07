#include <zmk/display/status_screen.h>
#include <lvgl.h>

#include <zmk/display.h>
#include <zmk/event_manager.h>
#include <zmk/usb.h>
#include <zmk/ble.h>
#include <zmk/endpoints.h>
#include <zephyr/bluetooth/services/bas.h>
#include <zmk/events/battery_state_changed.h>

#if IS_ENABLED(CONFIG_ZMK_SPLIT_ROLE_CENTRAL)
#include <zmk/events/usb_conn_state_changed.h>
#include <zmk/events/ble_active_profile_changed.h>
#include <zmk/events/endpoint_changed.h>
#else
#include <zmk/split/bluetooth/peripheral.h>
#include <zmk/events/split_peripheral_status_changed.h>
#endif

#include <zephyr/logging/log.h>

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

LV_IMG_DECLARE(jack_masked);
LV_IMG_DECLARE(queen_masked);
LV_IMG_DECLARE(spade);
LV_IMG_DECLARE(heart);
LV_IMG_DECLARE(diamond);
LV_IMG_DECLARE(club);
LV_IMG_DECLARE(club_r);
LV_IMG_DECLARE(usb);
LV_IMG_DECLARE(usb_r);
LV_IMG_DECLARE(excl);
LV_IMG_DECLARE(excl_r);
LV_IMG_DECLARE(question);
LV_IMG_DECLARE(question_r);
LV_IMG_DECLARE(j_logo);
LV_IMG_DECLARE(j_logo_r);
LV_IMG_DECLARE(q_logo);
LV_IMG_DECLARE(q_logo_r);

static lv_style_t line_style;
static lv_obj_t *suit_tl, *suit_br, *rank_tl, *rank_br;
static lv_obj_t *battery_line;
static lv_point_t battery_pts[2] = {{2, 1}, {2, 1}};

#if IS_ENABLED(CONFIG_ZMK_SPLIT_ROLE_CENTRAL)
struct output_status_state {
    enum zmk_transport selected_transport;
    bool active_profile_connected;
    bool active_profile_bonded;
    uint8_t active_profile_index;
};

static struct output_status_state get_state(const zmk_event_t *_eh) {
    return (struct output_status_state){.selected_transport = zmk_endpoints_selected().transport,
                                        .active_profile_connected =
                                            zmk_ble_active_profile_is_connected(),
                                        .active_profile_bonded = !zmk_ble_active_profile_is_open(),
                                        .active_profile_index = zmk_ble_active_profile_index()};
    ;
}

static void output_status_update_cb(struct output_status_state state) {
    static bool first_run = true;
    static struct output_status_state prev_state =
        (struct output_status_state){.selected_transport = 0,
                                     .active_profile_connected = false,
                                     .active_profile_bonded = false,
                                     .active_profile_index = 0};

    if (first_run || state.active_profile_index != prev_state.active_profile_index) {
        const lv_img_dsc_t *profile_suit, *profile_suit_r;
        switch (state.active_profile_index) {
        case 1:
            profile_suit = &heart;
            profile_suit_r = &spade;
            break;
        case 2:
            profile_suit = &diamond;
            profile_suit_r = &diamond;
            break;
        case 3:
            profile_suit = &club;
            profile_suit_r = &club_r;
            break;
        default:
            profile_suit = &spade;
            profile_suit_r = &heart;
            break;
        }
        lv_img_set_src(suit_tl, profile_suit);
#if !IS_ENABLED(CONFIG_ZEN_STATUS_SCREEN_8BIT_HALF)
        lv_img_set_src(suit_br, profile_suit_r);
#endif
    }
#if IS_ENABLED(CONFIG_ZEN_STATUS_SCREEN_8BIT_HALF)
    if (first_run) {
        lv_img_set_src(suit_br, &heart);
    }
#endif

    switch (state.selected_transport) {
    case ZMK_TRANSPORT_USB:
        if (first_run || prev_state.selected_transport != ZMK_TRANSPORT_USB) {
            lv_img_set_src(rank_tl, &usb);
#if !IS_ENABLED(CONFIG_ZEN_STATUS_SCREEN_8BIT_HALF)
            lv_img_set_src(rank_br, &usb_r);
#endif
        }
        break;
    case ZMK_TRANSPORT_BLE:
        if (first_run || prev_state.selected_transport != ZMK_TRANSPORT_BLE ||
            state.active_profile_connected != prev_state.active_profile_connected ||
            state.active_profile_bonded != prev_state.active_profile_bonded) {
            if (state.active_profile_bonded && state.active_profile_connected) {
                lv_img_set_src(rank_tl, &j_logo);
#if !IS_ENABLED(CONFIG_ZEN_STATUS_SCREEN_8BIT_HALF)
                lv_img_set_src(rank_br, &j_logo_r);
#endif
            } else if (state.active_profile_bonded) {
                lv_img_set_src(rank_tl, &excl);
#if !IS_ENABLED(CONFIG_ZEN_STATUS_SCREEN_8BIT_HALF)
                lv_img_set_src(rank_br, &excl_r);
#endif
            } else {
                lv_img_set_src(rank_tl, &question);
#if !IS_ENABLED(CONFIG_ZEN_STATUS_SCREEN_8BIT_HALF)
                lv_img_set_src(rank_br, &question_r);
#endif
            }
        }
    }
#if IS_ENABLED(CONFIG_ZEN_STATUS_SCREEN_8BIT_HALF)
    if (first_run) {
        lv_img_set_src(rank_br, &j_logo_r);
    }
#endif
    first_run = false;
    prev_state = state;
}

ZMK_DISPLAY_WIDGET_LISTENER(widget_output_status, struct output_status_state,
                            output_status_update_cb, get_state)
ZMK_SUBSCRIPTION(widget_output_status, zmk_endpoint_changed);
ZMK_SUBSCRIPTION(widget_output_status, zmk_usb_conn_state_changed);
ZMK_SUBSCRIPTION(widget_output_status, zmk_ble_active_profile_changed);
#else // peripheral
struct peripheral_status_state {
    bool connected;
};

static struct peripheral_status_state get_state(const zmk_event_t *_eh) {
    return (struct peripheral_status_state){.connected = zmk_split_bt_peripheral_is_connected()};
}

static void peripheral_status_update_cb(struct peripheral_status_state state) {
    if (state.connected) {
        lv_img_set_src(rank_tl, &q_logo);
#if !IS_ENABLED(CONFIG_ZEN_STATUS_SCREEN_8BIT_HALF)
        lv_img_set_src(rank_br, &q_logo_r);
#endif
    } else {
        lv_img_set_src(rank_tl, &excl);
#if !IS_ENABLED(CONFIG_ZEN_STATUS_SCREEN_8BIT_HALF)
        lv_img_set_src(rank_br, &excl_r);
#endif
    }
}

ZMK_DISPLAY_WIDGET_LISTENER(widget_peripheral_status, struct peripheral_status_state,
                            peripheral_status_update_cb, get_state)
ZMK_SUBSCRIPTION(widget_peripheral_status, zmk_split_peripheral_status_changed);
#endif

struct battery_status_state {
    uint8_t level;
};

static struct battery_status_state battery_status_get_state(const zmk_event_t *eh) {
    return (struct battery_status_state){.level = bt_bas_get_battery_level()};
}

static void battery_status_update_cb(struct battery_status_state state) {
    static uint8_t prev_line_length = 0;
    uint8_t line_length = state.level * 3 / 4; // scales between 0 and 75
    if (line_length != prev_line_length) {
        battery_pts[1] = (lv_point_t){2 + line_length, 1};
        lv_line_set_points(battery_line, battery_pts, 2);
        prev_line_length = line_length;
    }
}

ZMK_DISPLAY_WIDGET_LISTENER(widget_battery_status, struct battery_status_state,
                            battery_status_update_cb, battery_status_get_state)

ZMK_SUBSCRIPTION(widget_battery_status, zmk_battery_state_changed);

lv_obj_t *zmk_display_status_screen() {
    lv_obj_t *screen = lv_obj_create(NULL);

    lv_obj_t *card_img = lv_img_create(screen);

    rank_tl = lv_img_create(screen);
    rank_br = lv_img_create(screen);
    lv_obj_align(rank_tl, LV_ALIGN_TOP_LEFT, 8, 12);
    lv_obj_align(rank_br, LV_ALIGN_TOP_LEFT, 62, 105);

    suit_tl = lv_img_create(screen);
    suit_br = lv_img_create(screen);
    lv_obj_align(suit_tl, LV_ALIGN_TOP_LEFT, 8, 27);
    lv_obj_align(suit_br, LV_ALIGN_TOP_LEFT, 62, 90);

#if IS_ENABLED(CONFIG_ZMK_SPLIT_ROLE_CENTRAL)
    lv_img_set_src(card_img, &jack_masked);

    widget_output_status_init();
#else
    lv_img_set_src(card_img, &queen_masked);
    lv_img_set_src(suit_tl, &spade);
    lv_img_set_src(suit_br, &heart);
#if IS_ENABLED(CONFIG_ZEN_STATUS_SCREEN_8BIT_HALF)
    lv_img_set_src(rank_br, &q_logo_r);
#endif

    widget_peripheral_status_init();
#endif

    lv_style_init(&line_style);
    lv_style_set_line_width(&line_style, 2);
    lv_style_set_line_color(&line_style, lv_color_black());

    battery_line = lv_line_create(screen);
    lv_obj_add_style(battery_line, &line_style, LV_PART_MAIN);
    lv_obj_align(battery_line, LV_ALIGN_TOP_LEFT, 0, 0);

    widget_battery_status_init();

    return screen;
}
