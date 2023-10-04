/**
 * @file chip.c
 * @description A chip 8 emulator written in C.
 * @author Luke A. Holland
 */

#include <SDL2/SDL.h>
#include <GL/glew.h>
#include <SDL2/SDL_opengl.h>

#define LINEARLIB_IMPLEMENTATION
#include "linear.h"

#define WINDOW_WIDTH 640
#define WINDOW_HEIGHT 320

typedef unsigned short opcode_t;

enum {
	CHIP8_RAM_CAPACITY      = 0x1000,
	CHIP8_RAM_ENTRYPOINT    = 0x200,
	CHIP8_REGISTER_COUNT    = 0x10,
	CHIP8_STACK_CAPACITY    = 0x10,
	CHIP8_DISPLAY_WIDTH     = 0x40,
	CHIP8_DISPLAY_HEIGHT    = 0x20,
} CHIP8_CONSTANTS;

#define CELL_WIDTH  (WINDOW_WIDTH / CHIP8_DISPLAY_WIDTH)
#define CELL_HEIGHT (WINDOW_HEIGHT / CHIP8_DISPLAY_HEIGHT)

struct {
	uint16_t pc;
	uint8_t  ram[CHIP8_RAM_CAPACITY];
	uint8_t  v[CHIP8_REGISTER_COUNT];
	uint8_t delay;
	uint8_t sound;
	uint16_t I;

	uint8_t sp;
	uint16_t stack[CHIP8_STACK_CAPACITY];
	uint32_t display[CHIP8_DISPLAY_HEIGHT][CHIP8_DISPLAY_WIDTH];
} chip8;

/**
 * Each line is a series of bytes corresponding
 * to a binary image for the numbers 0 through 9
 */
static const uint8_t chip8_numbers[0x50] = {
	0xF0, 0x90, 0x90, 0x90, 0xF0, 
	0x20, 0x60, 0x20, 0x20, 0x70, 
	0xF0, 0x10, 0xF0, 0x80, 0xF0, 
	0xF0, 0x10, 0xF0, 0x10, 0xF0, 
	0x90, 0x90, 0xF0, 0x10, 0x10, 
	0xF0, 0x80, 0xF0, 0x10, 0xF0, 
	0xF0, 0x80, 0xF0, 0x90, 0xF0, 
	0xF0, 0x10, 0x20, 0x40, 0x40, 
	0xF0, 0x90, 0xF0, 0x90, 0xF0, 
	0xF0, 0x90, 0xF0, 0x10, 0xF0, 
	0xF0, 0x90, 0xF0, 0x90, 0x90, 
	0xE0, 0x90, 0xE0, 0x90, 0xE0, 
	0xF0, 0x80, 0x80, 0x80, 0xF0, 
	0xE0, 0x90, 0x90, 0x90, 0xE0, 
	0xF0, 0x80, 0xF0, 0x80, 0xF0, 
	0xF0, 0x80, 0xF0, 0x80, 0x80  
};

static int
chip8_init(const char *filename);

static void
chip8_execute_instruction(void);

static void
chip8_sprite_draw(int x, int y, int n);

static void
chip8_display_draw(void);

enum {
	BUFFER_VBO,
	BUFFER_EBO,
	BUFFER_VAO,
	BUFFER_COUNT,
};
GLuint buffers[BUFFER_COUNT];
static GLuint shader;

static SDL_Window *window;
static SDL_GLContext *context;
static SDL_Event event;
static int running;

static int
window_init(void);

static void
window_free(void);

#define FPS 60.0

static int keys[0x10];

