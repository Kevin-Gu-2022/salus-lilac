/*
 * Copyright (c) 2017 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(net_mqtt_publisher_sample, LOG_LEVEL_DBG);

#include <zephyr/kernel.h>
#include <zephyr/net/socket.h>
#include <zephyr/net/mqtt.h>
#include <zephyr/random/random.h>

#include <string.h>
#include <errno.h>

#include "config.h"
#include "net_sample_common.h"

#include <zephyr/net/net_if.h>       // For net_if_get_default, net_if_is_up, net_if_is_ready
#include <zephyr/net/wifi_mgmt.h>  // For Wi-Fi connection requests
#include <zephyr/net/net_event.h>  // For network management event callbacks

bool wifi_connected = false;

#define APP_BMEM
#define APP_DMEM

/* Buffers for MQTT client. */
static APP_BMEM uint8_t rx_buffer[APP_MQTT_BUFFER_SIZE];
static APP_BMEM uint8_t tx_buffer[APP_MQTT_BUFFER_SIZE];

/* The mqtt client struct */
static APP_BMEM struct mqtt_client client_ctx;

/* MQTT Broker details. */
static APP_BMEM struct sockaddr_storage broker;


static APP_BMEM struct pollfd fds[1];
static APP_BMEM int nfds;

static APP_BMEM bool connected;


#if defined(CONFIG_MQTT_LIB_TLS)

#include "test_certs.h"

#define TLS_SNI_HOSTNAME "localhost"
#define APP_CA_CERT_TAG 1
#define APP_PSK_TAG 2

static APP_DMEM sec_tag_t m_sec_tags[] = {
#if defined(MBEDTLS_X509_CRT_PARSE_C) || defined(CONFIG_NET_SOCKETS_OFFLOAD)
		APP_CA_CERT_TAG,
#endif
#if defined(MBEDTLS_KEY_EXCHANGE_SOME_PSK_ENABLED)
		APP_PSK_TAG,
#endif
};

static int tls_init(void)
{
	int err = -EINVAL;

#if defined(MBEDTLS_X509_CRT_PARSE_C) || defined(CONFIG_NET_SOCKETS_OFFLOAD)
	err = tls_credential_add(APP_CA_CERT_TAG, TLS_CREDENTIAL_CA_CERTIFICATE,
				 ca_certificate, sizeof(ca_certificate));
	if (err < 0) {
		LOG_ERR("Failed to register public certificate: %d", err);
		return err;
	}
#endif

#if defined(MBEDTLS_KEY_EXCHANGE_SOME_PSK_ENABLED)
	err = tls_credential_add(APP_PSK_TAG, TLS_CREDENTIAL_PSK,
				 client_psk, sizeof(client_psk));
	if (err < 0) {
		LOG_ERR("Failed to register PSK: %d", err);
		return err;
	}

	err = tls_credential_add(APP_PSK_TAG, TLS_CREDENTIAL_PSK_ID,
				 client_psk_id, sizeof(client_psk_id) - 1);
	if (err < 0) {
		LOG_ERR("Failed to register PSK ID: %d", err);
	}
#endif

	return err;
}

#endif /* CONFIG_MQTT_LIB_TLS */

static void prepare_fds(struct mqtt_client *client)
{
	if (client->transport.type == MQTT_TRANSPORT_NON_SECURE) {
		fds[0].fd = client->transport.tcp.sock;
	}
	#if defined(CONFIG_MQTT_LIB_TLS)
	else if (client->transport.type == MQTT_TRANSPORT_SECURE) {
		fds[0].fd = client->transport.tls.sock;
	}
	#endif

	fds[0].events = POLLIN;
	nfds = 1;
}

static void clear_fds(void)
{
	nfds = 0;
}

static int wait(int timeout)
{
	int ret = 0;

	if (nfds > 0) {
		ret = poll(fds, nfds, timeout);
		if (ret < 0) {
			LOG_ERR("poll error: %d", errno);
		}
	}

	return ret;
}

