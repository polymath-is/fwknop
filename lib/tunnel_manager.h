/*
 * tunnel_manager.h
 *
 *  Created on: Jun 27, 2017
 *      Author: Daniel Bailey
 */

#ifndef TUNNEL_MANAGER_H_
#define TUNNEL_MANAGER_H_

#include <uv.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <json-c/json.h>
#include "fko_limits.h"
#include "hash_table.h"

#define ID_TOKEN_BUF_LEN 2048
#define MAX_PIPE_MSG_LEN 65536   // TODO: this is max tcp packet size

#define MAX_TUNNEL_CON_ATTEMPTS 5
#define INITIAL_TUNNEL_CON_RETRY_DELAY 1

#define TUNNEL_PORT 8282
#define TUNNEL_BACKLOG 10

#define NAME_TM_GATEWAY_PIPE "tm_g_pipe"
#define NAME_TM_CLIENT_PIPE "tm_c_pipe"


typedef enum {
    KEY_DATA_TYPE_SDP_ID,
    KEY_DATA_TYPE_IP_STRING
} key_data_type_t;


typedef enum {
    REQUEST_OR_OPENED_TYPE_REQUEST = 1,
    REQUEST_OR_OPENED_TYPE_OPENED
} request_or_opened_type_t;

typedef enum {
    TM_CON_STATE_DISCONNECTED = 0,
    TM_CON_STATE_CONNECTING,
    TM_CON_STATE_CONNECTED
} tm_con_state_t;

enum {
    PTR_TO_JSON,
    PTR_TO_STR
};

enum {
    SEND_PTR,
    SEND_STR
};

enum {
    IS_SDP_GATEWAY = 0,
    IS_SDP_CLIENT = 1
};


struct pipe_client_item{
    uv_pipe_t *handle;
    struct pipe_client_item *next;
};
typedef struct pipe_client_item *pipe_client_item_t;


struct tunnel_manager{
    int is_sdp_client;
    char *pipe_name;
    uv_loop_t *loop;
    uv_pipe_t *tm_pipe;
    pipe_client_item_t pipe_client_list;
    uv_read_cb pipe_read_cb_ptr;
    int tm_sock_fd;
    uv_tcp_t *tm_tcp_server;
    void *program_options_ptr;
    //uv_pipe_t *pipe_to_tm;
    //int read_pipe_to_tunnel_manager;
	//int write_pipe_to_tunnel_manager;
    //int read_pipe_from_tunnel_manager;
	//int write_pipe_from_tunnel_manager;
	hash_table_t *open_tunnel_hash_tbl;
	hash_table_t *requested_tunnel_hash_tbl;
    pthread_mutex_t requested_tunnel_hash_tbl_mutex;
};
typedef struct tunnel_manager *tunnel_manager_t;


struct tunneled_service{
    uint32_t service_id;
    uint32_t idp_id;
    char id_token[ID_TOKEN_BUF_LEN];
    short int request_sent;
    struct tunneled_service *next;
};
typedef struct tunneled_service *tunneled_service_t;

struct tunnel_info{
    uint32_t sdp_id;
    char remote_public_ip[MAX_IPV4_STR_LEN];
    char remote_tunnel_ip[MAX_IPV4_STR_LEN];
    unsigned int remote_port;
    uint32_t idp_id;
    char id_token[ID_TOKEN_BUF_LEN];
    tunneled_service_t services_requested;
    tunneled_service_t services_opened;
    uv_tcp_t *handle;
    tm_con_state_t con_state;
    int con_attempts;
    time_t next_con_retry_time;
    tunnel_manager_t tunnel_mgr;
    time_t created_time;
    struct tunnel_info *next;
};
typedef struct tunnel_info *tunnel_info_t;



typedef struct {
    uv_write_t req;
    uv_buf_t buf;
} write_req_t;