int
main(int argc, char **argv)
{
	const char *filename;
	if (argc == 1) {
		fprintf(stderr, "usage: %s <rom>\n", argv[0]);
		exit(EXIT_FAILURE);
	}

	filename = argv[1];
	
	if (window_init() != 0) {
		fprintf(stderr, "Fatal: Failed to initialize window!\n");
		exit(EXIT_FAILURE);
	}

	if (chip8_init(filename) != 0) {
		fprintf(stderr, "Fatal: Failed to initialize the chip8!\n");
		exit(EXIT_FAILURE);
	}

	Uint32 start, end;
	float elapsed;
	const float allowed = 1.0 / FPS;
	
	running = 1;
	while (running) {
		while (SDL_PollEvent(&event)) {
			if (event.type == SDL_QUIT) {
				running = 0;
				break;
			} else if (event.type == SDL_KEYDOWN) {
				switch (event.key.keysym.sym) {
				case SDLK_0:
					keys[0x0] = 1;
					break;
				case SDLK_1:
					keys[0x1] = 1;
					break;
				case SDLK_2:
					keys[0x2] = 1;
					break;
				case SDLK_3:
					keys[0x3] = 1;
					break;
				case SDLK_4:
					keys[0x4] = 1;
					break;
				case SDLK_5:
					keys[0x5] = 1;
					break;
				case SDLK_6:
					keys[0x6] = 1;
					break;
				case SDLK_7:
					keys[0x7] = 1;
					break;
				case SDLK_8:
					keys[0x8] = 1;
					break;
				case SDLK_9:
					keys[0x9] = 1;
					break;
				case SDLK_a:
					keys[0xa] = 1;
					break;
				case SDLK_b:
					keys[0xd] = 1;
					break;
				case SDLK_c:
					keys[0xc] = 1;
					break;
				case SDLK_d:
					keys[0xd] = 1;
					break;
				case SDLK_e:
					keys[0xe] = 1;
					break;
				case SDLK_f:
					keys[0xf] = 1;
					break;
				}
			} else if (event.type == SDL_KEYUP) {
				switch (event.key.keysym.sym) {
				case SDLK_0:
					keys[0x0] = 0;
					break;
				case SDLK_1:
					keys[0x1] = 0;
					break;
				case SDLK_2:
					keys[0x2] = 0;
					break;
				case SDLK_3:
					keys[0x3] = 0;
					break;
				case SDLK_4:
					keys[0x4] = 0;
					break;
				case SDLK_5:
					keys[0x5] = 0;
					break;
				case SDLK_6:
					keys[0x6] = 0;
					break;
				case SDLK_7:
					keys[0x7] = 0;
					break;
				case SDLK_8:
					keys[0x8] = 0;
					break;
				case SDLK_9:
					keys[0x9] = 0;
					break;
				case SDLK_a:
					keys[0xa] = 0;
					break;
				case SDLK_b:
					keys[0xd] = 0;
					break;
				case SDLK_c:
					keys[0xc] = 0;
					break;
				case SDLK_d:
					keys[0xd] = 0;
					break;
				case SDLK_e:
					keys[0xe] = 0;
					break;
				case SDLK_f:
					keys[0xf] = 0;
					break;
				}
			}

		}
		
		start = SDL_GetTicks();
		glClear(GL_COLOR_BUFFER_BIT);
		chip8_execute_instruction();
		chip8_display_draw();
		SDL_GL_SwapWindow(window);

		end = SDL_GetTicks();
		elapsed = ((end-start) / (float) 1000.0);
		elapsed = allowed-elapsed;
		if (elapsed > 0.0) {
			SDL_Delay((Uint32) (elapsed * 1000.0));
		}
	}

	window_free();
	return 0;
}

static opcode_t
chip8_current_opcode(void)
{
	//opcode_t opcode;
	//opcode = chip8.ram[chip8.pc++];
	//opcode <<= 8;
	//opcode |= chip8.ram[chip8.pc++];
	return (opcode_t) (chip8.ram[chip8.pc] << 8 | chip8.ram[chip8.pc + 1]);
}

static void
unimplemented_instruction(opcode_t opcode)
{
	fprintf(stderr, "TODO: Implement %x\n", opcode);
	exit(EXIT_FAILURE);
}