void mqtt_evt_handler(struct mqtt_client *const client,
		      const struct mqtt_evt *evt)
{
	// Log the event type first
    LOG_ERR("MQTT Event received: %s",
            evt->type == MQTT_EVT_CONNACK ? "CONNACK" :
            evt->type == MQTT_EVT_DISCONNECT ? "DISCONNECT" :
            evt->type == MQTT_EVT_PUBLISH ? "PUBLISH" :
            evt->type == MQTT_EVT_PUBACK ? "PUBACK" :
            evt->type == MQTT_EVT_SUBACK ? "SUBACK" :
            "OTHER_EVENT"); // Added this for clarity
	
	int err;

	switch (evt->type) {
	case MQTT_EVT_CONNACK:
		if (evt->result != 0) {
			LOG_ERR("MQTT connect failed %d", evt->result);
			break;
		}

		connected = true;
		LOG_INF("MQTT client connected!");

		break;

	case MQTT_EVT_DISCONNECT:
		LOG_INF("MQTT client disconnected %d", evt->result);

		connected = false;
		clear_fds();

		break;

	case MQTT_EVT_PUBACK:
		if (evt->result != 0) {
			LOG_ERR("MQTT PUBACK error %d", evt->result);
			break;
		}

		LOG_INF("PUBACK packet id: %u", evt->param.puback.message_id);

		break;

	case MQTT_EVT_PUBREC:
		if (evt->result != 0) {
			LOG_ERR("MQTT PUBREC error %d", evt->result);
			break;
		}

		LOG_INF("PUBREC packet id: %u", evt->param.pubrec.message_id);

		const struct mqtt_pubrel_param rel_param = {
			.message_id = evt->param.pubrec.message_id
		};

		err = mqtt_publish_qos2_release(client, &rel_param);
		if (err != 0) {
			LOG_ERR("Failed to send MQTT PUBREL: %d", err);
		}

		break;

	case MQTT_EVT_PUBCOMP:
		if (evt->result != 0) {
			LOG_ERR("MQTT PUBCOMP error %d", evt->result);
			break;
		}

		LOG_INF("PUBCOMP packet id: %u",
			evt->param.pubcomp.message_id);

		break;

	case MQTT_EVT_PINGRESP:
		LOG_INF("PINGRESP packet");
		break;

	case MQTT_EVT_PUBLISH: // Added for completeness, if you ever subscribe
        LOG_INF("Received PUBLISH message, ID: %d",
                evt->param.publish.message_id);
        break;

	default:
		break;
	}
}

static char *get_mqtt_payload(enum mqtt_qos qos)
{

	static APP_DMEM char payload[] = "DOORS:OPEN_QoSx";

	payload[strlen(payload) - 1] = '0' + qos;


	return payload;
}

static char *get_mqtt_topic(void)
{
	return "sensors";
}

static int publish(struct mqtt_client *client, enum mqtt_qos qos)
{
	struct mqtt_publish_param param;

	param.message.topic.qos = qos;
	param.message.topic.topic.utf8 = (uint8_t *)get_mqtt_topic();
	param.message.topic.topic.size =
			strlen(param.message.topic.topic.utf8);
	param.message.payload.data = get_mqtt_payload(qos);
	param.message.payload.len =
			strlen(param.message.payload.data);
	param.message_id = sys_rand16_get();
	param.dup_flag = 0U;
	param.retain_flag = 0U;

	LOG_INF("Publishing to topic: %s with payload: %s", get_mqtt_topic(), param.message.payload.data);

	return mqtt_publish(client, &param);
}

#define RC_STR(rc) ((rc) == 0 ? "OK" : "ERROR")

#define PRINT_RESULT(func, rc) \
	LOG_INF("%s: %d <%s>", (func), rc, RC_STR(rc))

static void broker_init(void)
{

	struct sockaddr_in *broker4 = (struct sockaddr_in *)&broker;

	broker4->sin_family = AF_INET;
	broker4->sin_port = htons(SERVER_PORT);
	inet_pton(AF_INET, SERVER_ADDR, &broker4->sin_addr);
}

char client_id_buf[24];

static struct mqtt_utf8 mqtt_password = {
    .utf8 = (const uint8_t *)"Csse4011",
    .size = sizeof("Csse4011") - 1 // -1 to not include the null terminator
};

static struct mqtt_utf8 mqtt_username = {
    .utf8 = (const uint8_t *)"Salus-Lilac",
    .size = sizeof("Salus-Lilac") - 1
};

