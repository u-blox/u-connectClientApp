#include "ucxapp.h"

#include "u_cx_bluetooth.h"
#include "u_cx_diagnostics.h"
#include "u_cx_gatt_client.h"
#include "u_cx_gatt_server.h"
#include "ucxapp_cmd.h"
#include "u_cx_http.h"
#include "u_cx_mqtt.h"
#include "u_cx_socket.h"
#include "u_cx_sps.h"
#include "u_cx_wifi.h"

/* Handlers only raise event flags; they are the minimum the command packs wait on. */

static void startupUrc(struct uCxHandle *h)                                            { (void)h; ucxapp_events_signal(URC_FLAG_STARTUP); }
static void networkUpUrc(struct uCxHandle *h)                                          { (void)h; ucxapp_events_signal(URC_FLAG_NETWORK_UP); }
static void networkDownUrc(struct uCxHandle *h)                                        { (void)h; ucxapp_events_signal(URC_FLAG_NETWORK_DOWN); }
static void linkUpUrc(struct uCxHandle *h, int32_t wlan, uMacAddress_t *bssid, int32_t ch)
{
    (void)h; (void)wlan; (void)bssid; (void)ch;
    ucxapp_events_signal(URC_FLAG_WIFI_LINK_UP);
}
static void linkDownUrc(struct uCxHandle *h, int32_t wlan, int32_t reason)
{
    (void)h; (void)wlan; (void)reason;
    ucxapp_events_signal(URC_FLAG_WIFI_LINK_DOWN);
}
static void sockConnectedUrc(struct uCxHandle *h, int32_t s)                           { (void)h; (void)s; ucxapp_events_signal(URC_FLAG_SOCK_CONNECTED); }
static void sockClosedUrc(struct uCxHandle *h, int32_t s)                              { (void)h; ucxapp_web_on_closed(s); ucxapp_events_signal(URC_FLAG_SOCK_CLOSED); }
static void sockDataUrc(struct uCxHandle *h, int32_t s, int32_t n)                     { (void)h; ucxapp_web_on_data(s, n); ucxapp_events_signal(URC_FLAG_SOCK_DATA); }
static void sockIncomingUrc(struct uCxHandle *h, int32_t s, uSockIpAddress_t *ip, int32_t listenSock) { (void)h; (void)ip; ucxapp_web_on_incoming(s, listenSock); ucxapp_sock_on_incoming(s, listenSock); }
static void spsConnectedUrc(struct uCxHandle *h, int32_t c)                            { (void)h; (void)c; ucxapp_events_signal(URC_FLAG_SPS_CONNECTED); }
static void spsDisconnectedUrc(struct uCxHandle *h, int32_t c)                         { (void)h; (void)c; ucxapp_events_signal(URC_FLAG_SPS_DISCONNECTED); }
static void spsDataUrc(struct uCxHandle *h, int32_t c, int32_t n)                      { (void)h; (void)c; (void)n; ucxapp_events_signal(URC_FLAG_SPS_DATA); }
static void httpStatusUrc(struct uCxHandle *h, int32_t id, int32_t code, const char *d){ (void)h; (void)id; (void)code; (void)d; ucxapp_events_signal(URC_FLAG_HTTP_RESPONSE_READY); }
static void httpDisconnectUrc(struct uCxHandle *h, int32_t id)                         { (void)h; (void)id; ucxapp_events_signal(URC_FLAG_HTTP_DISCONNECTED); }
static void mqttConnectedUrc(struct uCxHandle *h, int32_t id)                          { (void)h; (void)id; ucxapp_events_signal(URC_FLAG_MQTT_CONNECTED); }
static void mqttDataUrc(struct uCxHandle *h, int32_t id, int32_t n)                    { (void)h; (void)id; (void)n; ucxapp_events_signal(URC_FLAG_MQTT_DATA); }
static void mqttDisconnectedUrc(struct uCxHandle *h, int32_t id, int32_t reason)       { (void)h; (void)id; (void)reason; ucxapp_events_signal(URC_FLAG_MQTT_DISCONNECTED); }
static void mqttSubscribedUrc(struct uCxHandle *h, int32_t id, uMqttSubscribeAction_t a)  { (void)h; (void)id; (void)a; ucxapp_events_signal(URC_FLAG_MQTT_SUBSCRIBED); }
static void btConnectedUrc(struct uCxHandle *h, int32_t c, uBtLeAddress_t *a)          { (void)h; (void)c; (void)a; ucxapp_events_signal(URC_FLAG_BT_CONNECTED); }
static void btDisconnectedUrc(struct uCxHandle *h, int32_t c)                          { (void)h; (void)c; ucxapp_events_signal(URC_FLAG_BT_DISCONNECTED); }

