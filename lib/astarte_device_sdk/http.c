/*
 * (C) Copyright 2024, SECO Mind Srl
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "http.h"

#include <zephyr/kernel.h>
#include <zephyr/net/http/client.h>
#include <zephyr/net/http/status.h>
#include <zephyr/net/socket.h>

#ifndef CONFIG_ASTARTE_DEVICE_SDK_DEVELOP_USE_NON_TLS_HTTP
#include <zephyr/net/tls_credentials.h>
#endif

#include "log.h"

ASTARTE_LOG_MODULE_REGISTER(astarte_http, CONFIG_ASTARTE_DEVICE_SDK_HTTP_LOG_LEVEL);

/************************************************
 *        Defines, constants and typedef        *
 ***********************************************/

/** @brief Context struct for any HTTP request */
struct http_req_ctx
{
    /** @brief Flag to store the success or failure of the request */
    bool request_ok;
    /** @brief Buffer where to store the response */
    uint8_t *resp_buf;
    /** @brief Size of the response buffer */
    size_t resp_buf_size;
    /** @brief Number of bytes written in the response buffer */
    size_t bytes_written;
};

/************************************************
 *       Checks over configuration values       *
 ***********************************************/

#ifdef CONFIG_ASTARTE_DEVICE_SDK_DEVELOP_USE_NON_TLS_HTTP
#warning "TLS has been disabled (unsafe)!"
#endif /* defined(CONFIG_ASTARTE_DEVICE_SDK_DEVELOP_USE_NON_TLS_HTTP) */

BUILD_ASSERT(sizeof(CONFIG_ASTARTE_DEVICE_SDK_HOSTNAME) != 1, "Missing hostname in configuration");

/************************************************
 *       Callbacks declaration/definition       *
 ***********************************************/

static int http_response_cb(
    struct http_response *rsp, enum http_final_call final_data, void *user_data)
{
    int res = 0;
    struct http_req_ctx *ctx = (struct http_req_ctx *) user_data;

    ASTARTE_LOG_DBG("http_response_cb called. Status: %s (%d), Fragment len: %zu",
        rsp->http_status ? rsp->http_status : "N/A", rsp->http_status_code, rsp->body_frag_len);

    // Evaluate the status code if it has been parsed by Zephyr
    if ((rsp->http_status_code < HTTP_200_OK)
        || (rsp->http_status_code >= HTTP_300_MULTIPLE_CHOICES)) {
        ASTARTE_LOG_ERR(
            "HTTP request failed, response code: %s %d", rsp->http_status, rsp->http_status_code);
        ctx->request_ok = false;
    }

    // Accumulate the parsed body fragment into the output buffer
    if (rsp->body_frag_start && rsp->body_frag_len > 0) {
        ASTARTE_LOG_DBG("Processing body fragment of size %zu. Current bytes written: %zu",
            rsp->body_frag_len, ctx->bytes_written);

        if (ctx->resp_buf) {
            // Check if we have enough space (saving 1 byte for the null terminator)
            if (ctx->bytes_written + rsp->body_frag_len < ctx->resp_buf_size) {
                memcpy(
                    ctx->resp_buf + ctx->bytes_written, rsp->body_frag_start, rsp->body_frag_len);
                ctx->bytes_written += rsp->body_frag_len;
                ASTARTE_LOG_DBG("Fragment successfully copied to response buffer.");
            } else {
                ASTARTE_LOG_ERR("HTTP reply body exceeds provided buffer size (%zu). Needed at "
                                "least %zu bytes.",
                    ctx->resp_buf_size, ctx->bytes_written + rsp->body_frag_len + 1);
                ctx->request_ok = false;
                res = -1;
                goto exit;
            }
        } else {
            ASTARTE_LOG_DBG("No response buffer provided in context. Dropping fragment.");
        }
    }

    if (final_data == HTTP_DATA_FINAL) {
        ASTARTE_LOG_DBG(
            "All HTTP data received. Total payload size written: %zu bytes", ctx->bytes_written);
        // Force null-termination safely
        if (ctx->resp_buf && ctx->resp_buf_size > 0) {
            ctx->resp_buf[ctx->bytes_written] = '\0';
            ASTARTE_LOG_DBG("Response buffer successfully null-terminated.");
        }
    } else {
        ASTARTE_LOG_DBG("Awaiting more HTTP data...");
    }

exit:
    return res;
}

/************************************************
 *         Static functions declaration         *
 ***********************************************/

/**
 * @brief Create a new TCP socket and connect it to a server.
 *
 * @note The returned socket should be closed once its use has terminated.
 *
 * @return -1 upon failure, a file descriptor for the new socket otherwise.
 */
static int create_and_connect_socket(void);

#ifdef CONFIG_ASTARTE_DEVICE_SDK_HTTP_LOG_LEVEL_DBG
/**
 * @brief Print content of addrinfo struct.
 *
 * @param[in] input_addinfo addrinfo struct to print.
 */