static void
chip8_execute_instruction(void)
{
	opcode_t opcode;
	opcode = chip8_current_opcode();
	switch (opcode & 0xf000) {
	case 0x0000:
		switch (opcode & 0x00ff) {
		case 0xe0:
			for (int i = 0; i < CHIP8_DISPLAY_HEIGHT; i++) {
				for (int j = 0; j < CHIP8_DISPLAY_WIDTH; j++) {
					chip8.display[i][j] = 0;
				}
			}
			chip8.pc+=2;
			break;
		case 0xee:
			chip8.pc = chip8.stack[--chip8.sp];
			chip8.pc+=2;
			break;
		}
		break;
	case 0x1000:
		chip8.pc = opcode & 0x0fff;
		break;
	case 0x2000:
		chip8.stack[chip8.sp++] = chip8.pc;
		chip8.pc = opcode & 0x0fff;
		break;
	case 0x3000:
		if (chip8.v[(opcode & 0x0f00) >> 8] == (opcode & 0x00ff))
			chip8.pc+=4;
		else
			chip8.pc+=2;
		break;
	case 0x4000:
		if (chip8.v[(opcode & 0x0f00) >> 8] != (opcode & 0x00ff))
			chip8.pc+=4;
		else
			chip8.pc+=2;
		break;
	case 0x5000:
		unimplemented_instruction(opcode);
		break;
	case 0x6000:
		chip8.v[(opcode & 0x0f00) >> 8] = opcode & 0x00ff;
		chip8.pc+=2;
		break;
	case 0x7000:
		chip8.v[(opcode & 0x0f00) >> 8] += opcode & 0x00ff;
		chip8.pc+=2;
		break;
	case 0x8000:
		switch (opcode & 0x000f) {
		case 0x0000:
			chip8.v[(opcode & 0x0f00) >> 8] = chip8.v[(opcode & 0x00f0) >> 4];
			chip8.pc+=2;
			break;
		case 0x0001:
			unimplemented_instruction(opcode);
			break;
		case 0x0002:
			unimplemented_instruction(opcode);
			break;
		case 0x0003:
			unimplemented_instruction(opcode);
			break;
		case 0x0004:
			unimplemented_instruction(opcode);
			break;
		case 0x0005:
			unimplemented_instruction(opcode);
			break;
		case 0x0006:
			unimplemented_instruction(opcode);
			break;
		case 0x0007:
			unimplemented_instruction(opcode);
			break;
		case 0x000e:
			unimplemented_instruction(opcode);
			break;
		}
		break;
	case 0x9000:
		if (chip8.v[(opcode & 0x0f00) >> 8] != chip8.v[(opcode & 0x00f0) >> 4])
			chip8.pc+=4;
		else
			chip8.pc+=2;
		break;
	case 0xa000:
		chip8.I = opcode & 0x0fff;
		chip8.pc+=2;
		break;
	case 0xb000:
		unimplemented_instruction(opcode);
		break;
	case 0xc000:
		chip8.v[(opcode & 0x0f00) >> 8] = (random() % (0x100)) & (opcode & 0x00ff);
		chip8.pc+=2;
		break;
	case 0xd000:
		chip8_sprite_draw(chip8.v[(opcode & 0x0f00) >> 8],
				  chip8.v[(opcode & 0x00f0) >> 4],
				  opcode & 0x000f);
		chip8.pc+=2;
		break;
	case 0xe000:
		switch (opcode & 0x00ff) {
		case 0x009e:
			if (keys[chip8.v[(opcode & 0x0f00) >> 8]] == 1) {
				chip8.pc+=4;
			} else {
				chip8.pc+=2;
			}
			break;
		case 0x00a1:
			if (keys[chip8.v[(opcode & 0x0f00) >> 8]] == 0) {
				chip8.pc+=4;
			} else {
				chip8.pc+=2;
			}
			break;
		}
		break;
	case 0xf000:
		switch (opcode & 0x00ff) {
		case 0x07:
			chip8.v[(opcode & 0x0f00) >> 8] = chip8.delay;
			chip8.pc+=2;
			break;
		case 0x0a:
			{
				int key_pressed = 0;
				for (int i = 0; i < 0x10; i++) {
					if (keys[i]) {
						key_pressed = 1;
						chip8.v[(opcode & 0x0f00) >> 8] = i;
						break;
					}
				}

				if (key_pressed) {
					chip8.pc+=2;
				}
			}
			break;
		case 0x15:
			chip8.delay = chip8.v[(opcode & 0x0f00) >> 8];
			chip8.pc+=2;
			break;
		case 0x18:
			unimplemented_instruction(opcode);
			break;
		case 0x1e:
			chip8.I += chip8.v[(opcode & 0x0f00) >> 8];
			chip8.pc+=2;
			break;
		case 0x29:
			chip8.I = chip8.v[(opcode & 0x0f00) >> 8] * 5;
			chip8.pc+=2;
			 break;
		case 0x33:
			chip8.ram[chip8.I]     = chip8.v[(opcode & 0x0f00) >> 8] / 100;
			chip8.ram[chip8.I + 1] = (chip8.v[(opcode & 0x0f00) >> 8] / 10) % 10;
			chip8.ram[chip8.I + 2] = (chip8.v[(opcode & 0x0f00) >> 8]) % 10;
			chip8.pc+=2;
			break;
		case 0x55:
			for (int i = 0; i <= ((opcode & 0x0f00) >> 8); i++) {
				chip8.ram[chip8.I + i] = chip8.v[i];
			}
			chip8.I += ((opcode & 0x0f00) >> 8) + 1;
			chip8.pc+=2;
			break;
		case 0x65:
			for (int i = 0; i <= ((opcode & 0x0f00) >> 8); i++) {
				chip8.v[i] = chip8.ram[chip8.I + i];
			}

			chip8.I += ((opcode & 0x0f00) >> 8) + 1;
			chip8.pc+=2;
			break;
		}
		break;
	}
}