static void client_init(struct mqtt_client *client)
{
	mqtt_client_init(client);

	broker_init();

	snprintf(client_id_buf, sizeof(client_id_buf), "zephyr_pub_%08x", sys_rand32_get());

	/* MQTT client configuration */
	client->broker = &broker;
	client->evt_cb = mqtt_evt_handler;
	client->client_id.utf8 = (const uint8_t *)client_id_buf;
	client->client_id.size = strlen(client_id_buf);
	// client->client_id.utf8 = (uint8_t *)MQTT_CLIENTID;
	// client->client_id.size = strlen(MQTT_CLIENTID);
	client->password = &mqtt_password;
	client->user_name = &mqtt_username;
	client->protocol_version = MQTT_VERSION_3_1_1;

	/* MQTT buffers configuration */
	client->rx_buf = rx_buffer;
	client->rx_buf_size = sizeof(rx_buffer);
	client->tx_buf = tx_buffer;
	client->tx_buf_size = sizeof(tx_buffer);

	client->clean_session = 1;
    client->keepalive = APP_KEEPALIVE_TIME_SECONDS;

	/* MQTT transport configuration */
	// client->transport.type = MQTT_TRANSPORT_NON_SECURE;

	#if defined(CONFIG_MQTT_LIB_TLS)
	client->transport.type = MQTT_TRANSPORT_SECURE;

	struct mqtt_sec_config *tls_config = &client->transport.tls.config;

	tls_config->peer_verify = TLS_PEER_VERIFY_REQUIRED;
	tls_config->cipher_list = NULL;
	tls_config->sec_tag_list = m_sec_tags;
	tls_config->sec_tag_count = ARRAY_SIZE(m_sec_tags);
	tls_config->hostname = SERVER_ADDR;
	#endif

}

/* In this routine we block until the connected variable is 1 */
static int try_to_connect(struct mqtt_client *client)
{
	int rc, i = 0;

	// LOG_ERR("HELLOOOO...");

	while (i++ < APP_CONNECT_TRIES && !connected) {

		client_init(client);

		rc = mqtt_connect(client);

		LOG_ERR("MQTT CONNECT...");

		if (rc != 0) {
			PRINT_RESULT("mqtt_connect", rc);
			k_sleep(K_MSEC(APP_SLEEP_MSECS));
			continue;
		}

		k_sleep(K_MSEC(1000));
		LOG_DBG("DEBUG: Short sleep after mqtt_connect sent. Connected flag value: %d", connected); // <-- ENSURE '%d', 'connected' IS HERE
		// LOG_ERR("MQTT CONNECT PAST...");

		prepare_fds(client);

		// LOG_ERR("MQTT PREPPED...");

		// if (wait(APP_CONNECT_TIMEOUT_MS)) {
		// 	mqtt_input(client);
		// 	LOG_ERR("MQTT WAIT...");
		// }

		int wait_attempts = 0;
        const int max_wait_attempts = APP_CONNECT_TIMEOUT_MS / 100; // Poll in smaller chunks, e.g., 100ms
        const int short_poll_timeout = 1000;

		while (!connected && wait_attempts < max_wait_attempts) {
			if (client->transport.type == MQTT_TRANSPORT_NON_SECURE) {
				if (client->transport.tcp.sock < 0) {
					LOG_ERR("Invalid MQTT socket before wait");
					mqtt_abort(client);
					return -EINVAL;
				}
			}

			// LOG_DBG("DEBUG: Waiting for MQTT input, connected: %d, wait_attempts: %d", connected, wait_attempts);

			// Wait for data or timeout
			
			int wait_ret = wait(short_poll_timeout); // Wait for a shorter period
            LOG_DBG("MQTT inner wait returned: %d, connected: %d", wait_ret, connected);

            if (wait_ret > 0) { // Data received
                int input_ret = mqtt_input(client); // Process it (this is where CONNACK is handled)
                LOG_DBG("MQTT input returned: %d", input_ret);
            } else if (wait_ret < 0) { // Error during poll
                LOG_ERR("MQTT inner wait error: %d, errno: %d", wait_ret, errno);
                mqtt_abort(client); // Abort this attempt
                break; // Exit inner while
            }
            // If wait_ret == 0 (timeout), just continue waiting for next short_poll_timeout
            wait_attempts++;
		}

		if (connected) {
            LOG_INF("DEBUG: Connected flag is true, breaking from try_to_connect loop.");
            break; // SUCCESS! Exit the main 'while' loop.
        }

        // If we reach here, it means we ran out of wait_attempts or an error occurred.
        if (!connected) {
            LOG_ERR("MQTT ABORT... (still not connected after polling)");
            mqtt_abort(client); // Abort if still not connected after attempts
        }

        LOG_ERR("MQTT WHAAA... (end of try_to_connect loop iteration, will retry if not connected)");
        k_sleep(K_MSEC(APP_SLEEP_MSECS)); // Sleep before next outer loop retry





		// int wait_ret = wait(APP_CONNECT_TIMEOUT_MS); // Capture wait() return value
        // LOG_ERR("MQTT WAIT RETURNED: %d", wait_ret); // NEW LOG

        // if (wait_ret > 0) { // Only call mqtt_input if wait() indicates data
        //     int input_ret = mqtt_input(client); // Capture mqtt_input() return value
        //     LOG_ERR("MQTT INPUT RETURNED: %d", input_ret); // NEW LOG
        // } else if (wait_ret == 0) {
        //     LOG_ERR("MQTT WAIT: Timeout occurred."); // NEW LOG
        // } else { // wait_ret < 0
        //     LOG_ERR("MQTT WAIT: Error occurred, ret=%d", wait_ret); // NEW LOG
        // }

		// // if (!connected) {
		// // 	mqtt_abort(client);
		// // 	LOG_ERR("MQTT ABORT...");
		// // }

		// if (connected) {
        //     LOG_INF("DEBUG: Connected flag is true, breaking from try_to_connect loop.");
        //     break; // Exit the while loop
        // }

        // if (!connected) { // If still not connected after polling, abort and retry
        //     mqtt_abort(client);
        //     LOG_ERR("MQTT ABORT...");
        // }

		// LOG_ERR("MQTT WHAAA... (end of try_to_connect loop iteration)");
        // k_sleep(K_MSEC(APP_SLEEP_MSECS));
		
	}

	if (connected) {
        LOG_ERR("MQTT CONNECTED: Returning 0");
        return 0;
    }

    LOG_ERR("MQTT NOT CONNECTED: Returning -EINVAL (Failed after retries)");
    return -EINVAL;
}