static void dump_addrinfo(const struct zsock_addrinfo *input_addinfo);
#endif

static astarte_result_t astarte_http_do_request(enum http_method method, int32_t timeout_ms,
    const char *url, const char **header_fields, const char *payload, struct http_req_ctx *ctx);

/************************************************
 *         Global functions definitions         *
 ***********************************************/

astarte_result_t astarte_http_post(int32_t timeout_ms, const char *url, const char **header_fields,
    const char *payload, uint8_t *resp_buf, size_t resp_buf_size)
{
    ASTARTE_LOG_DBG("Initiating HTTP POST request to URL: %s (Timeout: %d ms, Buffer size: %zu)",
        url, timeout_ms, resp_buf_size);

    struct http_req_ctx ctx = {
        .request_ok = true, .resp_buf = resp_buf, .resp_buf_size = resp_buf_size, .bytes_written = 0
    };

    return astarte_http_do_request(HTTP_POST, timeout_ms, url, header_fields, payload, &ctx);
}

astarte_result_t astarte_http_get(int32_t timeout_ms, const char *url, const char **header_fields,
    uint8_t *resp_buf, size_t resp_buf_size)
{
    ASTARTE_LOG_DBG("Initiating HTTP GET request to URL: %s (Timeout: %d ms, Buffer size: %zu)",
        url, timeout_ms, resp_buf_size);

    struct http_req_ctx ctx = {
        .request_ok = true, .resp_buf = resp_buf, .resp_buf_size = resp_buf_size, .bytes_written = 0
    };

    return astarte_http_do_request(HTTP_GET, timeout_ms, url, header_fields, NULL, &ctx);
}

/************************************************
 *         Static functions definitions         *
 ***********************************************/

static int create_and_connect_socket(void)
{
    char hostname[] = CONFIG_ASTARTE_DEVICE_SDK_HOSTNAME;
#ifdef CONFIG_ASTARTE_DEVICE_SDK_DEVELOP_USE_NON_TLS_HTTP
    char port[] = "80";
#else
    char port[] = "443";
#endif

    ASTARTE_LOG_DBG("Attempting DNS resolution for %s:%s", hostname, port);

    struct zsock_addrinfo hints = { 0 };
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    struct zsock_addrinfo *broker_addrinfo = NULL;
    int getaddrinfo_rc = zsock_getaddrinfo(hostname, port, &hints, &broker_addrinfo);
    if (getaddrinfo_rc != 0) {
        ASTARTE_LOG_ERR("Unable to resolve address (%d) %s", getaddrinfo_rc,
            zsock_gai_strerror(getaddrinfo_rc));
        if (getaddrinfo_rc == DNS_EAI_SYSTEM) {
            ASTARTE_LOG_ERR("Errno: (%d) %s", errno, strerror(errno));
        }
        return -1;
    }

    ASTARTE_LOG_DBG("DNS resolution successful. Iterating through available addresses.");

#ifdef CONFIG_ASTARTE_DEVICE_SDK_DEVELOP_USE_NON_TLS_HTTP
    int proto = IPPROTO_TCP;
    ASTARTE_LOG_DBG("Using cleartext TCP (IPPROTO_TCP)");
#else
    int proto = IPPROTO_TLS_1_2;
    ASTARTE_LOG_DBG("Using secure TLS (IPPROTO_TLS_1_2)");
#endif

    int sock = -1;
    struct zsock_addrinfo *curr_addr = NULL;

    // Iterate through the linked list of resolved addresses
    for (curr_addr = broker_addrinfo; curr_addr != NULL; curr_addr = curr_addr->ai_next) {
#ifdef CONFIG_ASTARTE_DEVICE_SDK_HTTP_LOG_LEVEL_DBG
        dump_addrinfo(curr_addr);
#endif

        ASTARTE_LOG_DBG("Attempting to create socket (family: %d, socktype: %d, proto: %d)",
            curr_addr->ai_family, curr_addr->ai_socktype, proto);

        sock = zsock_socket(curr_addr->ai_family, curr_addr->ai_socktype, proto);
        if (sock == -1) {
            ASTARTE_LOG_DBG("Socket creation failed for this address, trying next address...");
            continue;
        }

        ASTARTE_LOG_DBG("Socket successfully created (fd: %d). Applying options.", sock);

#ifndef CONFIG_ASTARTE_DEVICE_SDK_DEVELOP_USE_NON_TLS_HTTP
        sec_tag_t sec_tag_opt[] = {
            CONFIG_ASTARTE_DEVICE_SDK_HTTPS_CA_CERT_TAG,
        };

        ASTARTE_LOG_DBG("Setting TLS_SEC_TAG_LIST option.");
        int sockopt_rc
            = zsock_setsockopt(sock, SOL_TLS, TLS_SEC_TAG_LIST, sec_tag_opt, sizeof(sec_tag_opt));
        if (sockopt_rc == -1) {
            ASTARTE_LOG_ERR("Socket options error (TLS_SEC_TAG_LIST): %d", sockopt_rc);
            zsock_close(sock);
            sock = -1;
            continue;
        }

        ASTARTE_LOG_DBG("Setting TLS_HOSTNAME option to '%s'.", hostname);
        sockopt_rc = zsock_setsockopt(sock, SOL_TLS, TLS_HOSTNAME, hostname, strlen(hostname));
        if (sockopt_rc == -1) {
            ASTARTE_LOG_ERR("Socket options error (TLS_HOSTNAME): %d", sockopt_rc);
            zsock_close(sock);
            sock = -1;
            continue;
        }
#endif

        ASTARTE_LOG_DBG("Attempting to connect socket %d to remote address.", sock);
        int connect_rc = zsock_connect(sock, curr_addr->ai_addr, curr_addr->ai_addrlen);
        if (connect_rc == -1) {
            ASTARTE_LOG_DBG(
                "Connection failed (%d -  %s), closing socket and trying next address...", errno,
                strerror(errno));
            zsock_close(sock);
            sock = -1;
            continue;
        }

        // If we reach here, we have successfully connected
        ASTARTE_LOG_DBG("Successfully connected socket %d.", sock);
        break;
    }

    // Free the linked list after we are done iterating
    zsock_freeaddrinfo(broker_addrinfo);

    // Check if we exhausted the list without a successful connection
    if (sock == -1) {
        ASTARTE_LOG_ERR("Failed to connect to any resolved address. Exhausted all DNS records.");
    }

    return sock;
}

