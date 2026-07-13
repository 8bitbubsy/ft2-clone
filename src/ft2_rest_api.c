#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <microhttpd.h>
#include "ft2_renderer.h"

#define REST_API_PORT 8080
#define MAX_UPLOAD_SIZE (50 * 1024 * 1024)  /* 50MB max file size */
#define TMP_DIR "/tmp/ft2-api"

typedef struct {
	struct MHD_PostProcessor *pp;
	char *mod_filename;
	char mod_data[MAX_UPLOAD_SIZE];
	uint32_t mod_size;
	char output_name[256];
	render_config_t config;
} upload_context_t;

/* Simple JSON builder - no external dependencies */
static char *build_json_response(const char *status, const char *message)
{
	size_t size = 512;
	char *json = malloc(size);
	if (!json) return NULL;

	snprintf(json, size, "{"
		"\"status\": \"%s\","
		"\"message\": \"%s\""
		"}", status, message);
	
	return json;
}

static char *build_json_render_result(const render_result_t *result,
	const char *filename)
{
	size_t size = 1024;
	char *json = malloc(size);
	if (!json) return NULL;

	if (result->success) {
		snprintf(json, size, "{"
			"\"status\": \"success\","
			"\"filename\": \"%s\","
			"\"samples\": %u,"
			"\"duration_seconds\": %.2f,"
			"\"download_url\": \"/api/download/%s\""
			"}", 
			filename,
			result->total_samples,
			result->duration_ms / 1000.0f,
			filename);
	} else {
		snprintf(json, size, "{"
			"\"status\": \"error\","
			"\"error\": \"%s\""
			"}", 
			render_error_to_string(result));
	}

	return json;
}

/* Health check endpoint */
static int handle_health(struct MHD_Connection *connection)
{
	const char *response = "{\"status\": \"ok\", \"version\": \"1.0\"}";
	struct MHD_Response *mhd_response = MHD_create_response_from_buffer(
		strlen(response),
		(void *)response,
		MHD_RESPMEM_PERSISTENT
	);

	MHD_add_response_header(mhd_response, "Content-Type", "application/json");
	int ret = MHD_queue_response(connection, MHD_HTTP_OK, mhd_response);
	MHD_destroy_response(mhd_response);
	return ret;
}

/* Render endpoint - accepts file upload */
static int handle_render(struct MHD_Connection *connection,
	const char *upload_data, size_t *upload_data_size)
{
	/* This is a simplified version - real implementation would handle multipart/form-data */
	/* For proof-of-concept, we'll show the endpoint structure */

	if (*upload_data_size > 0) {
		/* Process uploaded file data */
		*upload_data_size = 0;
	}

	/* Render file */
	render_config_t config = {
		.sample_rate = 44100,
		.bit_depth = 16,
		.amplification = 16,
		.start_pos = 0,
		.stop_pos = 255,
	};

	render_result_t result = render_mod_to_wav(
		"/tmp/ft2-api/uploaded.mod",
		"/tmp/ft2-api/output.wav",
		config
	);

	char *json_response = build_json_render_result(&result, "output.wav");
	if (!json_response) {
		const char *error = "{\"status\": \"error\", \"error\": \"Memory allocation failed\"}";
		struct MHD_Response *response = MHD_create_response_from_buffer(
			strlen(error), (void *)error, MHD_RESPMEM_PERSISTENT
		);
		MHD_queue_response(connection, MHD_HTTP_INTERNAL_SERVER_ERROR, response);
		MHD_destroy_response(response);
		return MHD_YES;
	}

	struct MHD_Response *mhd_response = MHD_create_response_from_buffer(
		strlen(json_response),
		(void *)json_response,
		MHD_RESPMEM_MUST_FREE
	);

	MHD_add_response_header(mhd_response, "Content-Type", "application/json");
	int ret = MHD_queue_response(connection, MHD_HTTP_OK, mhd_response);
	MHD_destroy_response(mhd_response);
	return ret;
}