static int process_mqtt_and_sleep(struct mqtt_client *client, int timeout)
{
	int64_t remaining = timeout;
	int64_t start_time = k_uptime_get();
	int rc;

	while (remaining > 0 && connected) {
		if (wait(remaining)) {
			rc = mqtt_input(client);
			if (rc != 0) {
				PRINT_RESULT("mqtt_input", rc);
				return rc;
			}
		}

		rc = mqtt_live(client);
		if (rc != 0 && rc != -EAGAIN) {
			PRINT_RESULT("mqtt_live", rc);
			return rc;
		} else if (rc == 0) {
			rc = mqtt_input(client);
			if (rc != 0) {
				PRINT_RESULT("mqtt_input", rc);
				return rc;
			}
		}

		remaining = timeout + start_time - k_uptime_get();
	}

	return 0;
}

#define SUCCESS_OR_EXIT(rc) { if (rc != 0) { return 1; } }
#define SUCCESS_OR_BREAK(rc) { if (rc != 0) { break; } }

static int publisher(void)
{
	// LOG_ERR("Starting PUBLISHERRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRR...");
	int i, rc, r = 0;

	LOG_INF("attempting to connect: ");
	rc = try_to_connect(&client_ctx);
	// LOG_ERR("CONNECTED PUBLISHERRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRR...");
	PRINT_RESULT("try_to_connect", rc);
	SUCCESS_OR_EXIT(rc);

	i = 0;
	while (i++ < CONFIG_NET_SAMPLE_APP_MAX_ITERATIONS && connected) {
		r = -1;

		rc = mqtt_ping(&client_ctx);
		PRINT_RESULT("mqtt_ping", rc);
		SUCCESS_OR_BREAK(rc);

		rc = process_mqtt_and_sleep(&client_ctx, APP_SLEEP_MSECS);
		SUCCESS_OR_BREAK(rc);

		rc = publish(&client_ctx, MQTT_QOS_0_AT_MOST_ONCE);
		PRINT_RESULT("mqtt_publish", rc);
		SUCCESS_OR_BREAK(rc);

		rc = process_mqtt_and_sleep(&client_ctx, APP_SLEEP_MSECS);
		SUCCESS_OR_BREAK(rc);

		rc = publish(&client_ctx, MQTT_QOS_1_AT_LEAST_ONCE);
		PRINT_RESULT("mqtt_publish", rc);
		SUCCESS_OR_BREAK(rc);

		rc = process_mqtt_and_sleep(&client_ctx, APP_SLEEP_MSECS);
		SUCCESS_OR_BREAK(rc);

		rc = publish(&client_ctx, MQTT_QOS_2_EXACTLY_ONCE);
		PRINT_RESULT("mqtt_publish", rc);
		SUCCESS_OR_BREAK(rc);

		rc = process_mqtt_and_sleep(&client_ctx, APP_SLEEP_MSECS);
		SUCCESS_OR_BREAK(rc);

		r = 0;
	}

	rc = mqtt_disconnect(&client_ctx);
	PRINT_RESULT("mqtt_disconnect", rc);

	LOG_INF("Bye!");

	return r;
}

