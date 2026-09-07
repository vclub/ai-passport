#include "buddy_ble.h"

#include "esp_log.h"
#include "esp_mac.h"
#include "esp_timer.h"
#include "host/ble_att.h"
#include "host/ble_gap.h"
#include "host/ble_gatt.h"
#include "host/ble_hs.h"
#include "host/ble_hs_mbuf.h"
#include "host/util/util.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"

#include <stdio.h>
#include <string.h>

static const char *TAG = "buddy_ble";

static const ble_uuid128_t UUID_SVC = BLE_UUID128_INIT(
    0xfb, 0x34, 0x9b, 0x5f, 0x80, 0x00, 0x00, 0x80,
    0x00, 0x10, 0x00, 0x00, 0xef, 0xbe, 0x00, 0x00);
static const ble_uuid128_t UUID_WR = BLE_UUID128_INIT(
    0xfb, 0x34, 0x9b, 0x5f, 0x80, 0x00, 0x00, 0x80,
    0x00, 0x10, 0x01, 0x00, 0xef, 0xbe, 0x00, 0x00);
static const ble_uuid128_t UUID_NT = BLE_UUID128_INIT(
    0xfb, 0x34, 0x9b, 0x5f, 0x80, 0x00, 0x00, 0x80,
    0x00, 0x10, 0x02, 0x00, 0xef, 0xbe, 0x00, 0x00);

#define ADV_FAST_US      (30LL * 1000000)
#define ADV_SLOW_MIN     320
#define ADV_SLOW_MAX     480

static char s_name[16] = "Buddy";
static buddy_ble_write_cb_t s_on_write;
static buddy_ble_link_cb_t s_on_link;
static uint16_t s_conn = BLE_HS_CONN_HANDLE_NONE;
static uint16_t s_notify_handle;
static uint8_t s_own_addr;
static bool s_synced;
static bool s_fast_adv = true;
static esp_timer_handle_t s_adv_timer;

static int gap_event(struct ble_gap_event *event, void *arg);

static int wr_access(uint16_t conn, uint16_t attr, struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    (void)conn;
    (void)attr;
    (void)arg;
    if (ctxt->op != BLE_GATT_ACCESS_OP_WRITE_CHR) return 0;
    uint16_t n = OS_MBUF_PKTLEN(ctxt->om);
    if (n == 0 || n > 128) return 0;
    uint8_t buf[128];
    os_mbuf_copydata(ctxt->om, 0, n, buf);
    if (s_on_write) s_on_write(buf, n);
    return 0;
}

static int nt_access(uint16_t conn, uint16_t attr, struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    (void)conn;
    (void)attr;
    (void)arg;
    uint8_t z = 0;
    if (ctxt->op == BLE_GATT_ACCESS_OP_READ_CHR) {
        return os_mbuf_append(ctxt->om, &z, 1);
    }
    return 0;
}

static const struct ble_gatt_svc_def s_svcs[] = {
    {
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = &UUID_SVC.u,
        .characteristics = (struct ble_gatt_chr_def[]) {
            {
                .uuid = &UUID_WR.u,
                .access_cb = wr_access,
                .flags = BLE_GATT_CHR_F_WRITE | BLE_GATT_CHR_F_WRITE_NO_RSP,
            },
            {
                .uuid = &UUID_NT.u,
                .access_cb = nt_access,
                .val_handle = &s_notify_handle,
                .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_NOTIFY,
            },
            { 0 },
        },
    },
    { 0 },
};

static void advertise(void)
{
    if (!s_synced) return;
    if (s_conn != BLE_HS_CONN_HANDLE_NONE) return;

    const char *name = s_name;
    size_t n = strlen(name);
    uint8_t adv[31];
    int i = 0;
    adv[i++] = 2;
    adv[i++] = 0x01;
    adv[i++] = 0x06;
    if (i + 2 + n > 31) n = (size_t)(31 - i - 2);
    adv[i++] = (uint8_t)(1 + n);
    adv[i++] = 0x09;
    memcpy(&adv[i], name, n);
    i += (int)n;
    ble_gap_adv_set_data(adv, i);

    uint8_t rsp[31];
    int j = 0;
    rsp[j++] = 17;
    rsp[j++] = 0x07;
    memcpy(&rsp[j], UUID_SVC.value, 16);
    j += 16;
    ble_gap_adv_rsp_set_data(rsp, j);

    struct ble_gap_adv_params p;
    memset(&p, 0, sizeof(p));
    p.conn_mode = BLE_GAP_CONN_MODE_UND;
    p.disc_mode = BLE_GAP_DISC_MODE_GEN;
    p.itvl_min = s_fast_adv ? BLE_GAP_ADV_FAST_INTERVAL1_MIN : ADV_SLOW_MIN;
    p.itvl_max = s_fast_adv ? BLE_GAP_ADV_FAST_INTERVAL1_MAX : ADV_SLOW_MAX;
    int rc = ble_gap_adv_start(s_own_addr, NULL, BLE_HS_FOREVER, &p, gap_event, NULL);
    if (rc != 0 && rc != BLE_HS_EALREADY) {
        ESP_LOGW(TAG, "adv start rc=%d", rc);
    }
}