/* Main request handler */
static int request_handler(void *cls,
	struct MHD_Connection *connection,
	const char *url,
	const char *method,
	const char *version,
	const char *upload_data,
	size_t *upload_data_size,
	void **con_cls)
{
	(void)cls;
	(void)version;

	printf("[%s] %s\n", method, url);

	/* Route: GET /api/health */
	if (strcmp(url, "/api/health") == 0 && strcmp(method, "GET") == 0) {
		return handle_health(connection);
	}

	/* Route: POST /api/render */
	if (strcmp(url, "/api/render") == 0 && strcmp(method, "POST") == 0) {
		return handle_render(connection, upload_data, upload_data_size);
	}

	/* Route: GET /api/download/:filename */
	if (strncmp(url, "/api/download/", 14) == 0 && strcmp(method, "GET") == 0) {
		const char *filename = url + 14;
		char filepath[512];
		snprintf(filepath, sizeof(filepath), "%s/%s", TMP_DIR, filename);

		FILE *f = fopen(filepath, "rb");
		if (!f) {
			const char *error = "{\"status\": \"error\", \"error\": \"File not found\"}";
			struct MHD_Response *response = MHD_create_response_from_buffer(
				strlen(error), (void *)error, MHD_RESPMEM_PERSISTENT
			);
			MHD_queue_response(connection, MHD_HTTP_NOT_FOUND, response);
			MHD_destroy_response(response);
			return MHD_YES;
		}

		fseek(f, 0, SEEK_END);
		size_t filesize = ftell(f);
		fseek(f, 0, SEEK_SET);

		void *filebuffer = malloc(filesize);
		if (!filebuffer) {
			fclose(f);
			const char *error = "{\"status\": \"error\", \"error\": \"Memory allocation failed\"}";
			struct MHD_Response *response = MHD_create_response_from_buffer(
				strlen(error), (void *)error, MHD_RESPMEM_PERSISTENT
			);
			MHD_queue_response(connection, MHD_HTTP_INTERNAL_SERVER_ERROR, response);
			MHD_destroy_response(response);
			return MHD_YES;
		}

		fread(filebuffer, 1, filesize, f);
		fclose(f);

		struct MHD_Response *response = MHD_create_response_from_buffer(
			filesize, filebuffer, MHD_RESPMEM_MUST_FREE
		);
		MHD_add_response_header(response, "Content-Type", "audio/wav");
		int ret = MHD_queue_response(connection, MHD_HTTP_OK, response);
		MHD_destroy_response(response);
		return ret;
	}

	/* Route: Not Found */
	const char *not_found = "{\"status\": \"error\", \"error\": \"Endpoint not found\"}";
	struct MHD_Response *response = MHD_create_response_from_buffer(
		strlen(not_found), (void *)not_found, MHD_RESPMEM_PERSISTENT
	);
	int ret = MHD_queue_response(connection, MHD_HTTP_NOT_FOUND, response);
	MHD_destroy_response(response);
	return ret;
}

int handle_rest_api_mode(int argc, char *argv[])
{
	int port = REST_API_PORT;

	/* Parse optional port argument */
	if (argc > 3) {
		port = atoi(argv[3]);
		if (port < 1024 || port > 65535) {
			fprintf(stderr, "Error: Port must be between 1024 and 65535\n");
			return 1;
		}
	}

	/* Create temp directory if needed */
	system("mkdir -p " TMP_DIR);

	printf("Starting ft2-api REST server on port %d...\n", port);
	printf("  Health check:  GET http://localhost:%d/api/health\n", port);
	printf("  Render MOD:    POST http://localhost:%d/api/render\n", port);
	printf("  Download WAV:  GET http://localhost:%d/api/download/<filename>\n", port);
	printf("\nPress Ctrl+C to stop.\n\n");

	/* Start HTTP server */
	struct MHD_Daemon *daemon = MHD_start_daemon(
		MHD_USE_AUTO,           /* Use auto-select best multiplexing */
		port,                   /* Port */
		NULL,                   /* Accept policy callback */
		NULL,                   /* Accept policy callback data */
		&request_handler,       /* Request handler */
		NULL,                   /* Request handler data */
		MHD_OPTION_END
	);

	if (!daemon) {
		fprintf(stderr, "Error: Failed to start HTTP daemon\n");
		return 1;
	}

	printf("Server started successfully!\n");

	/* Run until interrupted */
	printf("Waiting for requests (Ctrl+C to exit)...\n");
	getchar();  /* Wait for user input */

	MHD_stop_daemon(daemon);
	printf("\nServer stopped.\n");

	return 0;
}
