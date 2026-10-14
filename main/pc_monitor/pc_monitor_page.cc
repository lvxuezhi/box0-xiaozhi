#include "pc_monitor_page.h"
#include <esp_log.h>
#include <lvgl.h>
#include <esp_lvgl_port.h>
#include <string.h>
#include <stdio.h>

#define TAG "PcMonitorPage"

// Bar colours for 0–100% fill
static const lv_color_t kBarColor = LV_COLOR_MAKE(0x00, 0xFF, 0xFF);  // cyan
static const lv_color_t kBarWarn  = LV_COLOR_MAKE(0xFF, 0x66, 0x00);  // orange
static const lv_color_t kBarDanger= LV_COLOR_MAKE(0xFF, 0x00, 0x00);  // red

// Row definitions: key name in JSON, warning threshold, danger threshold
struct RowDef {
    const char* key;
    float warn_pct;
    float danger_pct;
};

static constexpr RowDef kRowDefs[] = {
    {"cpu",  75.0f, 95.0f},
    {"mem",  80.0f, 95.0f},
    {"gpu",  70.0f, 90.0f},
    {"disk", 85.0f, 95.0f},
};

PcMonitorPage::~PcMonitorPage() {
    if (page_ && created_) {
        lv_obj_del(page_);
    }
}