static void btBondUrc(struct uCxHandle *h, uBtLeAddress_t *a, uBtBondStatus_t s)       { (void)h; ucxapp_bt_bond_status(a, (int32_t)s); }
static void btConfirmUrc(struct uCxHandle *h, uBtLeAddress_t *a, int32_t v)            { (void)h; ucxapp_bt_user_confirmation(h, a, v); }
static void btPasskeyShowUrc(struct uCxHandle *h, uBtLeAddress_t *a, int32_t v)        { (void)h; ucxapp_bt_passkey_display(a, v); }
static void btPasskeyAskUrc(struct uCxHandle *h, uBtLeAddress_t *a)                    { (void)h; ucxapp_bt_passkey_request(a); }

static void gattNotifyUrc(struct uCxHandle *h, int32_t conn, int32_t handle, uByteArray_t *data)
{
    (void)h;
    ucxapp_gatt_notification(conn, handle, data->pData, data->length);
}

static void gattWriteUrc(struct uCxHandle *h, int32_t conn, int32_t handle, uByteArray_t *data, uGattServerOptions_t options)
{
    (void)h; (void)options;
    ucxapp_gatt_server_write(conn, handle, data->pData, data->length);
}

static void gattReadUrc(struct uCxHandle *h, int32_t conn, int32_t handle)
{
    (void)h;
    ucxapp_gatt_server_read(conn, handle);
}

static void pingCompleteUrc(struct uCxHandle *h, int32_t sent, int32_t received, int32_t lossRate, int32_t avgMs)
{
    (void)h;
    (void)lossRate;
    gUcxappPingSent = sent;
    gUcxappPingReceived = received;
    gUcxappPingAvgMs = avgMs;
    ucxapp_events_signal(URC_FLAG_PING_COMPLETE);
}

void ucxapp_urc_register(uCxHandle_t *ucx)
{
    ucx->callbacks.STARTUP = startupUrc;

    uCxWifiRegisterLinkUp(ucx, linkUpUrc);
    uCxWifiRegisterLinkDown(ucx, linkDownUrc);
    uCxWifiRegisterStationNetworkUp(ucx, networkUpUrc);
    uCxWifiRegisterStationNetworkDown(ucx, networkDownUrc);

    uCxSocketRegisterConnect(ucx, sockConnectedUrc);
    uCxSocketRegisterClosed(ucx, sockClosedUrc);
    uCxSocketRegisterDataAvailable(ucx, sockDataUrc);
    uCxSocketRegisterIncomingConnection(ucx, sockIncomingUrc);

    uCxSpsRegisterConnect(ucx, spsConnectedUrc);
    uCxSpsRegisterDisconnect(ucx, spsDisconnectedUrc);
    uCxSpsRegisterDataAvailable(ucx, spsDataUrc);

    uCxDiagnosticsRegisterPingComplete(ucx, pingCompleteUrc);

    uCxHttpRegisterRequestStatus(ucx, httpStatusUrc);
    uCxHttpRegisterDisconnect(ucx, httpDisconnectUrc);

    uCxMqttRegisterConnect(ucx, mqttConnectedUrc);
    uCxMqttRegisterDisconnect(ucx, mqttDisconnectedUrc);
    uCxMqttRegisterDataAvailable(ucx, mqttDataUrc);
    uCxMqttRegisterSubscribeCompleted(ucx, mqttSubscribedUrc);

    uCxBluetoothRegisterConnect(ucx, btConnectedUrc);
    uCxBluetoothRegisterDisconnect(ucx, btDisconnectedUrc);
    uCxBluetoothRegisterBondStatus(ucx, btBondUrc);
    uCxBluetoothRegisterUserConfirmation(ucx, btConfirmUrc);
    uCxBluetoothRegisterPasskeyEntry(ucx, btPasskeyShowUrc);
    uCxBluetoothRegisterPasskeyRequest(ucx, btPasskeyAskUrc);

    uCxGattClientRegisterNotification(ucx, gattNotifyUrc);
    uCxGattClientRegisterIndication(ucx, gattNotifyUrc);
    uCxGattServerRegisterNotification(ucx, gattWriteUrc);
    uCxGattServerRegisterReadAttribute(ucx, gattReadUrc);
}
