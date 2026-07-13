#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Forward declarations */
int handle_cli_mode(int argc, char *argv[]);
int handle_rest_api_mode(int argc, char *argv[]);

static void print_main_help(void)
{
	printf("ft2-api - MOD to WAV renderer (CLI & REST API)\n");
	printf("Version: 1.0\n\n");
	printf("USAGE:\n");
	printf("  ft2-api --cli <command> [args]       CLI mode\n");
	printf("  ft2-api --server [port]              REST API server mode\n");
	printf("  ft2-api --help                       Show this help\n\n");
	printf("CLI EXAMPLES:\n");
	printf("  ft2-api --cli render song.mod song.wav\n");
	printf("  ft2-api --cli render song.mod song.wav --rate 48000\n");
	printf("  ft2-api --cli help                   Show CLI help\n\n");
	printf("REST API EXAMPLES:\n");
	printf("  ft2-api --server                     Start on port 8080\n");
	printf("  ft2-api --server 9000                Start on port 9000\n\n");
	printf("REST API ENDPOINTS:\n");
	printf("  GET  /api/health                     Health check\n");
	printf("  POST /api/render                     Render MOD to WAV\n");
	printf("  GET  /api/download/<filename>        Download rendered WAV\n\n");
	printf("CURL EXAMPLES:\n");
	printf("  curl http://localhost:8080/api/health\n");
	printf("  curl -F 'file=@song.mod' http://localhost:8080/api/render\n");
}

int main(int argc, char *argv[])
{
	/* No arguments - show help */
	if (argc < 2) {
		print_main_help();
		return 0;
	}

	const char *mode = argv[1];

	/* CLI mode */
	if (strcmp(mode, "--cli") == 0) {
		return handle_cli_mode(argc, argv);
	}

	/* REST API server mode */
	if (strcmp(mode, "--server") == 0) {
		return handle_rest_api_mode(argc, argv);
	}

	/* Help */
	if (strcmp(mode, "--help") == 0 || strcmp(mode, "-h") == 0) {
		print_main_help();
		return 0;
	}

	/* Unknown mode */
	fprintf(stderr, "Error: Unknown mode '%s'\n", mode);
	fprintf(stderr, "Use 'ft2-api --help' for usage information\n");
	return 1;
}