void PcMonitorPage::Create(lv_obj_t* screen, int width, int height) {
    if (created_) return;

    page_ = lv_obj_create(screen);
    // Fill the screen; the parent's SetupUI may create a container so we
    // position absolutely on top of it.
    lv_obj_set_size(page_, width, height);
    lv_obj_set_pos(page_, 0, 0);
    lv_obj_clear_flag(page_, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(page_, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(page_, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(page_, 0, 0);
    lv_obj_set_style_pad_all(page_, 12, 0);
    lv_obj_set_style_pad_row(page_, 14, 0);
    lv_obj_set_flex_flow(page_, LV_FLEX_FLOW_COLUMN);
    lv_obj_align(page_, LV_ALIGN_TOP_LEFT, 0, 0);

    auto* text_font = lv_obj_get_style_text_font(screen, LV_PART_MAIN);
    auto text_h = text_font ? text_font->line_height : 24;

    // Header
    lv_obj_t* title = lv_label_create(page_);
    lv_label_set_text(title, "PC Monitor");
    lv_obj_set_style_text_font(title, text_font, 0);
    lv_obj_set_style_text_color(title, lv_color_white(), 0);
    lv_obj_set_style_margin_bottom(title, 6, 0);

    // Row label
    lv_obj_t* row_label = lv_label_create(page_);
    lv_label_set_text(row_label, "CPU | MEM | GPU | DISK");
    lv_obj_set_style_text_font(row_label, text_font, 0);
    lv_obj_set_style_text_color(row_label, lv_color_make(0x80, 0x80, 0x80), 0);
    lv_obj_set_style_margin_bottom(row_label, 4, 0);

    float row_h = text_h * 1.8f + 12;
    lv_coord_t bar_max_w = width - 24;  // minus pad

    for (int i = 0; i < kRows; i++) {
        // Row container: label | bar | value
        lv_obj_t* row = lv_obj_create(page_);
        lv_obj_set_size(row, LV_PCT(100), (lv_coord_t)row_h);
        lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(row, 0, 0);
        lv_obj_set_style_pad_all(row, 4, 0);

        // Name label
        lv_obj_t* name = lv_label_create(row);
        const char* names[] = {"CPU", "MEM", "GPU", "DISK"};
        lv_label_set_text(name, names[i]);
        lv_obj_set_style_text_font(name, text_font, 0);
        lv_obj_set_style_text_color(name, lv_color_white(), 0);
        lv_obj_set_width(name, 52);
        lv_obj_align(name, LV_ALIGN_LEFT_MID, 0, 0);

        // Progress bar
        lv_obj_t* bar = lv_bar_create(row);
        lv_bar_set_range(bar, 0, 100);
        lv_bar_set_value(bar, 0, LV_ANIM_OFF);
        lv_obj_set_size(bar, bar_max_w, (lv_coord_t)(row_h - 8));
        lv_obj_align_to(bar, name, LV_ALIGN_OUT_RIGHT_MID, 6, 0);
        lv_obj_set_style_bg_color(bar, lv_color_make(0x33, 0x33, 0x33), 0);
        lv_obj_set_style_radius(bar, 4, 0);
        lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, 0);
        // Indicator
        lv_obj_t* indic = lv_obj_get_child(bar, 0);
        if (indic) {
            lv_obj_set_style_bg_color(indic, kBarColor, 0);
            lv_obj_set_style_radius(indic, 4, 0);
        }
        bar_[i] = bar;

        // Value label
        lv_obj_t* val = lv_label_create(row);
        lv_label_set_text(val, "--%");
        lv_obj_set_style_text_font(val, text_font, 0);
        lv_obj_set_style_text_color(val, lv_color_white(), 0);
        lv_obj_align(val, LV_ALIGN_RIGHT_MID, -2, 0);
        value_label_[i] = val;

        // Extra info label (temp, fan, etc.)
        lv_obj_t* extra = lv_label_create(row);
        lv_label_set_text(extra, "");
        lv_obj_set_width(extra, LV_SIZE_CONTENT);
        lv_obj_set_style_text_font(extra, text_font, 0);
        lv_obj_set_style_text_color(extra, lv_color_make(0x80, 0x80, 0x80), 0);
        lv_obj_align_to(extra, val, LV_ALIGN_OUT_BOTTOM_LEFT, 0, 2);
        extra_label_[i] = extra;
    }

    // Bottom status label
    bottom_label_ = lv_label_create(page_);
    lv_label_set_text(bottom_label_, "Waiting for PC data...");
    lv_obj_set_style_text_font(bottom_label_, text_font, 0);
    lv_obj_set_style_text_color(bottom_label_, lv_color_make(0x60, 0x60, 0x60), 0);
    lv_obj_align(bottom_label_, LV_ALIGN_BOTTOM_MID, 0, -4);

    // Touch the page to go back to chat
    lv_obj_add_event_cb(page_, [](lv_event_t* e) {
        auto* self = (PcMonitorPage*)lv_event_get_user_data(e);
        if (self && self->visible_) {
            self->Hide();
        }
    }, LV_EVENT_CLICKED, this);

    created_ = true;
    visible_ = false;
}

void PcMonitorPage::UpdateStats(const cJSON* root) {
    if (!created_ || !root) return;
    // Called from Application::Schedule (main task), use LVGL lock directly.
    lvgl_port_lock(0);

    char buf[128];
    for (int i = 0; i < kRows; i++) {
        float pct = 0.0f;
        cJSON* v = cJSON_GetObjectItemCaseSensitive(root, kRowDefs[i].key);
        if (cJSON_IsNumber(v)) pct = (float)v->valuedouble;

        int bar_val = (int)(pct + 0.5f);
        if (bar_val < 0) bar_val = 0;
        if (bar_val > 100) bar_val = 100;
        lv_bar_set_value(bar_[i], bar_val, LV_ANIM_OFF);

        // Colour by severity
        lv_obj_t* indic = lv_obj_get_child(bar_[i], 0);
        lv_color_t col = kBarColor;
        if (pct >= kRowDefs[i].danger_pct) col = kBarDanger;
        else if (pct >= kRowDefs[i].warn_pct) col = kBarWarn;
        if (indic) lv_obj_set_style_bg_color(indic, col, 0);

        snprintf(buf, sizeof(buf), "%d%%", bar_val);
        lv_label_set_text(value_label_[i], buf);

        // Extra row-specific info — keep in a separate buffer to avoid
        // aliasing the main `buf` which confuses the compiler's truncation check
        char extra_buf[64];
        const char* extra = "";
        if (strcmp(kRowDefs[i].key, "cpu") == 0) {
            cJSON* t = cJSON_GetObjectItemCaseSensitive(root, "cpu_temp");
            if (cJSON_IsNumber(t)) { snprintf(extra_buf, sizeof(extra_buf), "%g°C", t->valuedouble); extra = extra_buf; }
        } else if (strcmp(kRowDefs[i].key, "gpu") == 0) {
            cJSON* t = cJSON_GetObjectItemCaseSensitive(root, "gpu_temp");
            if (cJSON_IsNumber(t)) { snprintf(extra_buf, sizeof(extra_buf), "%g°C", t->valuedouble); extra = extra_buf; }
            cJSON* vr = cJSON_GetObjectItemCaseSensitive(root, "vram");
            if (cJSON_IsNumber(vr)) { snprintf(extra_buf, sizeof(extra_buf), "%d MB", (int)vr->valuedouble); extra = extra_buf; }
        } else if (strcmp(kRowDefs[i].key, "mem") == 0) {
            cJSON* u = cJSON_GetObjectItemCaseSensitive(root, "mem_used_mb");
            if (cJSON_IsNumber(u)) { snprintf(extra_buf, sizeof(extra_buf), "%lld MB", (long long)u->valuedouble); extra = extra_buf; }
        } else if (strcmp(kRowDefs[i].key, "disk") == 0) {
            snprintf(extra_buf, sizeof(extra_buf), "%d%% full", bar_val);
            extra = extra_buf;
        }
        if (extra_label_[i]) lv_label_set_text(extra_label_[i], extra);
    }

    // Bottom label: show network speeds
    cJSON* up = cJSON_GetObjectItemCaseSensitive(root, "net_up");
    cJSON* dn = cJSON_GetObjectItemCaseSensitive(root, "net_down");
    if (cJSON_IsNumber(up) && cJSON_IsNumber(dn)) {
        snprintf(buf, sizeof(buf), "↑ %.0f KB/s  ↓ %.0f KB/s",
                 up->valuedouble, dn->valuedouble);
        lv_label_set_text(bottom_label_, buf);
    }

    lvgl_port_unlock();
}

void PcMonitorPage::Show() {
    if (!created_) return;
    visible_ = true;
    // Bring to front
    lv_obj_move_foreground(page_);
    lv_obj_remove_flag(page_, LV_OBJ_FLAG_HIDDEN);
    if (bottom_label_) lv_label_set_text(bottom_label_, "Waiting for PC data...");
}

void PcMonitorPage::Hide() {
    if (!created_) return;
    visible_ = false;
    lv_obj_add_flag(page_, LV_OBJ_FLAG_HIDDEN);
}