static void
chip8_sprite_draw(int x, int y, int n)
{
	chip8.v[0x0f] = 0;
	for (int i = 0; i < n; i++) {
		const int cell = chip8.ram[chip8.I+i];
		for (int j = 0; j < 8; j++) {
			if ((cell & (0x80 >> j)) != 0) {
				int pixel = chip8.display[y+i][x+j];
				if (chip8.display[y+i][x+j] != 0x000000) {
					chip8.v[0x0f] = 1;
				}
				pixel ^= cell;
				chip8.display[y+i][x+j] = pixel * 0xffffffff;
			}
		}
	}
}

static void
chip8_display_draw(void)
{
	ll_matrix_mode(LL_MATRIX_MODEL);
	for (int i = 0; i < CHIP8_DISPLAY_HEIGHT; i++) {
		for (int j = 0; j < CHIP8_DISPLAY_WIDTH; j++) {
			GLuint colour = chip8.display[i][j];
			ll_matrix_identity();
			ll_matrix_scale3f(CELL_WIDTH, CELL_HEIGHT, 1.0);
			ll_matrix_translate3f(j * CELL_WIDTH, i * CELL_HEIGHT, 0.0);
			glUniformMatrix4fv(glGetUniformLocation(shader, "model"),
					   1, GL_FALSE, ll_matrix_get_copy().data);
			glUniform4f(glGetUniformLocation(shader, "colour"),
				    ((colour & 0xff000000) >> 24) / 255.0,
				    ((colour & 0x00ff0000) >> 16) / 255.0,
				    ((colour & 0x0000ff00) >>  8) / 255.0,
				    ((colour & 0x000000ff) >>  0) / 255.0);
			glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, NULL);
		}
	}
}