static void arm_fast_adv(void)
{
    s_fast_adv = true;
    if (s_adv_timer) {
        esp_timer_stop(s_adv_timer);
        esp_timer_start_once(s_adv_timer, ADV_FAST_US);
    }
}

static void adv_slow_cb(void *arg)
{
    (void)arg;
    if (!s_fast_adv) return;
    s_fast_adv = false;
    if (s_conn != BLE_HS_CONN_HANDLE_NONE) return;
    if (ble_gap_adv_active()) ble_gap_adv_stop();
    advertise();
}

static int gap_event(struct ble_gap_event *event, void *arg)
{
    (void)arg;
    switch (event->type) {
    case BLE_GAP_EVENT_CONNECT:
        if (event->connect.status == 0) {
            s_conn = event->connect.conn_handle;
            if (s_adv_timer) esp_timer_stop(s_adv_timer);
            ESP_LOGI(TAG, "connected conn=%u", s_conn);
            if (s_on_link) s_on_link(true);
        } else {
            s_conn = BLE_HS_CONN_HANDLE_NONE;
            arm_fast_adv();
            advertise();
        }
        return 0;
    case BLE_GAP_EVENT_DISCONNECT:
        ESP_LOGI(TAG, "disconnected reason=%d", event->disconnect.reason);
        s_conn = BLE_HS_CONN_HANDLE_NONE;
        if (s_on_link) s_on_link(false);
        arm_fast_adv();
        advertise();
        return 0;
    case BLE_GAP_EVENT_ADV_COMPLETE:
        advertise();
        return 0;
    default:
        return 0;
    }
}

static void on_sync(void)
{
    s_synced = true;
    ble_hs_util_ensure_addr(0);
    ble_hs_id_infer_auto(0, &s_own_addr);
    ble_svc_gap_device_name_set(s_name);
    ESP_LOGI(TAG, "advertising as %s", s_name);
    arm_fast_adv();
    advertise();
}

static void on_reset(int reason)
{
    bool was = s_conn != BLE_HS_CONN_HANDLE_NONE;
    s_synced = false;
    s_conn = BLE_HS_CONN_HANDLE_NONE;
    if (was && s_on_link) s_on_link(false);
    ESP_LOGW(TAG, "reset %d", reason);
}

static void host_task(void *param)
{
    (void)param;
    nimble_port_run();
    nimble_port_freertos_deinit();
}

esp_err_t buddy_ble_init(buddy_ble_write_cb_t on_write)
{
    s_on_write = on_write;
    uint8_t mac[6] = { 0 };
    if (esp_read_mac(mac, ESP_MAC_WIFI_STA) != ESP_OK) {
        esp_read_mac(mac, ESP_MAC_BT);
    }
    snprintf(s_name, sizeof(s_name), "Buddy-%02X%02X%02X", mac[3], mac[4], mac[5]);

    esp_err_t e = nimble_port_init();
    if (e != ESP_OK) {
        ESP_LOGE(TAG, "nimble_port_init %s", esp_err_to_name(e));
        return e;
    }

    ble_hs_cfg.reset_cb = on_reset;
    ble_hs_cfg.sync_cb = on_sync;
    ble_hs_cfg.sm_io_cap = BLE_HS_IO_NO_INPUT_OUTPUT;
    ble_hs_cfg.sm_bonding = 0;
    ble_hs_cfg.sm_mitm = 0;
    ble_hs_cfg.sm_sc = 0;

    ble_svc_gap_init();
    ble_svc_gatt_init();
    int rc = ble_gatts_count_cfg(s_svcs);
    if (rc == 0) rc = ble_gatts_add_svcs(s_svcs);
    if (rc != 0) {
        ESP_LOGE(TAG, "gatt rc=%d", rc);
        return ESP_FAIL;
    }
    ble_att_set_preferred_mtu(72);
    ble_svc_gap_device_name_set(s_name);

    if (!s_adv_timer) {
        const esp_timer_create_args_t ta = {
            .callback = adv_slow_cb,
            .name = "buddy_adv",
        };
        esp_timer_create(&ta, &s_adv_timer);
    }

    nimble_port_freertos_init(host_task);
    ESP_LOGI(TAG, "init name=%s", s_name);
    return ESP_OK;
}

void buddy_ble_set_link_cb(buddy_ble_link_cb_t cb)
{
    s_on_link = cb;
}

bool buddy_ble_ready(void)
{
    return s_synced;
}

const char *buddy_ble_name(void)
{
    return s_name;
}

bool buddy_ble_connected(void)
{
    return s_conn != BLE_HS_CONN_HANDLE_NONE;
}

void buddy_ble_notify(uint8_t byte)
{
    if (s_conn == BLE_HS_CONN_HANDLE_NONE || s_notify_handle == 0) return;
    struct os_mbuf *om = ble_hs_mbuf_from_flat(&byte, 1);
    if (!om) return;
    int rc = ble_gatts_notify_custom(s_conn, s_notify_handle, om);
    if (rc != 0) ESP_LOGW(TAG, "notify rc=%d", rc);
}

void buddy_ble_disconnect(void)
{
    if (s_conn == BLE_HS_CONN_HANDLE_NONE) return;
    ble_gap_terminate(s_conn, BLE_ERR_REM_USER_CONN_TERM);
}