void tunnel_manager_tcp_close_cb(uv_handle_t* handle);
void tunnel_manager_pipe_close_cb(uv_handle_t* handle);
int  tunnel_manager_send_msg(tunnel_manager_t tunnel_mgr, int send_type, 
        uv_stream_t *handle, char *msg);
void tunnel_manager_handle_tunnel_traffic(tunnel_manager_t tunnel_mgr, 
        uint32_t sdp_id, char *packet);

void tunnel_manager_destroy(tunnel_manager_t tunnel_mgr);
int  tunnel_manager_new(void *program_options, int is_sdp_client, int tbl_len, 
        uv_read_cb pipe_read_cb_ptr, tunnel_manager_t *r_tunnel_mgr);
int  tunnel_manager_connect_pipe(tunnel_manager_t tunnel_mgr);

int tunnel_manager_add_service_to_tunnel(
        tunnel_info_t tunnel_data,
        uint32_t service_id,
        uint32_t idp_id,
        char *id_token,
        request_or_opened_type_t which_list,
        short int request_sent);

int tunnel_manager_mark_service_opened(
        tunnel_info_t tunnel_data,
        uint32_t service_id);

int tunnel_manager_mark_service_rejected(
        tunnel_info_t tunnel_data,
        uint32_t service_id);

int tunnel_manager_create_tunnel_item(
        uint32_t sdp_id,
        tunneled_service_t services_requested,
        tunneled_service_t services_opened,
        char *remote_public_ip,
        char *remote_tunnel_ip,
        uint32_t remote_port,
        uint32_t idp_id,
        char *id_token,
        uv_tcp_t *handle,
        tunnel_manager_t tunnel_mgr,
        tunnel_info_t *item);

void tunnel_manager_destroy_tunnel_item(tunnel_info_t item);

int  tunnel_manager_submit_client_request(tunnel_manager_t tunnel_mgr, 
        uint32_t sdp_id, char *ip_str);

int  tunnel_manager_submit_tunnel_record(
        tunnel_manager_t tunnel_mgr, 
        void *key_data,
        key_data_type_t data_type,
        request_or_opened_type_t which_table,
        tunnel_info_t r_tunnel_data);

int  tunnel_manager_find_tunnel_record(
        tunnel_manager_t tunnel_mgr, 
        void *key_data,
        key_data_type_t data_type,
        request_or_opened_type_t which_table,
        tunnel_info_t *r_tunnel_data);

int tunnel_manager_remove_tunnel_record(
        tunnel_manager_t tunnel_mgr, 
        tunnel_info_t tunnel_data,
        key_data_type_t data_type);

int  tunnel_manager_get_peer_addr_and_port(uv_tcp_t *peer, 
        char **ip_str, uint32_t *ip_num, uint32_t *port_num);
void tunnel_manager_close_client_cb(uv_handle_t *handle);
void tunnel_manager_alloc_buffer(uv_handle_t *handle, size_t suggested_size, uv_buf_t *buf);
int  tunnel_manager_ptr_2_array(const char* const ptr, char **r_array);
int  tunnel_manager_array_2_ptr(const char* const array, char **r_ptr);
int  tunnel_manager_send_to_tm(tunnel_manager_t tunnel_mgr, void *msg);
int  tunnel_manager_make_msg(
        const char *action, 
        uint32_t sdp_id, 
        uint32_t service_id, 
        uint32_t idp_id, 
        char *id_token, 
        char *packet, 
        char **r_msg);

int  tunnel_manager_process_json_msg(
        json_object *json_msg, 
        int *r_action,
        uint32_t *r_sdp_id,
        uint32_t *r_idp_id,
        uint32_t *r_service_id,
        char **r_id_token,
        char **r_tunnel_ip,
        char **r_packet);
int  tunnel_manager_process_json_msg_string(
        char *msg,
        int *r_action,
        uint32_t *r_sdp_id,
        uint32_t *r_idp_id,
        uint32_t *r_service_id,
        char **r_id_token,
        char **r_tunnel_ip,
        char **r_packet);

#endif /* SERVER_TUNNEL_MANAGER_H_ */