// --- START: Added Wi-Fi connection functions ---
// Semaphore to signal when an IPv4 address is acquired
static K_SEM_DEFINE(ipv4_acquired, 0, 1);
static struct net_mgmt_event_callback net_mgmt_cb; // Callback for network events

static void net_mgmt_event_handler_ip_addr(struct net_mgmt_event_callback *cb,
					   uint32_t mgmt_event, struct net_if *iface)
{
	if (mgmt_event == NET_EVENT_IPV4_ADDR_ADD) {
		LOG_INF("IPv4 address acquired on interface %p", iface);
		k_sem_give(&ipv4_acquired);
	}
}




static void wifi_mgmt_event_handler(struct net_mgmt_event_callback *cb,
					uint32_t mgmt_event, struct net_if *iface)
{
	if (mgmt_event == NET_EVENT_WIFI_CONNECT_RESULT) {
		struct wifi_status *status = (struct wifi_status *)cb->info;
		if (status->status) {
			LOG_ERR("Wi-Fi connection failed: %d", status->status);
			wifi_connected = false;
		} else {
			LOG_INF("Wi-Fi connected successfully.");
			wifi_connected = true;
		}
	} else if (mgmt_event == NET_EVENT_WIFI_DISCONNECT_RESULT) {
		LOG_INF("Wi-Fi disconnected.");
		wifi_connected = false;
	}
}

static void connect_to_wifi(void)
{
	struct net_if *iface = net_if_get_default();
	static struct net_mgmt_event_callback wifi_cb;

	// Initialize and add the Wi-Fi management event callback
	net_mgmt_init_event_callback(&wifi_cb, wifi_mgmt_event_handler,
					NET_EVENT_WIFI_CONNECT_RESULT | NET_EVENT_WIFI_DISCONNECT_RESULT);
	net_mgmt_add_event_callback(&wifi_cb);

	struct wifi_connect_req_params wifi_params = {
		.ssid = MY_WIFI_SSID,
		.ssid_length = strlen(MY_WIFI_SSID),
		.psk = MY_WIFI_PASSWORD,
		.psk_length = strlen(MY_WIFI_PASSWORD),
		.channel = WIFI_CHANNEL_ANY,
		.security = WIFI_SECURITY_TYPE_PSK, // Assuming WPA/WPA2-PSK
	};

	LOG_INF("Initiating Wi-Fi connection to SSID: %s", MY_WIFI_SSID);
	int ret = net_mgmt(NET_REQUEST_WIFI_CONNECT, iface, &wifi_params, sizeof(wifi_params));
	if (ret) {
		LOG_ERR("Wi-Fi connect request failed: %d", ret);
	}

	// Wait for the wifi_connected flag to be set by the event handler
	while (!wifi_connected) {
		k_sleep(K_MSEC(500));
		LOG_DBG("Waiting for Wi-Fi connection...");
	}

	LOG_INF("Wi-Fi association successful.");
}
// --- END: Added Wi-Fi connection functions ---

static int start_app(void)
{
	// int r = 0, i = 0;

	// while (!CONFIG_NET_SAMPLE_APP_MAX_CONNECTIONS ||
	//        i++ < CONFIG_NET_SAMPLE_APP_MAX_CONNECTIONS) {
	// 	r = publisher();

	// 	if (!CONFIG_NET_SAMPLE_APP_MAX_CONNECTIONS) {
	// 		k_sleep(K_MSEC(5000));
	// 	}
	// }

	while (!wifi_connected) {
        k_sleep(K_MSEC(100)); // Just a small pause
        LOG_DBG("start_app waiting for wifi_connected...");
    }
    LOG_INF("Wi-Fi connected and network ready. Starting MQTT publisher operations.");

    int r = 0, i = 0;

    // ... rest of your publisher() logic remains the same ...
    // e.g., calling try_to_connect, publish, process_mqtt_and_sleep etc.
    r = publisher(); // This is the core loop of the original sample

	return r;
}


int main(void)
{
	connect_to_wifi();
	wait_for_network();

	#if defined(CONFIG_MQTT_LIB_TLS)
	int rc;

	rc = tls_init();
	PRINT_RESULT("tls_init", rc);
	#endif

	int ret = start_app();
	exit(ret);
	return ret;
}
