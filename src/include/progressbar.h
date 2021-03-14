#include <stdio.h>
#include <math.h>
#include <string.h>

#define PROGRESSBAR_LENGHT 20
#define SPIRAL_LENGHT 20
#define DEFAULT_STYLE "="

#define progressbar_construct(...) progressbar_construct_wrapper((construct_args){__VA_ARGS__});
#define progressbar_create(...) progressbar_create_wrapper((create_args){__VA_ARGS__});
#define progressbar_update(...) progressbar_update_wrapper((update_args){__VA_ARGS__});
#define progressbar_print(...) progressbar_print_wrapper((print_args){__VA_ARGS__});
#define progressbar_done(...) progressbar_done_wrapper((done_args){__VA_ARGS__});

typedef struct {
	int ready;
	int start;
	int end;
	int current;
	char style[4];
} progressbar_t;

typedef struct {
	progressbar_t *pbar;
	int start;
	int end;
	char style[4];
} create_args;

typedef struct {
	progressbar_t *pbar;
	int step;
} update_args;

typedef struct {
	progressbar_t *pbar;
	char *additional;
} print_args;

typedef struct {
	progressbar_t *pbar;
	char *additional;
} done_args;

void progressbar_create_base (progressbar_t *pbar, int start, int end, char *style);
void progressbar_create_wrapper (create_args a);
void progressbar_update_base (progressbar_t *pbar, int step);
void progressbar_update_wrapper (update_args a);
void progressbar_seek(progressbar_t *pbar, int index);
void progressbar_start(progressbar_t *pbar);
void progressbar_end(progressbar_t *pbar);
void nothing();
void delay(int milisecond);
void progressbar_print_base (progressbar_t *pbar, char *additional);
void progressbar_print_wrapper (print_args a);
void progressbar_spiral (progressbar_t *pbar);
void progressbar_done_base (progressbar_t *pbar, char *additional);
void progressbar_done_wrapper (done_args a);

void progressbar_create_base (progressbar_t *pbar, int start, int end, char *style) {
	pbar->ready = 1;
	pbar->start = start;
	pbar->end = end;
	pbar->current = start;
	strcpy(pbar->style, style);
}

void progressbar_create_wrapper (create_args a) {
	int a_start = a.start ? a.start : 0;
	int a_end = a.end ? a.end : 100;
	char a_style[4];
	if (strcmp(a.style, "") == 0) strcpy(a_style, DEFAULT_STYLE); else strcpy(a_style, a.style);
	progressbar_create_base(a.pbar, a_start, a_end, a_style);
}

void progressbar_update_base (progressbar_t *pbar, int step) {
	pbar->current += step;
}

void progressbar_update_wrapper (update_args a) {
	int a_step = a.step ? a.step : 1;
	progressbar_update_base(a.pbar, a_step);
}

void progressbar_seek(progressbar_t *pbar, int index) {
	pbar->current = index;
}

void progressbar_start(progressbar_t *pbar) {
	pbar->current = pbar->start;
}

void progressbar_end(progressbar_t *pbar) {
	pbar->current = pbar->end;
}

void nothing() {/*do nothing*/}

void delay(int milisecond) { 
    clock_t start_time = clock(); 
    while (clock() < start_time + milisecond) 
        ;
} 

void progressbar_print_base (progressbar_t *pbar, char *additional) {
	double percent = ((double)(pbar->current - pbar->start) / (double)(pbar->end - pbar->start)) * 100;
	int c1_idx = ((int) floor(percent) / (100 / PROGRESSBAR_LENGHT));
	int c2_idx = PROGRESSBAR_LENGHT - c1_idx;

	printf("\t%4.2f%s\t[", percent, "%");

	for (int i = 0; i < c1_idx; i++) printf("%s", pbar->style);
	for (int i = 0; i < c2_idx; i++) printf("%s", " ");

	printf("] ");

	if (additional != 0) printf("- %s", additional);

	if (percent == 100.00) printf("\n"); else printf("\r");
}

void progressbar_print_wrapper (print_args a) {
	progressbar_print_base(a.pbar, a.additional);
}

void progressbar_spiral (progressbar_t *pbar) {
	double percent = ((double)(pbar->current - pbar->start) / (double)(pbar->end - pbar->start)) * 100;
	int idx = ((int) floor(percent) / (100 / SPIRAL_LENGHT));
	int p = idx % 4;
	switch (p) {
		case 0:
			printf("%s\r", "|");
			break;
		case 1:
			printf("%s\r", "/");
			break;
		case 2:
			printf("%s\r", "-");
			break;
		case 3:
			printf("%s\r", "\\");
			break;
	}
}

void progressbar_done_base (progressbar_t *pbar, char *additional) {
	progressbar_end(pbar);
	progressbar_print_base(pbar, additional);
}

void progressbar_done_wrapper (done_args a) {
	progressbar_done_base(a.pbar, a.additional);
}