#ifdef CONFIG_ASTARTE_DEVICE_SDK_HTTP_LOG_LEVEL_DBG
#define ADDRINFO_IP_ADDR_SIZE 16U
static void dump_addrinfo(const struct zsock_addrinfo *input_addinfo)
{
    char ip_addr[ADDRINFO_IP_ADDR_SIZE] = { 0 };
    zsock_inet_ntop(AF_INET, &((struct sockaddr_in *) input_addinfo->ai_addr)->sin_addr, ip_addr,
        sizeof(ip_addr));
    ASTARTE_LOG_DBG("addrinfo @%p: ai_family=%d, ai_socktype=%d, ai_protocol=%d, "
                    "sa_family=%d, sin_port=%x, ip_addr=%s ai_addrlen=%zu",
        input_addinfo, input_addinfo->ai_family, input_addinfo->ai_socktype,
        input_addinfo->ai_protocol, input_addinfo->ai_addr->sa_family,
        ((struct sockaddr_in *) input_addinfo->ai_addr)->sin_port, ip_addr,
        input_addinfo->ai_addrlen);
}
#endif

static astarte_result_t astarte_http_do_request(enum http_method method, int32_t timeout_ms,
    const char *url, const char **header_fields, const char *payload, struct http_req_ctx *ctx)
{
    ASTARTE_LOG_DBG("Entering astarte_http_do_request. Method: %d, URL: %s", method, url);

    int sock = create_and_connect_socket();
    if (sock < 0) {
        ASTARTE_LOG_ERR("Aborting HTTP request due to socket creation/connection failure.");
        return ASTARTE_RESULT_SOCKET_ERROR;
    }

    struct http_request req = { 0 };
    // TODO: This buffer is still allocated on the stack. Consider moving this to the heap to avoid
    // the stack overflow risk.
    uint8_t recv_buf[CONFIG_ASTARTE_DEVICE_SDK_ADVANCED_HTTP_RCV_BUFFER_SIZE];
    memset(recv_buf, 0, sizeof(recv_buf));

    req.method = method;
    req.host = CONFIG_ASTARTE_DEVICE_SDK_HOSTNAME;
    req.port = NULL;
    req.url = url;
    req.header_fields = header_fields;
    req.protocol = "HTTP/1.1";
    req.response = http_response_cb;

    if (payload) {
        req.content_type_value = "application/json";
        req.payload = payload;
        req.payload_len = strlen(payload);
        ASTARTE_LOG_DBG("Payload attached. Length: %zu", req.payload_len);
    } else {
        ASTARTE_LOG_DBG("No payload attached to this request.");
    }

    req.recv_buf = recv_buf;
    req.recv_buf_len = sizeof(recv_buf);

    ASTARTE_LOG_DBG("Executing http_client_req on socket %d...", sock);

    // Pass context struct as the user_data parameter
    int http_rc = http_client_req(sock, &req, timeout_ms, ctx);

    ASTARTE_LOG_DBG("http_client_req returned with code: %d", http_rc);

    if ((http_rc < 0) || !ctx->request_ok) {
        ASTARTE_LOG_ERR("HTTP request failed (http_client_req code: %d, context flag ok: %d)",
            http_rc, ctx->request_ok);
        zsock_close(sock);
        return ASTARTE_RESULT_HTTP_REQUEST_ERROR;
    }

    ASTARTE_LOG_DBG("HTTP request completed successfully. Closing socket %d.", sock);
    zsock_close(sock);

    return ASTARTE_RESULT_OK;
}