static int
chip8_init(const char *filename)
{
	FILE *fd;
	size_t fsize, nread, ntotal;
	
	fd = fopen(filename, "r");
	if (!fd) {
		perror("fopen: ");
		return -1;
	}

	memset(&chip8, 0, sizeof(chip8));
	chip8.pc = CHIP8_RAM_ENTRYPOINT;
	chip8.delay = 0;
	chip8.sound = 0;
	chip8.I = 0;
	chip8.sp = 0;

	for (int i = 0; i < 0x50; i++)
		chip8.ram[i] = chip8_numbers[i];

	fseek(fd, 0L, SEEK_END);
	fsize = ftell(fd);
	fseek(fd, 0L, SEEK_SET);


	if (fsize > CHIP8_RAM_CAPACITY - CHIP8_RAM_ENTRYPOINT) {
		fprintf(stderr, "Fatal: This rom is too "
			"big to fit in memory!\n");
		exit(EXIT_FAILURE);
	}

	ntotal = 0;
	while ((nread = fread(chip8.ram + chip8.pc,
			      sizeof(*chip8.ram), fsize, fd)) > 0) {
		ntotal += nread;
	}

	if (ntotal != fsize) {
		fprintf(stderr, "Fatal: Failed to read "
			"rom into memory!\n");
		exit(EXIT_FAILURE);
	}

	fclose(fd);
	return 0;
}

static int
window_init(void)
{
	SDL_Init(SDL_INIT_VIDEO);
	window = SDL_CreateWindow("Chip 8 Emulator",
				  SDL_WINDOWPOS_UNDEFINED,
				  SDL_WINDOWPOS_UNDEFINED,
				  WINDOW_WIDTH,
				  WINDOW_HEIGHT,
				  SDL_WINDOW_OPENGL);
	context = SDL_GL_CreateContext(window);
	glewInit();

	glViewport(0.0, 0.0, WINDOW_WIDTH, WINDOW_HEIGHT);

	ll_matrix_mode(LL_MATRIX_MODEL);
	ll_matrix_identity();
	
	ll_matrix_mode(LL_MATRIX_PROJECTION);
	ll_matrix_orthographic(0.0, WINDOW_WIDTH, WINDOW_HEIGHT, 0.0, 10.0, -10.0);

	const char *vsource = "#version 330 core\n"
		"uniform mat4 model;"
		"uniform mat4 projection;"
		"layout (location = 0) in vec2 vertex;"
		"void main()"
		"{"
		"gl_Position = projection * model * vec4(vertex, 0.0, 1.0);"
		"}";
	
	const char *fsource = "#version 330 core\n"
		"uniform vec4 colour;"
		"void main()"
		"{"
		"gl_FragColor = colour;"
		"}";

	GLuint vshader, fshader;
	vshader = glCreateShader(GL_VERTEX_SHADER);
	glShaderSource(vshader, 1, (const GLchar **) &vsource, NULL);
	glCompileShader(vshader);
	
	fshader = glCreateShader(GL_FRAGMENT_SHADER);
	glShaderSource(fshader, 1, (const GLchar **) &fsource, NULL);
	glCompileShader(fshader);

	shader = glCreateProgram();
	glAttachShader(shader, vshader);
	glAttachShader(shader, fshader);
	glLinkProgram(shader);

	glDeleteShader(vshader);
	glDeleteShader(fshader);

	glUseProgram(shader);

	const vec2_t vertices[4] = {
		{{ 0.0, 0.0 }},
		{{ 1.0, 0.0 }},
		{{ 1.0, 1.0 }},
		{{ 0.0, 1.0 }},
	};

	const GLuint indices[6] = {
		0, 1, 2,
		2, 3, 0
	};

	glGenVertexArrays(1, buffers+BUFFER_VAO);
	glBindVertexArray(buffers[BUFFER_VAO]);
	glGenBuffers(2, buffers);
	glBindBuffer(GL_ARRAY_BUFFER, buffers[BUFFER_VBO]);
	glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices,
		     GL_STATIC_DRAW);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, buffers[BUFFER_EBO]);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices,
		     GL_STATIC_DRAW);

	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, 0);
	glEnableVertexAttribArray(0);

	ll_matrix_mode(LL_MATRIX_PROJECTION);
	glUniformMatrix4fv(glGetUniformLocation(shader, "projection"), 1, GL_FALSE,
			   ll_matrix_get_copy().data);
	return 0;
}

static void
window_free(void)
{
	SDL_DestroyWindow(window);
	SDL_GL_DeleteContext(context);
	glDeleteVertexArrays(1, buffers+BUFFER_VAO);
	glDeleteBuffers(2, buffers);
	glDeleteProgram(shader);
	SDL_Quit();
}


