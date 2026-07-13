#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include "ft2_renderer.h"

static void print_cli_help(void)
{
	printf("ft2-api CLI - MOD to WAV renderer\n\n");
	printf("USAGE:\n");
	printf("  ft2-api --cli render <input.mod> <output.wav> [options]\n");
	printf("  ft2-api --cli help\n\n");
	printf("OPTIONS:\n");
	printf("  --rate FREQ      Sample rate: 44100, 48000, 96000, etc. (default: 44100)\n");
	printf("  --bits DEPTH     Bit depth: 16 or 32 (default: 16)\n");
	printf("  --amp LEVEL      Amplification: 1-32 (default: 16)\n");
	printf("  --start POS      Start pattern (0-255, default: 0)\n");
	printf("  --stop POS       Stop pattern (0-255, default: end of song)\n");
	printf("  --help           Show this help message\n\n");
	printf("EXAMPLES:\n");
	printf("  ft2-api --cli render song.mod song.wav\n");
	printf("  ft2-api --cli render song.mod song.wav --rate 48000 --bits 16\n");
	printf("  ft2-api --cli render song.mod song.wav --amp 20 --start 0 --stop 31\n");
}

static void print_render_result(const render_result_t *result,
	const char *output_file)
{
	if (result->success) {
		printf("✓ Rendering successful!\n");
		printf("  Output: %s\n", output_file);
		printf("  Samples: %u\n", result->total_samples);
		printf("  Duration: %.2f seconds\n", result->duration_ms / 1000.0f);
	} else {
		printf("✗ Rendering failed:\n");
		printf("  Error: %s\n", render_error_to_string(result));
	}
}

int cli_render(int argc, char *argv[])
{
	if (argc < 4) {
		fprintf(stderr, "Error: Missing required arguments\n");
		fprintf(stderr, "Usage: ft2-api --cli render <input.mod> <output.wav> [options]\n");
		return 1;
	}

	const char *input_file = argv[2];
	const char *output_file = argv[3];

	/* Default configuration */
	render_config_t config = {
		.sample_rate = 44100,
		.bit_depth = 16,
		.amplification = 16,
		.start_pos = 0,
		.stop_pos = 255,  /* Will be set to actual end by renderer */
	};

	/* Parse optional arguments */
	static struct option long_options[] = {
		{"rate",   required_argument, 0, 'r'},
		{"bits",   required_argument, 0, 'b'},
		{"amp",    required_argument, 0, 'a'},
		{"start",  required_argument, 0, 's'},
		{"stop",   required_argument, 0, 't'},
		{"help",   no_argument,       0, 'h'},
		{0, 0, 0, 0}
	};

	int option_index = 0;
	int c;
	optind = 4;  /* Start parsing from 5th argument (after command, render, input, output) */

	while ((c = getopt_long(argc, argv, "r:b:a:s:t:h", long_options, &option_index)) != -1) {
		switch (c) {
			case 'r': {
				int rate = atoi(optarg);
				if (rate < 8000 || rate > 384000) {
					fprintf(stderr, "Error: Sample rate must be 8000-384000 Hz\n");
					return 1;
				}
				config.sample_rate = rate;
				break;
			}
			case 'b': {
				int bits = atoi(optarg);
				if (bits != 16 && bits != 32) {
					fprintf(stderr, "Error: Bit depth must be 16 or 32\n");
					return 1;
				}
				config.bit_depth = bits;
				break;
			}
			case 'a': {
				int amp = atoi(optarg);
				if (amp < 1 || amp > 32) {
					fprintf(stderr, "Error: Amplification must be 1-32\n");
					return 1;
				}
				config.amplification = amp;
				break;
			}
			case 's': {
				int start = atoi(optarg);
				if (start < 0 || start > 255) {
					fprintf(stderr, "Error: Start position must be 0-255\n");
					return 1;
				}
				config.start_pos = start;
				break;
			}
			case 't': {
				int stop = atoi(optarg);
				if (stop < 0 || stop > 255) {
					fprintf(stderr, "Error: Stop position must be 0-255\n");
					return 1;
				}
				config.stop_pos = stop;
				break;
			}
			case 'h':
				print_cli_help();
				return 0;
			default:
				fprintf(stderr, "Error: Unknown option\n");
				return 1;
		}
	}

	/* Validate file paths */
	if (!input_file || !output_file) {
		fprintf(stderr, "Error: Input and output file paths required\n");
		return 1;
	}

	/* Perform rendering */
	printf("Rendering MOD file to WAV...\n");
	printf("  Input:  %s\n", input_file);
	printf("  Output: %s\n", output_file);
	printf("  Rate:   %u Hz\n", config.sample_rate);
	printf("  Bits:   %u-bit\n", config.bit_depth);
	printf("  Amp:    %dx\n\n", config.amplification);

	render_result_t result = render_mod_to_wav(input_file, output_file, config);

	print_render_result(&result, output_file);
	printf("\n");

	return result.success ? 0 : 2;
}

int cli_help(void)
{
	print_cli_help();
	return 0;
}

int handle_cli_mode(int argc, char *argv[])
{
	if (argc < 3) {
		fprintf(stderr, "Usage: ft2-api --cli <command> [args]\n");
		fprintf(stderr, "Commands: render, help\n");
		return 1;
	}

	const char *command = argv[2];

	if (strcmp(command, "render") == 0) {
		return cli_render(argc, argv);
	} else if (strcmp(command, "help") == 0) {
		return cli_help();
	} else {
		fprintf(stderr, "Unknown command: %s\n", command);
		fprintf(stderr, "Valid commands: render, help\n");
		return 1;
	}
}
