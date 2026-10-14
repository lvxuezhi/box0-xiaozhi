#ifndef PC_MONITOR_PAGE_H
#define PC_MONITOR_PAGE_H

#include <lvgl.h>
#include <cJSON.h>

// Full-screen LVGL page that renders PC hardware stats received over MQTT.
// All methods must be called from the main application task (via
// Application::Schedule) so LVGL access stays on one thread.
class PcMonitorPage {
public:
    PcMonitorPage() = default;
    ~PcMonitorPage();

    void Create(lv_obj_t* screen, int width, int height);
    void UpdateStats(const cJSON* root);

    void Show();
    void Hide();
    bool IsVisible() const { return visible_; }

private:
    static constexpr int kRows = 4;  // CPU, MEM, GPU, DISK

    bool created_ = false;
    bool visible_ = false;
    lv_obj_t* page_ = nullptr;
    lv_obj_t* bar_[kRows] = {};
    lv_obj_t* value_label_[kRows] = {};
    lv_obj_t* extra_label_[kRows] = {};
    lv_obj_t* bottom_label_ = nullptr;
};

#endif  // PC_MONITOR_PAGE_H
