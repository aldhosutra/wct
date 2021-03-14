// #pragma GCC diagnostic ignored "-Wint-conversion"
// #pragma GCC diagnostic ignored "-Wincompatible-pointer-types"
// #pragma GCC diagnostic ignored "-Wdiscarded-qualifiers"

// Library Preprocessor
// #include <omp.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <time.h>
#include <dirent.h>
#include <errno.h>
#include <math.h>
#include <jansson.h>
#include <curl/curl.h>
#include "include/smtp.c"
#include "include/ini.c"
#include "include/log.c"
#include "include/vector.h"
#include "include/table.h"
#include "include/progressbar.h"
#include "include/EmbeddableWebServer.h"

// enum Declaration
enum {
    NUM_OPEN = 0,
    NUM_HIGH = 1,
    NUM_LOW = 2,
    NUM_CLOSE = 3
};

enum {
    CUSTOM = 0,
    CLASSIFICATION = 1,
    REGRESSION = 2,
    RISEFALL = 3
};

enum {
    AUTOLABEL = -1
};

enum {
    RANDOM_INIT = 0,
    POINTONE_INIT = 1
};

enum {
    NO_AVERAGING_MODE = 0,
    AVERAGING_RANGE_MODE = 1
};

enum {
    DF_UNDEFINED = -1,
    DF_SEQUENCE = 0,
    DF_SEQUENCE_SPLITTED = 1,
    DF_TABLE = 2,
    DF_TABLE_SPLITTED = 3
};

// typedef Declaration
struct write_result {
    char *data;
    int pos;
};

typedef struct {
	ivec_t tm;
	dvec_t open;
	dvec_t high;
	dvec_t low;
	dvec_t close;
} ohlc_t;

typedef struct {
	// Config Status
	int ready;

	// IQ Option Section
	char iq_user[32];
	char iq_pass[32];

	// Email Config Section
	char email_smtp[32];
	int email_port;
	char email_user[32];
	char email_pass[32];
	char email_sendto[32];

	// Default Section
	char resdir[32];
	char logdir[32];
	char coredir[32];
	char rawdir[32];
	char legacydir[32];

	int log;
    double lot;

	char default_assetcode[32];
	char default_core[32];
	int default_label_maxlen;
	int default_timeseries_shift;
	int default_range_step;
	int default_range_start;
	int default_range_end;
	int default_features_len;
	double default_similarity;
	int default_min_pattern_found;
	double default_min_prediction_rate;
	double default_learning_rate;
	double default_discount_rate;
} cfg_t;

typedef struct {
    int ready;

    int type;
    int major;
    int minor;
    int patch;
    int num_optimize;
    int num_trained;
    double last_wr;
    long int last_train;
    char core[64];
    int label_maxlen;
    int islabelfitted;
    int islabelencoded;
    char asset_code[10];
    int timeseries_shift;
    int range_step;
    int range_start;
    int range_end;
    int features_len;
    double similarity;
    int min_pattern_found;
    double min_prediction_rate;
} meta_t;

typedef struct {
    ivec_t index;
    ivec_t value;
} dynlabel_t;

typedef struct {
    dvec_t weight;
    dtab_t features;
    dtab_t label;
    meta_t meta;
    dynlabel_t dynlabel;
    dvec_t featureindex;
} wct_t;

typedef struct {
    dvec_t seq;
    dvec_t seq_test;
    dtab_t x;
    dtab_t y;
    dtab_t x_test;
    dtab_t y_test;
} dataframe_t;

typedef struct {
    int count;
    ivec_t index;
    dvec_t kernel;
} metrics_node_t;

typedef struct {
    int class;
    int label_col;
    double (*error_function)();
    double (*acc_function)();
    double (*label_return)();
    void (*gradient_operation)();
    int (*better_acc_condition)();
} metrics_t_misc;

typedef struct {
    metrics_node_t node;
    metrics_t_misc misc;
    dvec_t value;
    double true_label;
    double acc;
    double error;
} metrics_t;

// GLOBAL Variable Declaration
static struct {
    wct_t wct;
    cfg_t cfg;
    int ready;
} GLOBAL;

static struct Server server = {0};
static int from_wct_server = 0;

// Function Prototype
void char_prepend(char* s, const char* t);
char *config_get (char *inidir, char section[], char key[]);
void config_init(cfg_t *cfgobj);
void log_init_fp();
void wct_load (wct_t *wct, char *core_dir);

// Platform Specific Header
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wimplicit-function-declaration"

    #ifdef _WIN32
        #include <windows.h>
        #include <conio.h>
        #define mkdir(dir, mode) _mkdir(dir)
        #define rmdir(dir) _rmdir(dir)
        int _mkdir(const char *dirname);
        int _rmdir(const char *dirname);
        #pragma comment(lib, "ws2_32")

        void gettimeofday(struct timeval* tv, const void* unusedTimezone) {
            ULONGLONG ticks = GetTickCount64();
            tv->tv_sec = ticks / 1000;
            tv->tv_usec = ticks % 1000;
        }
    #endif

    #ifdef linux
        #include <termios.h>
        #include <unistd.h>
        #include <sys/time.h>

        int getch (void) {
            int ch;
            struct termios oldt, newt;

            tcgetattr(STDIN_FILENO, &oldt);
            newt = oldt;
            newt.c_lflag &= ~(ICANON|ECHO);
            tcsetattr(STDIN_FILENO, TCSANOW, &newt);
            ch = getchar();
            tcsetattr(STDIN_FILENO, TCSANOW, &oldt);

            return ch; }
    #endif

#pragma GCC diagnostic pop

// Error Code Definition
#define WCT_OK 					0
#define WCT_ERROR 				1
#define WCT_INVALID_ARGUMENT 	2
#define WCT_INVALID_STRUCT 		3
#define WCT_INVALID_CONFIG 		4
#define WCT_INVALID_DATA 		5
#define WCT_NOT_READY 			6
#define WCT_NOT_FOUND 			7
#define WCT_NEED_EMPTY 			8
#define WCT_SMALL_DATA 			9
#define WCT_OUT_OF_BOUNDS 		10
#define WCT_MAIL_ERROR			11

// Constant Preprocessor
#define CONFIGDIR               "wctengine.ini"
#define RESDIR                  (config_get(CONFIGDIR, "default", "resdir"))
#define LOGDIR                  (config_get(CONFIGDIR, "default", "logdir"))
#define COREDIR                 (config_get(CONFIGDIR, "default", "coredir"))
#define RAWDIR                  (config_get(CONFIGDIR, "default", "rawdir"))
#define LEGACYDIR               (config_get(CONFIGDIR, "default", "legacydir"))

#define DEFAULT_DISTANCE_FUNCTION       wct_manhattan_distance
#define DEFAULT_ESTIMATION_FUNCTION     wct_risefall
#define DEFAULT_ACTIVATION_FUNCTION     sigmoid
#define DEFAULT_ESTIMATION_CONDITION    by_minimal_similarity

// fnv1a64 (Fowler–Noll–Vo) Hash Function (Lower-Case String, ex: 'label')
#define LABELFILE "39f7fcec8fcb623d.wctx"
#define DYNLABELFILE "520d09de7323690e.wctx"
#define METAFILE "4320e9a2e32eac38.wctx"
#define WEIGHTFILE "6911f8de1f27bf19.wctx"
#define FEATURESFILE "be45a3a6ee3c626a.wctx"
#define FEATUREINDEXFILE "d11ff72db3507d23.wctx"

#define wct_create_from_sequence(...) wct_cfs_wrapper((wct_cfs_args){__VA_ARGS__})
#define wct_estimate(...) wct_estimate_wrapper((estimate_args){__VA_ARGS__})
#define wct_evaluate(...) wct_evaluate_wrapper((evaluate_args){__VA_ARGS__})
#define wct_label_encode(...) wct_label_encode_wrapper((label_encode_args){__VA_ARGS__})
#define wct_discriminator(...) wct_discriminator_wrapper((discriminator_args){__VA_ARGS__})
#define wct_test(...) wct_test_wrapper((test_args){__VA_ARGS__})
#define wct_fit(...) wct_fit_wrapper((fit_args){__VA_ARGS__})
#define wct_feed(...) wct_feed_wrapper((feed_args){__VA_ARGS__})
#define wct_dynlabel_fit(...) wct_dynlabel_fit_wrapper((dynlabel_fit_args){__VA_ARGS__})
#define wct_dynlabel_get(...) wct_dynlabel_get_wrapper((dynlabel_get_args){__VA_ARGS__})
#define wct_forecast(...) wct_forecast_wrapper((forecast_args){__VA_ARGS__})
#define wcthelper_get_test_sequence_label(...) wcthelper_get_test_sequence_label_wrapper((get_test_label_args){__VA_ARGS__})
#define wct_append(...) wct_append_wrapper((append_args){__VA_ARGS__})

// TIMER MACRO
#define DECLARE_TIMER(s)       clock_t timeStart_##s, timeDiff_##s; double timeTotal_##s = 0; int lapCount_##s = 0
#define START_TIMER(s)         timeStart_##s = clock()
#define STOP_TIMER(s)          timeDiff_##s = (double)(clock() - timeStart_##s); timeTotal_##s += timeDiff_##s; lapCount_##s++
#define GET_TIMER(s)           (double)((double)timeTotal_##s / (double)CLOCKS_PER_SEC)
#define GET_AVERAGE_TIMER(s)   (double)(lapCount_##s ? timeTotal_##s / ((double)lapCount_##s * (double)CLOCKS_PER_SEC) : 0)
#define CLEAR_AVERAGE_TIMER(s) timeTotal_##s = 0; lapCount_##s = 0

// Macro Preprocessor
#define SLICE(x, y, start, end) _Generic(x, char *: slice_c(x, y, start, end), double *: slice_d(x, y, start, end), int *: slice_i(x, y, start, end))
#define TYPECMP(x, type) _Generic(x, type: true, default: false)
#define LEN(array) (sizeof(array) / sizeof(array[0]))
#define AVG(array) _Generic(array, double *: AVG_d(array, LEN(array)), int *: AVG_i(array, LEN(array)))
#define SUM(array) _Generic(array, double *: SUM_d(array, LEN(array)), int *: SUM_i(array, LEN(array)))
#define CSV(filename, delimiter, array) (csv_parser(filename, delimiter, LEN(array), LEN(array[0]), array))
#define WRITE(filename, array) (csv_writer(filename, LEN(array), LEN(array[0]), array))
#define SORT(array) _Generic(array, double (*)[]: qsort(array, LEN(array), sizeof(*array), cmp2d_d), 	\
									double *: qsort(array, LEN(array), sizeof(*array), cmp2d_d), 		\
									int (*)[]: qsort(array, LEN(array), sizeof(*array), cmp2d_i), 		\
									int *: qsort(array, LEN(array), sizeof(*array), cmp2d_i))
#define BISEC2D(array, element) _Generic(array, double (*)[]: bisec2d_d(0, LEN(array), LEN(array[0]), element, array), 	\
												int (*)[]: bisec2d_i(0, LEN(array), LEN(array[0]), element, array))
#define BISEC(array, element) _Generic(array, 	int *: bisec_i(0, LEN(array), element, array),	 						\
												double *: bisec_d(0, LEN(array), element, array))
#define INDEX2D(array, element) _Generic(array, double (*)[]: linear_search2d_d(LEN(array), LEN(array[0]), element, array),  \
												int (*)[]: linear_search2d_i(LEN(array), LEN(array[0]), element, array))
#define INDEX(array, element) _Generic(array,   int *: linear_search_i(LEN(array), element, array),						  \
												double *: linear_search_d(LEN(array), element, array))
#define STRFND(array, element) (find_string(LEN(array), LEN(array[0]), strlen(element), array, element))
#define MAX_ARR(array) _Generic(array,   int *: max_i(array, LEN(array)), double *: max_d(array, LEN(array)))
#define MIN_ARR(array) _Generic(array,   int *: min_i(array, LEN(array)), double *: min_d(array, LEN(array)))

/*#define TYPENAME(x) _Generic((x),												  \
		_Bool: "_Bool",				  unsigned char: "unsigned char",			  \
		 char: "char",					 signed char: "signed char",			  \
	short int: "short int",		 unsigned short int: "unsigned short int",		  \
		  int: "int",					 unsigned int: "unsigned int",			  \
	 long int: "long int",		   unsigned long int: "unsigned long int",		  \
long long int: "long long int", unsigned long long int: "unsigned long long int", \
		float: "float",						 double: "double",					  \
  long double: "long double",				   char *: "pointer to char",		  \
	   void *: "pointer to void",				int *: "pointer to int",		  \
	 double *: "pointer to double",			default: "other")*/

enum {
	TYPE_BOOL = 0,					TYPE_UNSIGNED_CHAR = 1,
	TYPE_CHAR = 2,					TYPE_SIGNED_CHAR = 3,
	TYPE_SHORT_INT = 4,				TYPE_UNSIGNED_SHORT_INT = 5,
	TYPE_INT = 6,					TYPE_UNSIGNED_INT = 7,
	TYPE_LONG_INT = 8,				TYPE_UNSIGNED_LONG_INT = 9,
	TYPE_LONG_LONG_INT = 10,		TYPE_UNSIGNED_LONG_LONG_INT = 11,
	TYPE_FLOAT = 12,				TYPE_DOUBLE = 13,
	TYPE_LONG_DOUBLE = 14,			TYPE_POINTER_TO_CHAR = 15,
	TYPE_POINTER_TO_VOID = 16,		TYPE_POINTER_TO_INT = 17,
	TYPE_POINTER_TO_DOUBLE = 18,	TYPE_OTHER = 19
};

#define TYPENAME(x) _Generic((x),                                           	        \
        _Bool: TYPE_BOOL,                   unsigned char: TYPE_UNSIGNED_CHAR,          \
         char: TYPE_CHAR,                     signed char: TYPE_SIGNED_CHAR,            \
    short int: TYPE_SHORT_INT,         unsigned short int: TYPE_UNSIGNED_SHORT_INT,     \
          int: TYPE_INT,                     unsigned int: TYPE_UNSIGNED_INT,           \
     long int: TYPE_LONG_INT,           unsigned long int: TYPE_UNSIGNED_LONG_INT,      \
long long int: TYPE_LONG_LONG_INT, unsigned long long int: TYPE_UNSIGNED_LONG_LONG_INT, \
        float: TYPE_FLOAT,                         double: TYPE_DOUBLE,                 \
  long double: TYPE_LONG_DOUBLE,                   char *: TYPE_POINTER_TO_CHAR,        \
       void *: TYPE_POINTER_TO_VOID,                int *: TYPE_POINTER_TO_INT,         \
     double *: TYPE_POINTER_TO_DOUBLE,            default: TYPE_OTHER)


// Constructor & Deconstructor
void wct_constructor (void) __attribute__ ((constructor)); 
void wct_destructor (void) __attribute__ ((destructor)); 
DECLARE_TIMER(global_timer);

void wct_exit(int error_code) {
	printf("[Program Terminated With Exit Code: %d]\n", error_code);
	exit(error_code);
}

void wct_constructor (void) { 
	START_TIMER(global_timer);
    config_init(&GLOBAL.cfg);
    if (GLOBAL.cfg.log == 1) log_init_fp();
    srand(time(NULL));

    printf("libwct v1.0.0\n");
    printf("Weighted Cognition Table Library\n");
    printf("(c) 2019 - Aldo Suhartono Putra, All Right Reserved\n\n");

    if (GLOBAL.cfg.default_core != "0") {
	    char core_dir[64];
	    char core_name[32];
	    strcpy(core_name, GLOBAL.cfg.default_core);
	    strcpy(core_dir, core_name);
	    char_prepend(core_dir, COREDIR);

	    DIR* dir = opendir(core_dir);
	    if (dir) {
	        wct_load(&GLOBAL.wct, core_name);
	        GLOBAL.ready = 1;
	        closedir(dir);
	    } else if (ENOENT == errno) {
	        log_error("wct_constructor: WCT-Core in wctengine.ini Doesn't Exist\n");
	    } else {
	        log_error("wct_constructor: Opendir() Failed For Some Reason!\n");
	        wct_exit(WCT_ERROR);
	    }
	}
}

void wct_destructor (void) { 
    STOP_TIMER(global_timer);
	printf("\n[Programs Execution Took: %f Seconds]\n", GET_TIMER(global_timer));
}

int wct_mail(char *subject, char *contents) {
	cfg_t cfg = GLOBAL.cfg;
	if ((strcmp(cfg.email_smtp, "0") != 0) && (cfg.email_port != 0) && (strcmp(cfg.email_user, "0") != 0) && (strcmp(cfg.email_pass, "0") != 0) && (strcmp(cfg.email_sendto, "0") != 0)) {
		struct smtp *smtp;
		int rc;

		char server[32];
		char user[32];
		char pass[32];
		char port[8];
		char sendto[32];
		strcpy(server, cfg.email_smtp);
		strcpy(user, cfg.email_user);
		strcpy(pass, cfg.email_pass);
		sprintf(port, "%d", cfg.email_port);
		strcpy(sendto, cfg.email_sendto);

		log_info("Sending Email To: %s", sendto);

		rc = smtp_open(server, port, SMTP_SECURITY_STARTTLS, SMTP_DEBUG | SMTP_NO_CERT_VERIFY, NULL, &smtp);
		rc = smtp_auth(smtp, SMTP_AUTH_PLAIN, user, pass);
		rc = smtp_address_add(smtp, SMTP_ADDRESS_FROM, user, user);
		rc = smtp_address_add(smtp, SMTP_ADDRESS_TO, sendto, sendto);
		rc = smtp_header_add(smtp, "Subject", subject);
		rc = smtp_mail(smtp, contents);
		rc = smtp_close(smtp);

		if(rc != SMTP_STATUS_OK) {
			log_error("SMTP failed: %s\n", smtp_status_code_errstr(rc));
			return WCT_MAIL_ERROR;
		}

		log_info("Email Successfully Sent!");
		return WCT_OK;
	} else return WCT_INVALID_CONFIG;
}

static size_t write_response(void *ptr, size_t size, size_t nmemb, void *stream) {
    struct write_result *result = (struct write_result *)stream;
    if(result->pos + size * nmemb >= 1024 - 1) {
        log_error("too small buffer for storing web response");
        return 0;
    }

    memcpy(result->data + result->pos, ptr, size * nmemb);
    result->pos += size * nmemb;
    return size * nmemb;
}

static char *request(const char *url) {
    CURL *curl = NULL;
    CURLcode status;
    struct curl_slist *headers = NULL;
    char *data = NULL;
    long code;

    curl_global_init(CURL_GLOBAL_ALL);
    curl = curl_easy_init();
    if(!curl)
        goto error;

    data = malloc(1024);
    if(!data)
        goto error;

    struct write_result write_result = {
        .data = data,
        .pos = 0
    };

    curl_easy_setopt(curl, CURLOPT_URL, url);

    headers = curl_slist_append(headers, "User-Agent: Weighted-Cognition-Table-v1.0");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_response);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &write_result);

    status = curl_easy_perform(curl);
    if(status != 0)
    {
        log_error("unable to request data from %s:\n", url);
        log_error("%s\n", curl_easy_strerror(status));
        goto error;
    }

    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &code);
    if(code >= 500) {
        log_error("server responded with code %ld\n", code);
        goto error;
    }
    else if(code > 200) log_warn("server responded with non-200 code: %ld\n", code);

    curl_easy_cleanup(curl);
    curl_slist_free_all(headers);
    curl_global_cleanup();

    data[write_result.pos] = '\0';

    return data;

error:
    if(data)
        free(data);
    if(curl)
        curl_easy_cleanup(curl);
    if(headers)
        curl_slist_free_all(headers);
    curl_global_cleanup();
    return NULL;
}

// Function Declaration
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wint-conversion"
#pragma GCC diagnostic ignored "-Wincompatible-pointer-types"

	// AVG Macro Helper
	double AVG_i(int array[], int lenght) {
		int i;
		double ret = 0.0;
		for (i = 0; i < lenght; i++) {
			ret += (double)*(array + i);
		}
		return ret / lenght;
	}

	double AVG_d(double array[], int lenght) {
		int i;
		double ret = 0.0;
		for (i = 0; i < lenght; i++) {
			ret += *(array + i);
		}
		return ret / lenght;
	}

	// SUM Macro Helper
	int SUM_i(int array[], int lenght) {
		int i;
		int ret = 0;
		for (i = 0; i < lenght; i++) {
			ret += *(array + i);
		}
		return ret;
	}

	double SUM_d(double array[], int lenght) {
		int i;
		double ret = 0.0;
		for (i = 0; i < lenght; i++) {
			ret += *(array + i);
		}
		return ret;
	}

	// SORT Macro Helper - Compare Function
	int cmp2d_i(const void *a, const void *b) {
		int x1 = *(const int*)a;
		int x2 = *(const int*)b;
		if (x1 > x2) return  1;
		if (x1 < x2) return -1;
		// x1 and x2 are equal; compare y's
		int y1 = *(((const int*)a)+1);
		int y2 = *(((const int*)b)+1);
		if (y1 > y2) return  1;
		if (y1 < y2) return -1;
		return 0;
	}

	int cmp2d_d(const void *a, const void *b) {
		double x1 = *(const double*)a;
		double x2 = *(const double*)b;
		if (x1 > x2) return  1;
		if (x1 < x2) return -1;
		// x1 and x2 are equal; compare y's
		double y1 = *(((const double*)a)+1);
		double y2 = *(((const double*)b)+1);
		if (y1 > y2) return  1;
		if (y1 < y2) return -1;
		return 0;
	}

	// MAX and MIN Macro Helper
	double max_d(double *array, int arraySize) {
		double ret = array[0];
		for (int i = 0; i < arraySize; i++) {
			if (array[i] > ret) {
				ret = array[i];
			}
		}
		return ret;
	}

	double min_d(double *array, int arraySize) {
		double ret = array[0];
		for (int i = 0; i < arraySize; i++) {
			if (array[i] < ret) {
				ret = array[i];
			}
		}
		return ret;
	}

	int max_i(int *array, int arraySize) {
		int ret = array[0];
		for (int i = 0; i < arraySize; i++) {
			if (array[i] > ret) {
				ret = array[i];
			}
		}
		return ret;
	}

	int min_i(int *array, int arraySize) {
		int ret = array[0];
		for (int i = 0; i < arraySize; i++) {
			if (array[i] < ret) {
				ret = array[i];
			}
		}
		return ret;
	}

	// slice_i(int); slice_d(double) and slice_c(string) need destination variable as argument
	void slice_i (int *source, int *dest, int startpoint, int endpoint) {
		memcpy(dest, source + startpoint, (endpoint - startpoint + 1) * sizeof(*source));
	}

	void slice_d (double *source, double *dest, int startpoint, int endpoint) {
		memcpy(dest, source + startpoint, (endpoint - startpoint + 1) * sizeof(*source));
	}

	void slice_c (char *source, char *dest, int startpoint, int endpoint) {
		memcpy(dest, source + startpoint, (endpoint - startpoint + 2) * sizeof(*source));
		dest[endpoint - startpoint + 1] = '\0';
	}

	// [DEPRECATED] slice_ip(int) and slice_dp(double) return memory address (pointer)
	/*int slice_ip (int *source, int startpoint, int endpoint) {
		static int dest[10];
		int ret = memcpy(dest, source + startpoint, (endpoint - startpoint + 1) * sizeof(*source));
		return dest;
	}

	int slice_dp (double *source, int startpoint, int endpoint) {
		static double dest[10];
		int ret = memcpy(dest, source + startpoint, (endpoint - startpoint + 1) * sizeof(*source));
		return dest;
	}*/

#pragma GCC diagnostic pop

/**
 * Cross-platform sleep function for C
 * @param int milliseconds
 */
void sleep_ms(int milliseconds) {
    #ifdef WIN32
        Sleep(milliseconds);
    #elif _POSIX_C_SOURCE >= 199309L
        struct timespec ts;
        ts.tv_sec = milliseconds / 1000;
        ts.tv_nsec = (milliseconds % 1000) * 1000000;
        nanosleep(&ts, NULL);
    #else
        usleep(milliseconds * 1000);
    #endif
}

double math_percentchange(double startPoint, double currentPoint) {
	if (startPoint != 0) {
		return ((currentPoint - startPoint) / fabs(startPoint)) * 100.00;
	} else {
		return 0.000000001;
	}
}

static inline double math_exp (double x) {
  x = 1.0 + x / 256.0;
  x *= x; x *= x; x *= x; x *= x;
  x *= x; x *= x; x *= x; x *= x;
  return x;
}

double math_manhattan_distance (double *p, double *q, int len) {
    double s = 0.0;
    for (int i = 0; i < len; i++) {
        s += fabs(p[i] - q[i]);
    }
    return s;
}

double math_euclidean_distance (double *p, double *q, int len) {
    double s = 0.0;
    for (int i = 0; i < len; i++) {
        s += pow(p[i] - q[i], 2);
    }
    return sqrt(s);
}

static inline double math_sigmoid (double x) {
    return 1 / (1 + math_exp(-x));
}

double math_cross_entropy (double *x, double *y, int len) {
    double sum = 0;
    double logs = 0.0;
    for (int i = 0; i < len; i++) {
        logs = (x[i] > 0) ? log(x[i]) : 0;
        sum += (logs * y[i]);
    }
    return -1.0 * sum;
}

double math_mean_squared_error (double x, double y) {
    return (pow(y - x, 2));
}

// NOTE: Before calling random_d(), make sure to call srand(time(NULL));
double random_d(double min, double max) {
    double range = (max - min); 
    double div = RAND_MAX / range;
    return min + (rand() / div);
}

// NOTE: Before calling random_i(), make sure to call srand(time(NULL));
int random_i(int min, int max) {
    int range = (max - min); 
    int div = RAND_MAX / range;
    return min + (rand() / div);
}

void file_insert(char str[], char filename[]) {
	int i;
	FILE * fptr;
	fptr = fopen(filename, "a"); // "a" defines "append mode"
	for (i = 0; str[i] != '\0'; i++) {
		fputc(str[i], fptr);
	}
	fclose(fptr);
}

int file_replace(char filename[], int delete_line, char *data) {
	FILE *fileptr1, *fileptr2;
	char c;
	int temp = 1;
 
	fileptr1 = fopen(filename, "r");
	if (fileptr1 == NULL) return 0;
	c = getc(fileptr1);
	rewind(fileptr1);
	fileptr2 = fopen("temp.c", "w");
	c = getc(fileptr1);

	while (c != EOF)
	{
		if (c == '\n') temp++;
		if (temp != delete_line) { putc(c, fileptr2); } else {
			while ((c = getc(fileptr1)) != '\n') {}
			if (delete_line != 1) putc('\n', fileptr2);
			fputs(data, fileptr2);
			putc('\n', fileptr2);
			temp++;
		}

		c = getc(fileptr1);
	}

	fclose(fileptr1);
	fclose(fileptr2);
	remove(filename);
	rename("temp.c", filename);
	
	return 1;
}

int file_linecount(char filename[]) {
	FILE *fp;
	int count = 1;
	char c;

	fp = fopen(filename, "r");
	if (fp == NULL) return -1;

	for (c = getc(fp); c != EOF; c = getc(fp))
		if (c == '\n')
			count = count + 1;

	fclose(fp);
	return count;
}

long int file_size(char *file_name) { 
    FILE* fp = fopen(file_name, "r"); 
    if (fp == NULL) { 
        printf("File Not Found!\n"); 
        return -1; 
    } 
    fseek(fp, 0L, SEEK_END); 
    long int res = ftell(fp); 
    fclose(fp); 
    return res; 
} 

int linear_search_d(int lenght, double element, double sorted_list[]) {
	int i;
	for (i = 0; i < lenght; i++) {
		if (sorted_list[i] == element) return i;
	}
	return -1;
}

int linear_search_i(int lenght, int element, int sorted_list[]) {
	int i;
	for (i = 0; i < lenght; i++) {
		if (sorted_list[i] == element) return i;
	}
	return -1;
}

int linear_search2d_d(int lenght, int lenght_2d, double element[], double sorted_list[lenght][lenght_2d]) {
	int i;
	for (i = 0; i < lenght; i ++) {
		if (sorted_list[i][0] == element[0] && sorted_list[i][1] == element[1]) return i;
	}
	return -1;
}

int linear_search2d_i(int lenght, int lenght_2d, int element[], int sorted_list[lenght][lenght_2d]) {
	int i;
	for (i = 0; i < lenght; i ++) {
		if (sorted_list[i][0] == element[0] && sorted_list[i][1] == element[1]) return i;
	}
	return -1;
}

int bisec_d(int low, int high, double element, double sorted_list[]) {
	int middle;
	while (low <= high) {
		middle = low + (high - low)/2;
		if (element > sorted_list[middle])
			low = middle + 1;
		else if (element < sorted_list[middle])
			high = middle - 1;
		else
			return middle;
	}
	return -1;
}

int bisec_i(int low, int high, int element, int sorted_list[]) {
	int middle;
	while (low <= high) {
		middle = low + (high - low)/2;
		if (element > sorted_list[middle])
			low = middle + 1;
		else if (element < sorted_list[middle])
			high = middle - 1;
		else
			return middle;
	}
	return -1;
}

int bisec2d_d(int low, int high, int lenght_2d, double element[], double sorted_list[][lenght_2d]) {
	int middle;
	while (low <= high) {
		middle = low + (high - low)/2;
		if (element[0] > sorted_list[middle][0]) {
			low = middle + 1;
		} else if (element[0] < sorted_list[middle][0]) {
			high = middle - 1;
		} else {
			if (element[1] == sorted_list[middle][1])
				return middle;
		}
	}
	return -1;
}

int bisec2d_i(int low, int high, int lenght_2d, int element[], int sorted_list[][lenght_2d]) {
	int middle;
	while (low <= high) {
		middle = low + (high - low)/2;
		if (element[0] > sorted_list[middle][0]) {
			low = middle + 1;
		} else if (element[0] < sorted_list[middle][0]) {
			high = middle - 1;
		} else {
			if (element[1] == sorted_list[middle][1])
				return middle;
		}
	}
	return -1;
}

int unique_d (double *array, int len) {
    dvec_t temp;
    dvec_init(&temp);
    for (int i = 0; i < len; i++) {
        if (dvec_search(&temp, array[i]) == -1) dvec_append(&temp, array[i]);
    }
    int ret = temp.size;
    dvec_free(&temp);
    return ret;
}

int unique_i (int *array, int len) {
    ivec_t temp;
    ivec_init(&temp);
    for (int i = 0; i < len; i++) {
        if (ivec_search(&temp, array[i]) == -1) ivec_append(&temp, array[i]);
    }
    int ret = temp.size;
    ivec_free(&temp);
    return ret;
}

int find_string(int lencol, int lenrow, int lenstr, char array[lencol][lenrow], char string[]) {
	for (int i = 0; i < lencol; i++) {
		if (!strncasecmp(array[i], string, lenstr)) return i;
	}
	return -1;
}

void char_prepend(char* s, const char* t) {
    size_t len = strlen(t);
    size_t i;
    memmove(s + len, s, strlen(s) + 1);
    for (i = 0; i < len; ++i) s[i] = t[i];
}

char *char_replace(char* string, const char* substr, const char* replacement) {
    char* tok = NULL;
    char* newstr = NULL;
    char* oldstr = NULL;
    int   oldstr_len = 0;
    int   substr_len = 0;
    int   replacement_len = 0;

    newstr = strdup(string);
    substr_len = strlen(substr);
    replacement_len = strlen(replacement);

    if (substr == NULL || replacement == NULL) return newstr;

    while ((tok = strstr(newstr, substr))) {
        oldstr = newstr;
        oldstr_len = strlen(oldstr);
        newstr = (char*)malloc(sizeof(char) * (oldstr_len - substr_len + replacement_len + 1));

        if (newstr == NULL) {
            free(oldstr);
            return NULL;
        }

        memcpy(newstr, oldstr, tok - oldstr);
        memcpy(newstr + (tok - oldstr), replacement, replacement_len);
        memcpy(newstr + (tok - oldstr) + replacement_len, tok + substr_len, oldstr_len - substr_len - (tok - oldstr));
        memset(newstr + oldstr_len - substr_len + replacement_len, 0, 1);

        free(oldstr);
    }

    // free(string);
    return newstr;
}

void log_init_fp() {
	mkdir(LOGDIR, 0777);
    char logfile[32];
    long int now = time(NULL);
    strftime(logfile, sizeof(logfile), "wctlog_%Y-%m-%d.txt", localtime(&now));
    char_prepend(logfile, LOGDIR);
    FILE *fp = fopen(logfile,"w");
    log_set_fp(fp);
}

void csv_parser(char csvfile[], char *delimiter, int lenrow, int lencol, double returnArray[lenrow][lencol]) {
  int rowIndex = 0;
  char line[2048];
  char* token = NULL;
  FILE* fp = fopen(csvfile,"r");
  if (fp != NULL) {
	while (fgets(line, sizeof(line), fp) != NULL && rowIndex < lenrow) {
	  int colIndex = 0;
	  for (token = strtok(line, delimiter); token != NULL && colIndex < lencol; token = strtok(NULL, delimiter)) {
		returnArray[rowIndex][colIndex++] = atof(token);
	  }
	  rowIndex++;
	}
	fclose(fp);
  } else {
	log_error("File %s Not Found!\n", csvfile);
	wct_exit(WCT_NOT_FOUND);
  }
}

int csv_writer (char filename[], int lenrow, int lencol, double array[lenrow][lencol]) {
	FILE *fp1;
	int array_row, array_col;
	fp1 = fopen(filename, "w");
		if (fp1 == NULL) {
			log_error("Unable to open file\n");
			return 0;
		}
	for (array_row = 0; array_row < lenrow; array_row++) {
		for (array_col = 0; array_col < lencol; array_col++) {
			fprintf(fp1, "%.17f%s", array[array_row][array_col],
					(array_col < lencol - 1 ? "," : ""));
		}
		if (array_row < lenrow - 1) fprintf(fp1,"\n");
	}
	fclose(fp1);
}

void csv_getheader(char csvfile[], char *delimiter, int headercount, char returnArray[headercount][64]) {
  char line[64];
  char* token = NULL;
  FILE* fp = fopen(csvfile,"r");
  if (fp != NULL) {
	if (fgets(line, sizeof(line), fp) != NULL) {
	  int colIndex = 0;
	  for (token = strtok(line, delimiter); token != NULL; token = strtok(NULL, delimiter)) {
		int len = strlen(token);
		if (token[len-1] == '\n') token[len-1] = '\0';
		strcpy(returnArray[colIndex++], token);
	  }
	}
	fclose(fp);
  } else {
	log_error("File %s Not Found!\n", csvfile);
	wct_exit(WCT_NOT_FOUND);
  }
}

int csv_headercount (char csvfile[], char *delimiter) {
  char line[2048];
  char* token = NULL;
  int colIndex = 0;
  FILE* fp = fopen(csvfile,"r");
  if (fp != NULL) {
    if (fgets(line, sizeof(line), fp) != NULL) {
      for (token = strtok(line, delimiter); token != NULL; token = strtok(NULL, delimiter)) {
        colIndex++;
      }
    }
    fclose(fp);
    return colIndex;
  } else {
    return -1;
  }
}

void csv_prepend_each_line(char *csvfile, char *delimiter, char *target_string) {
    FILE * fPtr;
    FILE * fTemp;
    char buffer[2048];

    fPtr  = fopen(csvfile, "r");
    fTemp = fopen("replace.tmp", "w"); 

    if (fPtr == NULL || fTemp == NULL) {
        log_error("Unable to open file.\n");
        wct_exit(WCT_ERROR);
    }
    while ((fgets(buffer, 2048, fPtr)) != NULL) {
        char_prepend(buffer, delimiter);
        char_prepend(buffer, target_string);
        fputs(buffer, fTemp);
    }
    fclose(fPtr);
    fclose(fTemp);
    remove(csvfile);
    rename("replace.tmp", csvfile);
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdiscarded-qualifiers"

	char *config_get (char *inidir, char section[], char key[]) {
		ini_t *config = ini_load(inidir);
		char *ret = ini_get(config, section, key);
		ini_free(config);
		return ret;
	}

#pragma GCC diagnostic pop

void config_fix_lastnewline() {
   /*
   Config_Modify() Requires Last Character To Be '\n'
   config_fix_lastnewline() Fix if last Character is not '\n'
   */
   FILE *fp;
   char ch;
   int num = 1;
   long length;
 
   fp = fopen(CONFIGDIR, "rw");
 
   fseek(fp, 0, SEEK_END);
   length = ftell(fp);
   fseek(fp, (length - num), SEEK_SET);
   ch = fgetc(fp);
   fclose(fp);

   if (ch != '\n') {
   	fp = fopen(CONFIGDIR, "a");
      fputc('\n', fp);
      fclose(fp);
   }
}

void config_modify(char *inidir, char section[], char key[], char value[]) {
	char data[64];
	strcpy(data, key);
	strcat(data, " = ");
	strcat(data, value);

	ini_t *config = ini_load(inidir);
	int replace_line = ini_getline(config, section, key);
	ini_free(config);

	file_replace(CONFIGDIR, replace_line, data);
}

void config_init(cfg_t *cfgobj) {
	cfgobj->ready = 1;

	strcpy(cfgobj->iq_user, config_get(CONFIGDIR, "iqoption", "user"));
	strcpy(cfgobj->iq_pass, config_get(CONFIGDIR, "iqoption", "pass"));

	strcpy(cfgobj->email_smtp, config_get(CONFIGDIR, "email", "smtp"));
	cfgobj->email_port = atoi(config_get(CONFIGDIR, "email", "port"));
	strcpy(cfgobj->email_user, config_get(CONFIGDIR, "email", "user"));
	strcpy(cfgobj->email_pass, config_get(CONFIGDIR, "email", "pass"));
	strcpy(cfgobj->email_sendto, config_get(CONFIGDIR, "email", "send_to"));

	strcpy(cfgobj->resdir, config_get(CONFIGDIR, "default", "resdir"));
	strcpy(cfgobj->logdir, config_get(CONFIGDIR, "default", "logdir"));
	strcpy(cfgobj->coredir, config_get(CONFIGDIR, "default", "coredir"));
	strcpy(cfgobj->rawdir, config_get(CONFIGDIR, "default", "rawdir"));
	strcpy(cfgobj->legacydir, config_get(CONFIGDIR, "default", "legacydir"));

	cfgobj->log = atoi(config_get(CONFIGDIR, "default", "log"));
    cfgobj->lot = atof(config_get(CONFIGDIR, "default", "lot"));

	strcpy(cfgobj->default_assetcode, config_get(CONFIGDIR, "default", "asset"));
	strcpy(cfgobj->default_core, config_get(CONFIGDIR, "default", "core"));
	cfgobj->default_label_maxlen = atoi(config_get(CONFIGDIR, "default", "label_maxlen"));
	cfgobj->default_timeseries_shift = atoi(config_get(CONFIGDIR, "default", "timeseries_shift"));
	cfgobj->default_range_step = atoi(config_get(CONFIGDIR, "default", "range_step"));
	cfgobj->default_range_start = atoi(config_get(CONFIGDIR, "default", "range_start"));
	cfgobj->default_range_end = atoi(config_get(CONFIGDIR, "default", "range_end"));
	cfgobj->default_features_len = atoi(config_get(CONFIGDIR, "default", "features_len"));
	cfgobj->default_similarity = atof(config_get(CONFIGDIR, "default", "similarity"));
	cfgobj->default_min_pattern_found = atoi(config_get(CONFIGDIR, "default", "min_pattern_found"));
	cfgobj->default_min_prediction_rate = atof(config_get(CONFIGDIR, "default", "min_prediction_rate"));
	cfgobj->default_learning_rate = atof(config_get(CONFIGDIR, "default", "learning_rate"));
	cfgobj->default_discount_rate = atof(config_get(CONFIGDIR, "default", "discount_rate"));
}

void config_print() {
	char c;
	FILE *fptr;
	fptr = fopen(CONFIGDIR, "r"); 
    if (fptr == NULL) { 
        log_error("Cannot Open File \n"); 
        wct_exit(WCT_ERROR); 
    } 
    c = fgetc(fptr); 
    printf("WCT Configuration (%s):\n{\n\t", CONFIGDIR);
    while (c != EOF) { 
    	if (c == '\n') printf("\n\t"); else printf ("%c", c); 
        c = fgetc(fptr); 
    } 
    printf("\n}\n");
    fclose(fptr); 
}

void config_save (cfg_t cfg) {
	config_fix_lastnewline();
	char iq_user[32];
	char iq_pass[32];

	char email_smtp[32];
	char email_port[32];
	char email_user[32];
	char email_pass[32];
	char email_sendto[32];

	char resdir[32];
	char logdir[32];
	char coredir[32];
	char rawdir[32];
	char legacydir[32];

	char log[32];
    char lot[32];

	char default_assetcode[32];
	char default_core[32];
	char default_label_maxlen[32];
	char default_timeseries_shift[32];
	char default_range_step[32];
	char default_range_start[32];
	char default_range_end[32];
	char default_features_len[32];
	char default_similarity[32];
	char default_min_pattern_found[32];
	char default_min_prediction_rate[32];
	char default_learning_rate[32];
	char default_discount_rate[32];

	strcpy(iq_user, cfg.iq_user);
	strcpy(iq_pass, cfg.iq_pass);

	strcpy(email_smtp, cfg.email_smtp);
	sprintf(email_port, "%d", cfg.email_port);
	strcpy(email_user, cfg.email_user);
	strcpy(email_pass, cfg.email_pass);
	strcpy(email_sendto, cfg.email_sendto);

	sprintf(resdir, "\"%s\"", cfg.resdir);
	sprintf(logdir, "\"%s\"", cfg.logdir);
	sprintf(coredir, "\"%s\"", cfg.coredir);
	sprintf(rawdir, "\"%s\"", cfg.rawdir);
	sprintf(legacydir, "\"%s\"", cfg.legacydir);

	sprintf(log, "%d", cfg.log);
    sprintf(lot, "%f", cfg.log);

	strcpy(default_assetcode, cfg.default_assetcode);
	strcpy(default_core, cfg.default_core);
	sprintf(default_label_maxlen, "%d", cfg.default_label_maxlen);
	sprintf(default_timeseries_shift, "%d", cfg.default_timeseries_shift);
	sprintf(default_range_step, "%d", cfg.default_range_step);
	sprintf(default_range_start, "%d", cfg.default_range_start);
	sprintf(default_range_end, "%d", cfg.default_range_end);
	sprintf(default_features_len, "%d", cfg.default_features_len);
	sprintf(default_similarity, "%.3f", cfg.default_similarity);
	sprintf(default_min_pattern_found, "%d", cfg.default_min_pattern_found);
	sprintf(default_min_prediction_rate, "%.3f", cfg.default_min_prediction_rate);
	sprintf(default_learning_rate, "%.3f", cfg.default_learning_rate);
	sprintf(default_discount_rate, "%.3f", cfg.default_discount_rate);

	config_modify(CONFIGDIR, "iqoption", "user", iq_user);
	config_modify(CONFIGDIR, "iqoption", "pass", iq_pass);

	config_modify(CONFIGDIR, "email", "smtp", email_smtp);
	config_modify(CONFIGDIR, "email", "port", email_port);
	config_modify(CONFIGDIR, "email", "user", email_user);
	config_modify(CONFIGDIR, "email", "pass", email_pass);
	config_modify(CONFIGDIR, "email", "send_to", email_sendto);

	config_modify(CONFIGDIR, "default", "resdir", resdir);
	config_modify(CONFIGDIR, "default", "logdir", logdir);
	config_modify(CONFIGDIR, "default", "coredir", coredir);
	config_modify(CONFIGDIR, "default", "rawdir", rawdir);
	config_modify(CONFIGDIR, "default", "legacydir", legacydir);

	config_modify(CONFIGDIR, "default", "log", log);
    config_modify(CONFIGDIR, "default", "lot", lot);

	config_modify(CONFIGDIR, "default", "asset", default_assetcode);
	config_modify(CONFIGDIR, "default", "core", default_core);
	config_modify(CONFIGDIR, "default", "label_maxlen", default_label_maxlen);
	config_modify(CONFIGDIR, "default", "timeseries_shift", default_timeseries_shift);
	config_modify(CONFIGDIR, "default", "range_step", default_range_step);
	config_modify(CONFIGDIR, "default", "range_start", default_range_start);
	config_modify(CONFIGDIR, "default", "range_end", default_range_end);
	config_modify(CONFIGDIR, "default", "features_len", default_features_len);
	config_modify(CONFIGDIR, "default", "similarity", default_similarity);
	config_modify(CONFIGDIR, "default", "min_pattern_found", default_min_pattern_found);
	config_modify(CONFIGDIR, "default", "min_prediction_rate", default_min_prediction_rate);
	config_modify(CONFIGDIR, "default", "learning_rate", default_learning_rate);
	config_modify(CONFIGDIR, "default", "discount_rate", default_discount_rate);
}

char *config_json() {
    config_fix_lastnewline();
    char *cfgJson = malloc(file_size(CONFIGDIR) + 100);
    char c;
    char *temp;

    FILE *fptr;
    fptr = fopen(CONFIGDIR, "r"); 
    c = fgetc(fptr);
    cfgJson[0] = '{';
    cfgJson[1] = '\0';
    while (c != EOF) { 
        sprintf(cfgJson, "%s%c", cfgJson, c); 
        c = fgetc(fptr);
    } 
    fclose(fptr);

    temp = char_replace(cfgJson, " = ", "\": \"");
    strcpy(cfgJson, temp);
    free(temp);

    temp = char_replace(cfgJson, "\n[", "\"}, \"");
    strcpy(cfgJson, temp);
    free(temp);

    temp = char_replace(cfgJson, "]\n", "\": {\"");
    strcpy(cfgJson, temp);
    free(temp);

    temp = char_replace(cfgJson, "[", "\"");
    strcpy(cfgJson, temp);
    free(temp);

    temp = char_replace(cfgJson, "\n", "\", \"");
    strcpy(cfgJson, temp);
    free(temp);

    temp = char_replace(cfgJson, "\"\"", "\"");
    strcpy(cfgJson, temp);
    free(temp);
    
    for (int i = 0; i < 3; i++) cfgJson[strlen(cfgJson) - 1] = '\0';
    cfgJson[strlen(cfgJson)] = '}';

    return cfgJson;
}

void ohlc_init(char filename[], char *delimiter, ohlc_t *ohlc_obj, int target_shift) {  
    int valid[] = {0, 1, 5, 15, 30, 60, 240, 1440, 10080};
    if (linear_search_i(LEN(valid), target_shift, valid) == -1) {
        log_error("Invalid Target Shift Argument\n");
        wct_exit(WCT_INVALID_ARGUMENT);
    }

    ivec_init(&ohlc_obj->tm);
    dvec_init(&ohlc_obj->open);
    dvec_init(&ohlc_obj->high);
    dvec_init(&ohlc_obj->low);
    dvec_init(&ohlc_obj->close);

    ohlc_t temp_ohlc;
    ivec_init(&temp_ohlc.tm);
    dvec_init(&temp_ohlc.open);
    dvec_init(&temp_ohlc.high);
    dvec_init(&temp_ohlc.low);
    dvec_init(&temp_ohlc.close);

    int time_index, open_index, high_index, low_index, close_index = 0;
    char header[16][64];
    int collimit = csv_headercount(filename, delimiter);
    csv_getheader(filename, ",", collimit, header);
    time_index = STRFND(header, "time");
    open_index = STRFND(header, "open");
    high_index = STRFND(header, "high");
    low_index = STRFND(header, "low");
    close_index = STRFND(header, "close");

    int rowIndex = 0;
    char line[128];
    char* token = NULL;
    FILE* fp = fopen(filename,"r");
    if (fp != NULL) {
      while (fgets(line, sizeof(line), fp) != NULL && rowIndex < file_linecount(filename)) {
        int colIndex = 0;
        for (token = strtok( line, delimiter); token != NULL && colIndex < collimit; token = strtok(NULL, delimiter)) {
            if (rowIndex == 0) {
                colIndex++;
                continue;
            } else {
                if (colIndex == time_index) {
                    colIndex++;

                    struct tm tmdate;
                    sscanf(token, "%d.%d.%d %d:%d", &tmdate.tm_year, &tmdate.tm_mon, &tmdate.tm_mday, &tmdate.tm_hour, &tmdate.tm_min);
                    tmdate.tm_year -= 1900;
                    tmdate.tm_mon -= 1;
                    tmdate.tm_sec = 0;
                    ivec_append(&temp_ohlc.tm, mktime(&tmdate));

                    // strcpy(timevar[rowIndex - 1], token);
                } else if (colIndex == open_index) {
                    colIndex++;
                    dvec_append(&temp_ohlc.open, atof(token));
                } else if (colIndex == high_index) {
                    colIndex++;
                    dvec_append(&temp_ohlc.high, atof(token));
                } else if (colIndex == low_index) {
                    colIndex++;
                    dvec_append(&temp_ohlc.low, atof(token));
                } else if (colIndex == close_index) {
                    colIndex++;
                    dvec_append(&temp_ohlc.close, atof(token));
                } else {
                    colIndex++;
                }
            }
        }
        rowIndex++;
      }
      fclose(fp);

      int current_shift = (temp_ohlc.tm.data[1] - temp_ohlc.tm.data[0]) / 60;

      if (current_shift >= target_shift) {
        // log_info("Initializing ohlc_t Without Shift\n");

        for (int j = 0; j < temp_ohlc.tm.size; j++) {
            ivec_append(&ohlc_obj->tm, temp_ohlc.tm.data[j]);
            dvec_append(&ohlc_obj->open, temp_ohlc.open.data[j]);
            dvec_append(&ohlc_obj->high, temp_ohlc.high.data[j]);
            dvec_append(&ohlc_obj->low, temp_ohlc.low.data[j]);
            dvec_append(&ohlc_obj->close, temp_ohlc.close.data[j]);
        }

        ivec_free(&temp_ohlc.tm);
        dvec_free(&temp_ohlc.open);
        dvec_free(&temp_ohlc.high);
        dvec_free(&temp_ohlc.low);
        dvec_free(&temp_ohlc.close);

      } else {

        int a = 0;
        int step = 0;
        int shift_factor = (target_shift / current_shift);

        while (1) {
            if (temp_ohlc.tm.data[a] % (target_shift * 60) == 0) break; else a++;
        }

        for (int j = 0; j < temp_ohlc.tm.size; j++) {
            if (temp_ohlc.tm.data[j] % (target_shift * 60) == 0) {
                double temp[240];
                slice_d(temp_ohlc.close.data, temp, j - step - 1, j - 1);
                ivec_append(&ohlc_obj->tm, temp_ohlc.tm.data[j] - (target_shift * 60));
                dvec_append(&ohlc_obj->open, temp[0]);
                dvec_append(&ohlc_obj->high, max_d(temp, step + 1));
                dvec_append(&ohlc_obj->low, min_d(temp, step + 1));
                dvec_append(&ohlc_obj->close, temp[step]);
                step = 0;
            } else {
                step++;
            }
        }

        ivec_free(&temp_ohlc.tm);
        dvec_free(&temp_ohlc.open);
        dvec_free(&temp_ohlc.high);
        dvec_free(&temp_ohlc.low);
        dvec_free(&temp_ohlc.close);

      }

      ivec_pop(&ohlc_obj->tm);
      dvec_pop(&ohlc_obj->open);
      dvec_pop(&ohlc_obj->high);
      dvec_pop(&ohlc_obj->low);
      dvec_pop(&ohlc_obj->close);

    } else {
      log_error("File %s Not Found!\n", filename);
      wct_exit(WCT_NOT_FOUND);
    }
}

void ohlc_print(ohlc_t *ohlc_obj, int count) {
    char buffer[80];
    printf("\nTime\t\t\t\tOpen\t\tHigh\t\tLow\t\t\tClose\n");

    int lenght = count;

    for (int i = 0; i < lenght; i++) {
        strftime(buffer, sizeof(buffer), "%Y.%m.%d %H:%M", localtime((long int *)&ohlc_obj->tm.data[i]));
        printf("%s\t%.5f\t\t%.5f\t\t%.5f\t\t%.5f\n", buffer, ohlc_obj->open.data[i], ohlc_obj->high.data[i],
            ohlc_obj->low.data[i], ohlc_obj->close.data[i]);
    }
}

void ohlc_dropna(ohlc_t *ohlc_obj, int target_num) {
    ivec_t filter;
    ivec_init(&filter);

    int lenght = ohlc_obj->tm.size;

    switch (target_num) {
        case NUM_OPEN:
            for (int i = 0; i < lenght; i++) {
                if (ohlc_obj->open.data[i] < 0.000001) ivec_append(&filter, i);
            }
            break;
        case NUM_HIGH:
            for (int i = 0; i < lenght; i++) {
                if (ohlc_obj->high.data[i] < 0.000001) ivec_append(&filter, i);
            }
            break;
        case NUM_LOW:
            for (int i = 0; i < lenght; i++) {
                if (ohlc_obj->low.data[i] < 0.000001) ivec_append(&filter, i);
            }
            break;
        case NUM_CLOSE:
            for (int i = 0; i < lenght; i++) {
                if (ohlc_obj->close.data[i] < 0.000001) ivec_append(&filter, i);
            }
            break;
        default:
            log_error("Invalid 'target_num' Argument!\n");
            wct_exit(WCT_INVALID_ARGUMENT);
    }
    ivec_filter(&ohlc_obj->tm, filter.data, filter.size);
    dvec_filter(&ohlc_obj->open, filter.data, filter.size);
    dvec_filter(&ohlc_obj->high, filter.data, filter.size);
    dvec_filter(&ohlc_obj->low, filter.data, filter.size);
    dvec_filter(&ohlc_obj->close, filter.data, filter.size);

    ivec_free(&filter);
}

void meta_save(meta_t *meta) {
    if (meta->ready != 1) {
        log_error("WCT is Not Initialized, Can't Proceed With Meta Operation!\n");
        wct_exit(WCT_NOT_READY);
    }
    mkdir(COREDIR, 0777);
    char meta_dir[64];
    strcpy(meta_dir, meta->core);
    char_prepend(meta_dir, COREDIR);
    strcat(meta_dir, "/");
    mkdir(meta_dir, 0777);
    strcat(meta_dir, METAFILE);
    FILE *fp;
    fp = fopen(meta_dir,"wb");
    fwrite(&meta->ready, sizeof(int), 1, fp);
    fwrite(&meta->type, sizeof(int), 1, fp);
    fwrite(&meta->major, sizeof(int), 1, fp);
    fwrite(&meta->minor, sizeof(int), 1, fp);
    fwrite(&meta->patch, sizeof(int), 1, fp);
    fwrite(&meta->num_optimize, sizeof(int), 1, fp);
    fwrite(&meta->num_trained, sizeof(int), 1, fp);
    fwrite(&meta->last_wr, sizeof(double), 1, fp);
    fwrite(&meta->last_train, sizeof(long int), 1, fp);
    for (int i = 0; i < 64; i++)
        fwrite(&meta->core[i], sizeof(char), 1, fp);
    fwrite(&meta->label_maxlen, sizeof(int), 1, fp);
    fwrite(&meta->islabelfitted, sizeof(int), 1, fp);
    fwrite(&meta->islabelencoded, sizeof(int), 1, fp);
    for (int i = 0; i < 10; i++)
        fwrite(&meta->asset_code[i], sizeof(char), 1, fp);
    fwrite(&meta->timeseries_shift, sizeof(int), 1, fp);
    fwrite(&meta->range_step, sizeof(int), 1, fp);
    fwrite(&meta->range_start, sizeof(int), 1, fp);
    fwrite(&meta->range_end, sizeof(int), 1, fp);
    fwrite(&meta->features_len, sizeof(int), 1, fp);
    fwrite(&meta->similarity, sizeof(double), 1, fp);
    fwrite(&meta->min_pattern_found, sizeof(int), 1, fp);
    fwrite(&meta->min_prediction_rate, sizeof(double), 1, fp);
    fclose(fp);
}

void meta_load(meta_t *meta, char *metadir) {
    if (meta->ready != 1) {
        log_error("WCT is Not Initialized, Can't Proceed With Meta Operation!\n");
        wct_exit(WCT_NOT_READY);
    }
    char meta_dir[64];
    strcpy(meta_dir, metadir);
    char_prepend(meta_dir, COREDIR);
    strcat(meta_dir, "/");
    strcat(meta_dir, METAFILE);
    FILE *fp;
    fp = fopen(meta_dir,"rb");
    if (fp == NULL) {
        log_error("Core: '%s' Not Found, Check Your Argument!\n", metadir);
        wct_exit(WCT_NOT_FOUND);
    }
    fread(&meta->ready, sizeof(int), 1, fp);
    fread(&meta->type, sizeof(int), 1, fp);
    fread(&meta->major, sizeof(int), 1, fp);
    fread(&meta->minor, sizeof(int), 1, fp);
    fread(&meta->patch, sizeof(int), 1, fp);
    fread(&meta->num_optimize, sizeof(int), 1, fp);
    fread(&meta->num_trained, sizeof(int), 1, fp);
    fread(&meta->last_wr, sizeof(double), 1, fp);
    fread(&meta->last_train, sizeof(long int), 1, fp);
    for (int i = 0; i < 64; i++)
        fread(&meta->core[i], sizeof(char), 1, fp);
    fread(&meta->label_maxlen, sizeof(int), 1, fp);
    fread(&meta->islabelfitted, sizeof(int), 1, fp);
    fread(&meta->islabelencoded, sizeof(int), 1, fp);
    for (int i = 0; i < 10; i++)
        fread(&meta->asset_code[i], sizeof(char), 1, fp);
    fread(&meta->timeseries_shift, sizeof(int), 1, fp);
    fread(&meta->range_step, sizeof(int), 1, fp);
    fread(&meta->range_start, sizeof(int), 1, fp);
    fread(&meta->range_end, sizeof(int), 1, fp);
    fread(&meta->features_len, sizeof(int), 1, fp);
    fread(&meta->similarity, sizeof(double), 1, fp);
    fread(&meta->min_pattern_found, sizeof(int), 1, fp);
    fread(&meta->min_prediction_rate, sizeof(double), 1, fp);
    fclose(fp);
}

void meta_import(meta_t *meta, char *metadir) {
    meta->ready = 1;
    
    meta->type = atoi(config_get(metadir, "default", "type"));
    meta->major = atoi(config_get(metadir, "default", "major"));
    meta->minor = atoi(config_get(metadir, "default", "minor"));
    meta->patch = atoi(config_get(metadir, "default", "patch"));
    meta->num_optimize = atoi(config_get(metadir, "default", "num_optimize"));
    meta->num_trained = atoi(config_get(metadir, "default", "num_trained"));
    meta->last_wr = atof(config_get(metadir, "default", "last_wr"));
    meta->last_train = atoi(config_get(metadir, "default", "last_train"));
    strcpy(meta->core, config_get(metadir, "default", "core"));
    meta->label_maxlen = atoi(config_get(metadir, "default", "label_maxlen"));
    meta->islabelfitted = atoi(config_get(metadir, "default", "islabelfitted"));
    meta->islabelencoded = atoi(config_get(metadir, "default", "islabelencoded"));
    strcpy(meta->asset_code, config_get(metadir, "default", "asset_code"));
    meta->timeseries_shift = atoi(config_get(metadir, "default", "timeseries_shift"));
    meta->range_step = atoi(config_get(metadir, "default", "range_step"));
    meta->range_start = atoi(config_get(metadir, "default", "range_start"));
    meta->range_end = atoi(config_get(metadir, "default", "range_end"));
    meta->features_len = atoi(config_get(metadir, "default", "features_len"));
    meta->similarity = atof(config_get(metadir, "default", "similarity"));
    meta->min_pattern_found = atoi(config_get(metadir, "default", "min_pattern_found"));
    meta->min_prediction_rate = atof(config_get(metadir, "default", "min_prediction_rate"));

    meta_save(meta);
}

void meta_reset_default (meta_t *meta) {
    if (meta->ready != 1) {
        log_error("WCT is Not Initialized, Can't Proceed With Meta Operation!\n");
        wct_exit(WCT_NOT_READY);
    }

    char *metadir = CONFIGDIR;

    meta->ready = 1;
    meta->type = 0;
    meta->major = 0;
    meta->minor = 0;
    meta->patch = 0;
    meta->num_optimize = 0;
    meta->num_trained = 0;
    meta->last_wr = 0.0;
    meta->last_train = time(NULL);
    meta->islabelfitted = 0;
    meta->islabelencoded = 0;

    strcpy(meta->core, config_get(metadir, "default", "core"));
    meta->label_maxlen = atoi(config_get(metadir, "default", "label_maxlen"));
    strcpy(meta->asset_code, config_get(metadir, "default", "asset_code"));
    meta->timeseries_shift = atoi(config_get(metadir, "default", "timeseries_shift"));
    meta->range_step = atoi(config_get(metadir, "default", "range_step"));
    meta->range_start = atoi(config_get(metadir, "default", "range_start"));
    meta->range_end = atoi(config_get(metadir, "default", "range_end"));
    meta->features_len = atoi(config_get(metadir, "default", "features_len"));
    meta->similarity = atof(config_get(metadir, "default", "similarity"));
    meta->min_pattern_found = atoi(config_get(metadir, "default", "min_pattern_found"));
    meta->min_prediction_rate = atof(config_get(metadir, "default", "min_prediction_rate"));

    meta_save (meta);
}

void meta_update_prompt (meta_t *meta) {
    if (meta->ready != 1) {
        log_error("WCT is Not Initialized, Can't Proceed With Meta Operation!\n");
        wct_exit(WCT_NOT_READY);
    }
    // Pure Meta Section
    // printf("Type: "); scanf("%d", &meta->type);
    // printf("Major: "); scanf("%d", &meta->major);
    // printf("Minor: "); scanf("%d", &meta->minor);
    // printf("Patch: "); scanf("%d", &meta->patch);
    // printf("Num_Optimize: "); scanf("%d", &meta->num_optimize);
    // printf("Num_Trained: "); scanf("%d", &meta->num_trained);
    // printf("Last_WR: "); scanf("%f", &meta->last_wr);
    // printf("Last_Train: "); scanf("%d", &meta->last_train);
    // printf("Core: "); scanf("%s", meta->core);
    // printf("islabelfitted: "); scanf("%d", &meta->islabelfitted);
    // printf("islabelencoded: "); scanf("%d", &meta->islabelencoded);

    // Parameter Meta Section
    printf("Label_Maxlen: "); scanf("%d", &meta->label_maxlen);
    printf("Asset_Code: "); scanf("%s", meta->asset_code);
    printf("Timeseries-Shift: "); scanf("%d", &meta->timeseries_shift);
    printf("Range_Step: "); scanf("%d", &meta->range_step);
    printf("Range_Start: "); scanf("%d", &meta->range_start);
    printf("Range_End: "); scanf("%d", &meta->range_end);
    printf("Features_Len: "); scanf("%d", &meta->features_len);
    printf("similarity: "); scanf("%f", &meta->similarity);
    printf("Min_Pattern_Found: "); scanf("%d", &meta->min_pattern_found);
    printf("Min_Prediction_Rate: "); scanf("%f", &meta->min_prediction_rate);
    meta_save(meta);
    log_info("Meta Update Done!\n");
}

double wct_random_init() {
    return random_d(-1,1);
}

double wct_pointone_init() {
    return 0.1;
}

const char *core_name (char *assetcode, int type, int shift, int step, int start, int end, int len, int label_len) {
    char buff[64];
    sprintf(buff, "wctcore(%d)-%s-%d%d%d%d%d%d", type, assetcode, shift, step, start, end, len, label_len);
    char *ret = malloc(strlen(buff));
    strcpy(ret, buff);
    return ret;
}

const char *wct_name (wct_t *wct) {
    char *ret = malloc(strlen(core_name(wct->meta.asset_code, wct->meta.type, wct->meta.timeseries_shift, wct->meta.range_step, wct->meta.range_start, wct->meta.range_end, wct->meta.features_len, wct->meta.label_maxlen)));
    strcpy(ret, core_name(wct->meta.asset_code, wct->meta.type, wct->meta.timeseries_shift, wct->meta.range_step, wct->meta.range_start, wct->meta.range_end, wct->meta.features_len, wct->meta.label_maxlen));
    return ret;
}

void wct_print (wct_t *wct) {
    printf("WCT Insight [%s]: {\n", core_name(wct->meta.asset_code, wct->meta.type, wct->meta.timeseries_shift, wct->meta.range_step, wct->meta.range_start, wct->meta.range_end, wct->meta.features_len, wct->meta.label_maxlen));
    switch (wct->meta.type){
        case CUSTOM:
            printf("%25s: %s\n", "Type", "CUSTOM");
            break;
        case CLASSIFICATION:
            printf("%25s: %s\n", "Type", "CLASSIFICATION");
            break;
        case REGRESSION:
            printf("%25s: %s\n", "Type", "REGRESSION");
            break;
        case RISEFALL:
            printf("%25s: %s\n", "Type", "RISEFALL");
            break;
        default:
            printf("%25s: %s\n", "Type", "UNDEFINED");
            break;
    }
    printf("%25s: %d.%d.%d\n", "Core Version", wct->meta.major, wct->meta.minor, wct->meta.patch);
    printf("%25s: %d\n", "Number Optimized:", wct->meta.num_optimize);
    printf("%25s: %d\n", "Number Trained", wct->meta.num_trained);
    printf("%25s: %.2f\n", "Last Win Rate", wct->meta.last_wr);
    char last_train_buffer[80];
    strftime(last_train_buffer, sizeof(last_train_buffer), "%Y.%m.%d %H:%M", localtime((long int *)&wct->meta.last_train));
    printf("%25s: %s\n", "Last Trained", last_train_buffer);
    printf("%25s: %s\n", "Core Name", wct->meta.core);
    printf("%25s: %d\n", "Label Maxlen", wct->meta.label_maxlen);
    printf("%25s: %d\n", "Is Label Fitted", wct->meta.islabelfitted);
    printf("%25s: %d\n", "Is Label Encoded", wct->meta.islabelencoded);
    printf("%25s: %s\n", "Core Code", wct->meta.asset_code);
    printf("%25s: %d\n", "Timeseries Shift", wct->meta.timeseries_shift);
    printf("%25s: %d\n", "Range Step", wct->meta.range_step);
    printf("%25s: %d\n", "Range Start", wct->meta.range_start);
    printf("%25s: %d\n", "Range End", wct->meta.range_end);
    printf("%25s: %d\n", "Node Lenght", wct->meta.features_len);
    printf("%25s: %.2f\n", "Minimal Similarity", wct->meta.similarity);
    printf("%25s: %d\n", "Minimal Node Found", wct->meta.min_pattern_found);
    printf("%25s: %.2f\n", "Minimal Prediction Rate", wct->meta.min_prediction_rate);
    printf("%25s: %d\n", "Node Count", wct->weight.size);
    printf("}\n\n");
}

void wct_save (wct_t *wct) {
    char meta_dir[64];
    char weight_dir[64];
    char feat_dir[64];
    char label_dir[64];
    char dynlabel_dir[64];
    char featureindex_dir[64];

    mkdir(COREDIR, 0777);
    char core_dir[64];
    strcpy(core_dir, wct_name(wct));

    // Meta Write
    strcpy(meta_dir, core_dir);
    char_prepend(meta_dir, COREDIR);
    strcat(meta_dir, "/");
    mkdir(meta_dir, 0777);
    strcat(meta_dir, METAFILE);
    FILE *fp;
    fp = fopen(meta_dir,"wb");
    fwrite(&wct->meta.ready, sizeof(int), 1, fp);
    fwrite(&wct->meta.type, sizeof(int), 1, fp);
    fwrite(&wct->meta.major, sizeof(int), 1, fp);
    fwrite(&wct->meta.minor, sizeof(int), 1, fp);
    fwrite(&wct->meta.patch, sizeof(int), 1, fp);
    fwrite(&wct->meta.num_optimize, sizeof(int), 1, fp);
    fwrite(&wct->meta.num_trained, sizeof(int), 1, fp);
    fwrite(&wct->meta.last_wr, sizeof(double), 1, fp);
    fwrite(&wct->meta.last_train, sizeof(long int), 1, fp);
    for (int i = 0; i < 64; i++)
        fwrite(&wct->meta.core[i], sizeof(char), 1, fp);
    int label_len = wct->label.col[0];
    fwrite(&label_len, sizeof(int), 1, fp);
    fwrite(&wct->meta.islabelfitted, sizeof(int), 1, fp);
    fwrite(&wct->meta.islabelencoded, sizeof(int), 1, fp);
    for (int i = 0; i < 10; i++)
        fwrite(&wct->meta.asset_code[i], sizeof(char), 1, fp);
    fwrite(&wct->meta.timeseries_shift, sizeof(int), 1, fp);
    fwrite(&wct->meta.range_step, sizeof(int), 1, fp);
    fwrite(&wct->meta.range_start, sizeof(int), 1, fp);
    fwrite(&wct->meta.range_end, sizeof(int), 1, fp);
    int feat_len = wct->features.col[0];
    fwrite(&feat_len, sizeof(int), 1, fp);
    fwrite(&wct->meta.similarity, sizeof(double), 1, fp);
    fwrite(&wct->meta.min_pattern_found, sizeof(int), 1, fp);
    fwrite(&wct->meta.min_prediction_rate, sizeof(double), 1, fp);
    fclose(fp);

    // Weight Write
    strcpy(weight_dir, core_dir);
    char_prepend(weight_dir, COREDIR);
    strcat(weight_dir, "/");
    mkdir(weight_dir, 0777);
    strcat(weight_dir, WEIGHTFILE);
    fp = fopen(weight_dir,"wb");
    int len = wct->weight.size;
    fwrite(&len, sizeof(int), 1, fp);
    for (int i = 0; i < len; i++){
        fwrite(&wct->weight.data[i], sizeof(double), 1, fp);
    }
    fclose(fp);

    // Features Write
    strcpy(feat_dir, core_dir);
    char_prepend(feat_dir, COREDIR);
    strcat(feat_dir, "/");
    mkdir(feat_dir, 0777);
    strcat(feat_dir, FEATURESFILE);
    fp = fopen(feat_dir,"wb");
    fwrite(&len, sizeof(int), 1, fp);
    for (int r = 0; r < len; r++){
        for (int c = 0; c < feat_len; c++){
            fwrite(&wct->features.data[r][c], sizeof(double), 1, fp);
        }
    }
    fclose(fp);

    // Label Write
    strcpy(label_dir, core_dir);
    char_prepend(label_dir, COREDIR);
    strcat(label_dir, "/");
    mkdir(label_dir, 0777);
    strcat(label_dir, LABELFILE);
    fp = fopen(label_dir,"wb");
    fwrite(&len, sizeof(int), 1, fp);
    for (int r = 0; r < len; r++){
        for (int c = 0; c < label_len; c++) {
            fwrite(&wct->label.data[r][c], sizeof(double), 1, fp);
        }
    }
    fclose(fp);

    // Dynlabel Write
    strcpy(dynlabel_dir, core_dir);
    char_prepend(dynlabel_dir, COREDIR);
    strcat(dynlabel_dir, "/");
    mkdir(dynlabel_dir, 0777);
    strcat(dynlabel_dir, DYNLABELFILE);
    fp = fopen(dynlabel_dir,"wb");
    int indexlen = wct->dynlabel.index.size;
    fwrite(&indexlen, sizeof(int), 1, fp);
    for (int i = 0; i < indexlen; i++){
        fwrite(&wct->dynlabel.index.data[i], sizeof(int), 1, fp);
    }
    int valuelen = wct->dynlabel.value.size;
    fwrite(&valuelen, sizeof(int), 1, fp);
    for (int i = 0; i < valuelen; i++){
        fwrite(&wct->dynlabel.value.data[i], sizeof(int), 1, fp);
    }
    fclose(fp);

    // FeatureIndex Write
    strcpy(featureindex_dir, core_dir);
    char_prepend(featureindex_dir, COREDIR);
    strcat(featureindex_dir, "/");
    mkdir(featureindex_dir, 0777);
    strcat(featureindex_dir, FEATUREINDEXFILE);
    fp = fopen(featureindex_dir,"wb");
    int featureindexlen = wct->featureindex.size;
    fwrite(&featureindexlen, sizeof(int), 1, fp);
    for (int i = 0; i < featureindexlen; i++){
        fwrite(&wct->featureindex.data[i], sizeof(double), 1, fp);
    }
    fclose(fp);
}

void wct_load (wct_t *wct, char *core_dir) {
    char meta_dir[64];
    char weight_dir[64];
    char feat_dir[64];
    char label_dir[64];
    char dynlabel_dir[64];
    char featureindex_dir[64];

    double temp_double = 0.0;
    int temp_int = 0;
    int len = 0;
    int feat_len = 0;
    int label_len = 0;

    dvec_init(&wct->weight);
    dtab_init(&wct->features);
    dtab_init(&wct->label);
    dvec_init(&wct->featureindex);
    ivec_init(&wct->dynlabel.index);
    ivec_init(&wct->dynlabel.value);

    // Meta Load
    strcpy(meta_dir, core_dir);
    char_prepend(meta_dir, COREDIR);
    strcat(meta_dir, "/");
    strcat(meta_dir, METAFILE);
    FILE *fp;
    fp = fopen(meta_dir,"rb");
    if (fp == NULL) {
        log_error("Core: '%s' Not Found, Check Your Argument!\n", core_dir);
        wct_exit(WCT_NOT_FOUND);
    }
    fread(&wct->meta.ready, sizeof(int), 1, fp);
    fread(&wct->meta.type, sizeof(int), 1, fp);
    fread(&wct->meta.major, sizeof(int), 1, fp);
    fread(&wct->meta.minor, sizeof(int), 1, fp);
    fread(&wct->meta.patch, sizeof(int), 1, fp);
    fread(&wct->meta.num_optimize, sizeof(int), 1, fp);
    fread(&wct->meta.num_trained, sizeof(int), 1, fp);
    fread(&wct->meta.last_wr, sizeof(double), 1, fp);
    fread(&wct->meta.last_train, sizeof(long int), 1, fp);
    for (int i = 0; i < 64; i++)
        fread(&wct->meta.core[i], sizeof(char), 1, fp);
    fread(&label_len, sizeof(int), 1, fp);
    wct->meta.label_maxlen = label_len;
    fread(&wct->meta.islabelfitted, sizeof(int), 1, fp);
    fread(&wct->meta.islabelencoded, sizeof(int), 1, fp);
    for (int i = 0; i < 10; i++)
        fread(&wct->meta.asset_code[i], sizeof(char), 1, fp);
    fread(&wct->meta.timeseries_shift, sizeof(int), 1, fp);
    fread(&wct->meta.range_step, sizeof(int), 1, fp);
    fread(&wct->meta.range_start, sizeof(int), 1, fp);
    fread(&wct->meta.range_end, sizeof(int), 1, fp);
    fread(&feat_len, sizeof(int), 1, fp);
    wct->meta.features_len = feat_len;
    fread(&wct->meta.similarity, sizeof(double), 1, fp);
    fread(&wct->meta.min_pattern_found, sizeof(int), 1, fp);
    fread(&wct->meta.min_prediction_rate, sizeof(double), 1, fp);
    fclose(fp);

    // Weight Load
    strcpy(weight_dir, core_dir);
    char_prepend(weight_dir, COREDIR);
    strcat(weight_dir, "/");
    strcat(weight_dir, WEIGHTFILE);
    fp = fopen(weight_dir,"rb");
    fread(&len, sizeof(int), 1, fp);
    for (int i = 0; i < len; i++){
        fread(&temp_double, sizeof(double), 1, fp);
        dvec_append(&wct->weight, temp_double);
    }
    fclose(fp);

    // Features Load
    strcpy(feat_dir, core_dir);
    char_prepend(feat_dir, COREDIR);
    strcat(feat_dir, "/");
    strcat(feat_dir, FEATURESFILE);
    fp = fopen(feat_dir,"rb");
    fread(&len, sizeof(int), 1, fp);
    for (int r = 0; r < len; r++) {
        dtab_add_row(&wct->features);
        for (int c = 0; c < feat_len; c++) {
            fread(&temp_double, sizeof(double), 1, fp);
            dtab_append_col(&wct->features, r, temp_double);
        }
    }
    fclose(fp);

    // Label Load
    strcpy(label_dir, core_dir);
    char_prepend(label_dir, COREDIR);
    strcat(label_dir, "/");
    strcat(label_dir, LABELFILE);
    fp = fopen(label_dir,"rb");
    fread(&len, sizeof(int), 1, fp);
    for (int r = 0; r < len; r++){
        dtab_add_row(&wct->label);
        for (int c = 0; c < label_len; c++) {
            fread(&temp_double, sizeof(double), 1, fp);
            dtab_append_col(&wct->label, r, temp_double);
        }
    }
    fclose(fp);

    // Dynlabel Load
    strcpy(dynlabel_dir, core_dir);
    char_prepend(dynlabel_dir, COREDIR);
    strcat(dynlabel_dir, "/");
    strcat(dynlabel_dir, DYNLABELFILE);
    fp = fopen(dynlabel_dir,"rb");
    fread(&len, sizeof(int), 1, fp);
    for (int i = 0; i < len; i++){
        fread(&temp_int, sizeof(int), 1, fp);
        ivec_append(&wct->dynlabel.index, temp_int);
    }
    fread(&len, sizeof(int), 1, fp);
    for (int i = 0; i < len; i++){
        fread(&temp_int, sizeof(int), 1, fp);
        ivec_append(&wct->dynlabel.value, temp_int);
    }
    fclose(fp);

    // FeatureIndex Load
    strcpy(featureindex_dir, core_dir);
    char_prepend(featureindex_dir, COREDIR);
    strcat(featureindex_dir, "/");
    strcat(featureindex_dir, FEATUREINDEXFILE);
    fp = fopen(featureindex_dir,"rb");
    fread(&len, sizeof(int), 1, fp);
    for (int i = 0; i < len; i++){
        fread(&temp_double, sizeof(double), 1, fp);
        dvec_append(&wct->featureindex, temp_double);
    }
    fclose(fp);
}

void wct_delete (char *filename) {
    char core_dir[64];
    char meta_dir[64];
    char weight_dir[64];
    char feat_dir[64];
    char label_dir[64];
    char dynlabel_dir[64];
    char featureindex_dir[64];

    strcpy(core_dir, filename);
    char_prepend(core_dir, COREDIR);
    strcat(core_dir, "/");

    strcpy(meta_dir, core_dir);
    strcpy(weight_dir, core_dir);
    strcpy(feat_dir, core_dir);
    strcpy(label_dir, core_dir);
    strcpy(dynlabel_dir, core_dir);
    strcpy(featureindex_dir, core_dir);
    strcat(meta_dir, METAFILE);
    strcat(weight_dir, WEIGHTFILE);
    strcat(feat_dir, FEATURESFILE);
    strcat(label_dir, LABELFILE);
    strcpy(dynlabel_dir, DYNLABELFILE);
    strcpy(featureindex_dir, FEATUREINDEXFILE);

    remove(meta_dir);
    remove(weight_dir);
    remove(feat_dir);
    remove(label_dir);
    remove(dynlabel_dir);
    remove(featureindex_dir);
    rmdir(core_dir);
}

void wct_import (wct_t *wct, char *filename) {
    DECLARE_TIMER(timer);
    START_TIMER(timer);

    char filedir[32];
    strcpy(filedir, filename);
    char_prepend(filedir, RESDIR);
    strcat(filedir, ".csv");
    int table_line_count = file_linecount(filedir);

    char metadir[32];
    strcpy(metadir, filename);
    strcat(metadir, ".ini");
    char_prepend(metadir, RESDIR);

    FILE* fp = fopen(metadir, "r");
    if (fp == NULL) {
        // meta_import(&wct->meta, CONFIGDIR);
        log_error("WCT Structure Not Correct, Missing Meta. Do you mean to use wct_import_legacy()?\n");
        wct_exit(WCT_INVALID_STRUCT);
    } else {
        meta_import(&wct->meta, metadir);
    }
    fclose(fp);

    int needed_row = 1 + wct->meta.features_len + wct->meta.label_maxlen;
    if (csv_headercount(filedir, ",") != needed_row) {
        log_error("WCT Structure Not Correct, Missing Weight. Please Fix Your WCT Nodes, Or Use Different Import Function!\n");
        wct_exit(WCT_INVALID_STRUCT);
    }

    dvec_init(&wct->weight);
    dtab_init(&wct->features);
    dtab_init(&wct->label);
    dvec_init(&wct->featureindex);
    ivec_init(&wct->dynlabel.index);
    ivec_init(&wct->dynlabel.value);

    dtab_t tempTab;
    dtab_init(&tempTab);
    dtab_read_csv(&tempTab, filedir, ",");
    STOP_TIMER(timer);

    for (int r = 0; r < table_line_count; r++) {
        START_TIMER(timer);

        dvec_append(&wct->weight, dtab_get(&tempTab, r, 0));
        dtab_add_row(&wct->features);
        for (int c = 1; c <= wct->meta.features_len; c++)
            dtab_append_col(&wct->features, r, dtab_get(&tempTab, r, c));
        dtab_add_row(&wct->label);
        for (int c = 0; c < wct->meta.label_maxlen; c++)
            dtab_append_col(&wct->label, r, dtab_get(&tempTab, r, wct->meta.features_len + c + 1));

        dvec_append(&wct->featureindex, 0.0);
        ivec_append(&wct->dynlabel.value, 0);

        STOP_TIMER(timer);
    }
    wct_save(wct);

    log_info("Loaded WCT Nodes: %d\n", wct->weight.size);
    log_info("Overall wct_import() took: %f; Average per loop took: %f\n", GET_TIMER(timer), GET_AVERAGE_TIMER(timer));
    dtab_free(&tempTab);
}

void wct_import_legacy (wct_t *wct, cfg_t cfg, int features_count, int time_shift, int label_count, double (*initializer_function)(), int type) {
    DECLARE_TIMER(timer);
    START_TIMER(timer);

    char filename[32];
    sprintf(filename, "%d-%dT", features_count, time_shift);

    char filedir[32];
    strcpy(filedir, filename);
    char_prepend(filedir, RESDIR);
    strcat(filedir, ".csv");
    int table_line_count = file_linecount(filedir);

    wct->meta.ready = 1;
    wct->meta.type = type;
    wct->meta.major = 0;
    wct->meta.minor = 0;
    wct->meta.patch = 1;
    wct->meta.num_optimize = 0;
    wct->meta.num_trained = 0;
    wct->meta.last_wr = 0.0;
    wct->meta.last_train = time(NULL);
    wct->meta.label_maxlen = label_count;
    wct->meta.islabelfitted = 0;
    wct->meta.islabelencoded = 0;
    wct->meta.timeseries_shift = time_shift;
    wct->meta.range_step = cfg.default_range_step;
    wct->meta.range_start = cfg.default_range_start;
    wct->meta.range_end = cfg.default_range_end;
    wct->meta.features_len = features_count;
    wct->meta.similarity = cfg.default_similarity;
    wct->meta.min_pattern_found = cfg.default_min_pattern_found;
    wct->meta.min_prediction_rate = cfg.default_min_prediction_rate;

        #pragma GCC diagnostic push
        #pragma GCC diagnostic ignored "-Wint-conversion"
        #pragma GCC diagnostic ignored "-Wincompatible-pointer-types"
            strcpy(&wct->meta.asset_code, "UNDEFINED");
            strcpy(&wct->meta.core, wct_name(wct));
        #pragma GCC diagnostic pop

    dvec_init(&wct->weight);
    dtab_init(&wct->features);
    dtab_init(&wct->label);
    dvec_init(&wct->featureindex);
    ivec_init(&wct->dynlabel.index);
    ivec_init(&wct->dynlabel.value);

    dtab_t tempTab;
    dtab_init(&tempTab);
    dtab_read_csv(&tempTab, filedir, ",");
    STOP_TIMER(timer);

    for (int r = 0; r < table_line_count; r++) {
        START_TIMER(timer);

        dvec_append(&wct->weight, initializer_function());
        dtab_add_row(&wct->features);
        for (int c = 0; c < features_count; c++)
            dtab_append_col(&wct->features, r, dtab_get(&tempTab, r, c));
        dtab_add_row(&wct->label);
        for (int c = 0; c < label_count; c++)
            dtab_append_col(&wct->label, r, dtab_get(&tempTab, r, features_count + c));

        dvec_append(&wct->featureindex, 0.0);
        ivec_append(&wct->dynlabel.value, 0);

        STOP_TIMER(timer);
    }
    wct_save(wct);

    log_info("Loaded WCT Nodes: %d\n", wct->weight.size);
    log_info("Overall wct_import_legacy() took: %f; Average per loop took: %f\n", GET_TIMER(timer), GET_AVERAGE_TIMER(timer));
    dtab_free(&tempTab);
}

void wct_create_from_csv (wct_t *wct, cfg_t cfg, char *filename, int index_start, int index_end, double (*initializer_function)(), int type) {
    DECLARE_TIMER(timer);
    START_TIMER(timer);

    char filedir[32];
    strcpy(filedir, filename);
    char_prepend(filedir, RESDIR);
    int table_line_count = file_linecount(filedir);
    int headercount = csv_headercount(filedir, ",");

    int features_count = index_end - index_start + 1;
    int label_count = headercount - features_count;

    wct->meta.ready = 1;
    wct->meta.type = type;
    wct->meta.major = 0;
    wct->meta.minor = 0;
    wct->meta.patch = 1;
    wct->meta.num_optimize = 0;
    wct->meta.num_trained = 0;
    wct->meta.last_wr = 0.0;
    wct->meta.last_train = time(NULL);
    wct->meta.label_maxlen = label_count;
    wct->meta.islabelfitted = 0;
    wct->meta.islabelencoded = 0;
    wct->meta.timeseries_shift = cfg.default_timeseries_shift;
    wct->meta.range_step = cfg.default_range_step;
    wct->meta.range_start = cfg.default_range_start;
    wct->meta.range_end = cfg.default_range_end;
    wct->meta.features_len = features_count;
    wct->meta.similarity = cfg.default_similarity;
    wct->meta.min_pattern_found = cfg.default_min_pattern_found;
    wct->meta.min_prediction_rate = cfg.default_min_prediction_rate;

        #pragma GCC diagnostic push
        #pragma GCC diagnostic ignored "-Wint-conversion"
        #pragma GCC diagnostic ignored "-Wincompatible-pointer-types"
            strcpy(&wct->meta.asset_code, "UNDEFINED");
            strcpy(&wct->meta.core, wct_name(wct));
        #pragma GCC diagnostic pop

    dvec_init(&wct->weight);
    dtab_init(&wct->features);
    dtab_init(&wct->label);
    dvec_init(&wct->featureindex);
    ivec_init(&wct->dynlabel.index);
    ivec_init(&wct->dynlabel.value);

    dtab_t tempTab;
    dtab_init(&tempTab);
    dtab_read_csv(&tempTab, filedir, ",");
    STOP_TIMER(timer);

    for (int r = 0; r < table_line_count; r++) {
        START_TIMER(timer);

        dvec_append(&wct->weight, initializer_function());
        dtab_add_row(&wct->features);
        for (int c = 0; c < features_count; c++)
            dtab_append_col(&wct->features, r, dtab_get(&tempTab, r, c));
        dtab_add_row(&wct->label);
        for (int c = 0; c < label_count; c++)
            dtab_append_col(&wct->label, r, dtab_get(&tempTab, r, features_count + c));

        dvec_append(&wct->featureindex, 0.0);
        ivec_append(&wct->dynlabel.value, 0);

        STOP_TIMER(timer);
    }
    wct_save(wct);

    log_info("Loaded WCT Nodes: %d\n", wct->weight.size);
    log_info("Overall wct_create_from_csv() took: %f; Average per loop took: %f\n", GET_TIMER(timer), GET_AVERAGE_TIMER(timer));
    dtab_free(&tempTab);
}

typedef struct {
    wct_t *wct;
    cfg_t cfg;
    char code[10];
    double (*initializer_function)();
    // int *timeseq;
    // int timeseq_len;
    double *seq;
    int seq_len;
    int overwrite;
    int type;
} wct_cfs_args;

void wct_cfs_base (wct_t *wct, cfg_t cfg, char *assetcode, double (*initializer_function)(), double *seq, int seq_len, int overwrite, int type) {
    DECLARE_TIMER(seq);
    START_TIMER(seq);

    dvec_init(&wct->weight);
    dtab_init(&wct->features);
    dtab_init(&wct->label);
    dvec_init(&wct->featureindex);
    ivec_init(&wct->dynlabel.index);
    ivec_init(&wct->dynlabel.value);

    int shift = cfg.default_timeseries_shift;
    int step = cfg.default_range_step;
    int start = cfg.default_range_start;
    int end = cfg.default_range_end;
    int len = cfg.default_features_len;
    int label_len = cfg.default_label_maxlen;

    log_info("Creating WCT-Core, Mode: %s, For: %s, With Parameter: Shift: %d, Step (for NO_AVERAGING_MODE): %d, Features_Len: %d, Label_Len: %d, Sequence To Be Processed: %d\n", 
        (end - start) ? "Averaging-Range-Mode" : "No-Averaging-Mode", assetcode, shift, step, len, label_len, seq_len);

    int mode = (end - start) ? AVERAGING_RANGE_MODE : NO_AVERAGING_MODE;
    int len_factor = mode ? end + label_len : step + label_len;
    int x = seq_len - len_factor;
    int y = len + 1;
    int outcome_index = mode ? end - start + 1 : 1;
    double outcome = 0.0;
    int row = 0;

    if (y >= x) {
        log_error("Data Too Small, Please Provide More Seq Data!\n");
        wct_exit(WCT_SMALL_DATA);
    }

    while (y < x) {
        double *pattern = malloc(sizeof(double) * len);
        double *result = malloc(sizeof(double) * label_len);
        double *outcomerange = malloc(sizeof(double) * outcome_index);

        for (int i = 0; i < len; i++) {
            pattern[i] = math_percentchange(seq[y-len], seq[y-len+i+1]);
        }

        if (mode == AVERAGING_RANGE_MODE) {
            for (int i = 0; i < label_len; i++) {
                slice_d(seq, outcomerange, y+start+i, y+end+i);
                outcome = AVG_d(outcomerange, outcome_index);
                result[i] = math_percentchange(seq[y-len], outcome);
            }
        } else if (mode == NO_AVERAGING_MODE) {
            for (int i = 0; i < label_len; i++) {
                result[i] = math_percentchange(seq[y-len], seq[y+step+i]);
            }
        }
        dvec_append(&wct->weight, initializer_function());

        dtab_add_row(&wct->features);
        for (int i = 0; i < len; i++)
            dtab_append_col(&wct->features, row, pattern[i]);

        dtab_add_row(&wct->label);
        for (int i = 0; i < label_len; i++)
            dtab_append_col(&wct->label, row, result[i]);

        dvec_append(&wct->featureindex, seq[y-len]);
        ivec_append(&wct->dynlabel.value, 0);

        free(pattern);
        free(result);
        free(outcomerange);

        y += 1;
        row++;
    }

    wct->meta.ready = 1;
    wct->meta.type = type;
    wct->meta.major = 0;
    wct->meta.minor = 0;
    wct->meta.patch = 1;
    wct->meta.num_optimize = 0;
    wct->meta.num_trained = 0;
    wct->meta.last_wr = 0.0;
    wct->meta.last_train = time(NULL);
    wct->meta.label_maxlen = label_len;
    wct->meta.islabelfitted = 0;
    wct->meta.islabelencoded = 0;
    wct->meta.timeseries_shift = shift;
    wct->meta.range_step = step;
    wct->meta.range_start = start;
    wct->meta.range_end = end;
    wct->meta.features_len = len;
    wct->meta.similarity = cfg.default_similarity;
    wct->meta.min_pattern_found = cfg.default_min_pattern_found;
    wct->meta.min_prediction_rate = cfg.default_min_prediction_rate;

        #pragma GCC diagnostic push
        #pragma GCC diagnostic ignored "-Wint-conversion"
        #pragma GCC diagnostic ignored "-Wincompatible-pointer-types"
            strcpy(&wct->meta.core, wct_name(wct));
            strcpy(&wct->meta.asset_code, assetcode);
        #pragma GCC diagnostic pop

    STOP_TIMER(seq);
    wct_save(wct);

    log_info("Created WCT Nodes: %d\n", wct->weight.size);
    log_info("Overall wct_create_from_sequence() took: %f\n\n", GET_TIMER(seq));
}

void wct_cfs_wrapper (wct_cfs_args a) {
    char a_code[10];
    if (a.code[0] == '\0') {
        strcpy(a_code, "UNDEFINED");
    } else {
        strcpy(a_code, a.code);
    }
    if (a.cfg.ready != 1) config_init(&a.cfg);
    double (*a_initializer)() = a.initializer_function ? a.initializer_function : wct_pointone_init;
    int a_seq_len = a.seq_len ? a.seq_len : 0;
    int a_overwrite = a.overwrite ? a.overwrite : 0;
    int a_type = a.type ? a.type : CUSTOM;

    int a_shift = a.cfg.default_timeseries_shift;
    int a_step = a.cfg.default_range_step;
    int a_start = a.cfg.default_range_start;
    int a_end = a.cfg.default_range_end;
    int a_len = a.cfg.default_features_len;
    int a_label = a.cfg.default_label_maxlen;

    if (a.wct->meta.ready == 1) {
        log_error("WCT Already Initialized, Use Uninitialized WCT Variable!\n");
        wct_exit(WCT_NEED_EMPTY);
    }
    if (a_seq_len == 0) {
        log_error("Please Provide Valid .seq_len Argument!\n");
        wct_exit(WCT_INVALID_ARGUMENT);
    }
    char core_dir[64];
    strcpy(core_dir, core_name(a_code, a_type, a_shift, a_step, a_start, a_end, a_len, a_label));
    char_prepend(core_dir, COREDIR);
    DIR* dir = opendir(core_dir);
    if (dir) { /* WCT-Core exists, Display Warning If Overwrite Argument is 0, and Overwrite Old Core Otherwise */
        if (a_overwrite > 0) {
            wct_cfs_base(a.wct, a.cfg, a_code, a_initializer, a.seq, a_seq_len, a_overwrite, a_type);
        } else {
            log_warn("WCT-Core Directory Exist, Cancel Creating New Core!\n");
        }
        closedir(dir);
    } else if (ENOENT == errno) { /* WCT-Core Does Not Exist, Safe To Proceed Creating New WCT. */
        wct_cfs_base(a.wct, a.cfg, a_code, a_initializer, a.seq, a_seq_len, a_overwrite, a_type);
    } else { /* opendir() failed for some other reason. */
        log_error("Opendir() Failed For Some Reason!\n");
        wct_exit(WCT_ERROR);
    }
}

void wct_meta_save(wct_t *wct) {
    if (wct->meta.ready != 1) {
        log_error("WCT is Not Initialized, Can't Proceed With Meta Operation!\n");
        wct_exit(WCT_NOT_READY);
    }
    mkdir(COREDIR, 0777);
    char meta_dir[64];
    strcpy(meta_dir, wct_name(wct));
    char_prepend(meta_dir, COREDIR);
    strcat(meta_dir, "/");
    mkdir(meta_dir, 0777);
    strcat(meta_dir, METAFILE);
    FILE *fp;
    fp = fopen(meta_dir,"wb");
    fwrite(&wct->meta.ready, sizeof(int), 1, fp);
    fwrite(&wct->meta.type, sizeof(int), 1, fp);
    fwrite(&wct->meta.major, sizeof(int), 1, fp);
    fwrite(&wct->meta.minor, sizeof(int), 1, fp);
    fwrite(&wct->meta.patch, sizeof(int), 1, fp);
    fwrite(&wct->meta.num_optimize, sizeof(int), 1, fp);
    fwrite(&wct->meta.num_trained, sizeof(int), 1, fp);
    fwrite(&wct->meta.last_wr, sizeof(double), 1, fp);
    fwrite(&wct->meta.last_train, sizeof(long int), 1, fp);
    for (int i = 0; i < 64; i++)
        fwrite(&wct->meta.core[i], sizeof(char), 1, fp);
    fwrite(&wct->meta.label_maxlen, sizeof(int), 1, fp);
    fwrite(&wct->meta.islabelfitted, sizeof(int), 1, fp);
    fwrite(&wct->meta.islabelencoded, sizeof(int), 1, fp);
    for (int i = 0; i < 10; i++)
        fwrite(&wct->meta.asset_code[i], sizeof(char), 1, fp);
    fwrite(&wct->meta.timeseries_shift, sizeof(int), 1, fp);
    fwrite(&wct->meta.range_step, sizeof(int), 1, fp);
    fwrite(&wct->meta.range_start, sizeof(int), 1, fp);
    fwrite(&wct->meta.range_end, sizeof(int), 1, fp);
    fwrite(&wct->meta.features_len, sizeof(int), 1, fp);
    fwrite(&wct->meta.similarity, sizeof(double), 1, fp);
    fwrite(&wct->meta.min_pattern_found, sizeof(int), 1, fp);
    fwrite(&wct->meta.min_prediction_rate, sizeof(double), 1, fp);
    fclose(fp);
}

void wct_weight_save (wct_t *wct) {
    FILE *fp;

    char weight_dir[64];
    strcpy(weight_dir, wct_name(wct));
    char_prepend(weight_dir, COREDIR);
    strcat(weight_dir, "/");
    strcat(weight_dir, WEIGHTFILE);
    mkdir(COREDIR, 0777);

    fp = fopen(weight_dir,"wb");
    fwrite(&wct->weight.size, sizeof(int), 1, fp);
    for (int i = 0; i < wct->weight.size; i++){
        fwrite(&wct->weight.data[i], sizeof(double), 1, fp);
    }
    fclose(fp);
}

void wct_weight_load (wct_t *wct) {
    FILE *fp;
    int len = 0;
    double temp_double = 0.0;
    char weight_dir[64];

    strcpy(weight_dir, wct_name(wct));
    char_prepend(weight_dir, COREDIR);
    strcat(weight_dir, "/");
    strcat(weight_dir, WEIGHTFILE);

    if (wct->meta.ready == 1) {
        dvec_free(&wct->weight);
    }
    dvec_init(&wct->weight);

    fp = fopen(weight_dir,"rb");
    fread(&len, sizeof(int), 1, fp);
    for (int i = 0; i < len; i++){
        fread(&temp_double, sizeof(double), 1, fp);
        dvec_append(&wct->weight, temp_double);
    }
    fclose(fp);
}

void wct_weight_reinit (wct_t *wct, double (*initializer_function)()) {
    FILE *fp;
    int len = wct->weight.size;
    double temp_double = 0.0;
    char weight_dir[64];

    strcpy(weight_dir, wct_name(wct));
    char_prepend(weight_dir, COREDIR);
    strcat(weight_dir, "/");
    strcat(weight_dir, WEIGHTFILE);
    mkdir(COREDIR, 0777);

    if (wct->meta.ready == 1) {
        dvec_free(&wct->weight);
    }
    dvec_init(&wct->weight);

    fp = fopen(weight_dir,"wb");
    fwrite(&len, sizeof(int), 1, fp);
    for (int i = 0; i < len; i++){
        dvec_append(&wct->weight, initializer_function());
        fwrite(&wct->weight.data[i], sizeof(double), 1, fp);
    }
    fclose(fp);
}

/*
void wct_weight_scaler (wct_t *wct) {
    // (value * 2) - 1 is Scaling From -1 to 1;

    wct->meta.minor++;
    wct->meta.patch = 0;

    double min = min_d(wct->weight.data, wct->weight.size);
    double max = max_d(wct->weight.data, wct->weight.size);
    for (int i = 0; i < wct->weight.size; i++)
        wct->weight.data[i] = (((wct->weight.data[i] - min) / (max - min)) * 2) - 1;
}
*/

void wct_weight_scaler (wct_t *wct) {
    wct->meta.minor++;
    wct->meta.patch = 0;

    double min = min_d(wct->weight.data, wct->weight.size);
    double max = max_d(wct->weight.data, wct->weight.size);
    for (int i = 0; i < wct->weight.size; i++)
        wct->weight.data[i] = ((wct->weight.data[i] - min) / (max - min));
}

void wct_dynlabel_save (wct_t *wct) {
    FILE *fp;

    char dynlabel_dir[64];
    strcpy(dynlabel_dir, wct_name(wct));
    char_prepend(dynlabel_dir, COREDIR);
    strcat(dynlabel_dir, "/");
    strcat(dynlabel_dir, DYNLABELFILE);
    mkdir(COREDIR, 0777);

    fp = fopen(dynlabel_dir,"wb");
    int indexlen = wct->dynlabel.index.size;
    fwrite(&indexlen, sizeof(int), 1, fp);
    for (int i = 0; i < indexlen; i++){
        fwrite(&wct->dynlabel.index.data[i], sizeof(int), 1, fp);
    }
    int valuelen = wct->dynlabel.value.size;
    fwrite(&valuelen, sizeof(int), 1, fp);
    for (int i = 0; i < valuelen; i++){
        fwrite(&wct->dynlabel.value.data[i], sizeof(int), 1, fp);
    }
    fclose(fp);
}

void wct_dynlabel_load (wct_t *wct) {
    FILE *fp;
    int len = 0;
    int temp_int = 0;
    char dynlabel_dir[64];

    strcpy(dynlabel_dir, wct_name(wct));
    char_prepend(dynlabel_dir, COREDIR);
    strcat(dynlabel_dir, "/");
    strcat(dynlabel_dir, DYNLABELFILE);

    if (wct->meta.ready == 1) {
        ivec_free(&wct->dynlabel.index);
        ivec_free(&wct->dynlabel.value);
    }
    ivec_init(&wct->dynlabel.index);
    ivec_init(&wct->dynlabel.value);

    fp = fopen(dynlabel_dir,"rb");
    fread(&len, sizeof(int), 1, fp);
    for (int i = 0; i < len; i++){
        fread(&temp_int, sizeof(int), 1, fp);
        ivec_append(&wct->dynlabel.index, temp_int);
    }
    fread(&len, sizeof(int), 1, fp);
    for (int i = 0; i < len; i++){
        fread(&temp_int, sizeof(int), 1, fp);
        ivec_append(&wct->dynlabel.value, temp_int);
    }
    fclose(fp);
}

void wct_dynlabel_reset (wct_t *wct) {
    FILE *fp;
    int temp_int = 0;
    int valuelen = wct->dynlabel.value.size;
    char dynlabel_dir[64];

    strcpy(dynlabel_dir, wct_name(wct));
    char_prepend(dynlabel_dir, COREDIR);
    strcat(dynlabel_dir, "/");
    strcat(dynlabel_dir, DYNLABELFILE);
    mkdir(COREDIR, 0777);

    if (wct->meta.ready == 1) {
        ivec_free(&wct->dynlabel.index);
        ivec_free(&wct->dynlabel.value);
    }
    ivec_init(&wct->dynlabel.index);
    ivec_init(&wct->dynlabel.value);

    fp = fopen(dynlabel_dir,"wb");
    int indexlen = wct->dynlabel.index.size;
    fwrite(&indexlen, sizeof(int), 1, fp);

    // for (int i = 0; i < indexlen; i++){
    //     fwrite(&wct->dynlabel.index.data[i], sizeof(int), 1, fp);
    // }
    
    fwrite(&valuelen, sizeof(int), 1, fp);
    for (int i = 0; i < valuelen; i++){
        ivec_append(&wct->dynlabel.value, 0);
        fwrite(&wct->dynlabel.value.data[i], sizeof(int), 1, fp);
    }
    fclose(fp);
}

void wct_deinit(wct_t *wct) {
    dvec_free(&wct->weight);
    dtab_free(&wct->features);
    dtab_free(&wct->label);
    dvec_free(&wct->featureindex);
    ivec_free(&wct->dynlabel.index);
    ivec_free(&wct->dynlabel.value);
}

void wct_copy(wct_t *from, wct_t *to) {
    if (to->meta.ready == 1) {
        wct_deinit(to);
    }
    dvec_copy(&from->weight, &to->weight);
    dtab_copy(&from->features, &to->features);
    dtab_copy(&from->label, &to->label);
    dvec_copy(&from->featureindex, &to->featureindex);
    ivec_copy(&from->dynlabel.index, &to->dynlabel.index);
    ivec_copy(&from->dynlabel.value, &to->dynlabel.value);
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdiscarded-qualifiers"

	void wct_make_default_core(wct_t *wct) {
		config_modify(CONFIGDIR, "default", "core", wct_name(wct));
	}

#pragma GCC diagnostic push

void wct_clean_default_core() {
	config_modify(CONFIGDIR, "default", "core", "0");
}

void wct_make_default_email(char *smtp, char *port, char *user, char *pass, char *send_to) {
	config_modify(CONFIGDIR, "email", "smtp", smtp);
	config_modify(CONFIGDIR, "email", "port", port);
	config_modify(CONFIGDIR, "email", "user", user);
	config_modify(CONFIGDIR, "email", "pass", pass);
	config_modify(CONFIGDIR, "email", "send_to", send_to);
}

void wct_clean_default_email() {
	config_modify(CONFIGDIR, "email", "smtp", "0");
	config_modify(CONFIGDIR, "email", "port", "0");
	config_modify(CONFIGDIR, "email", "user", "0");
	config_modify(CONFIGDIR, "email", "pass", "0");
	config_modify(CONFIGDIR, "email", "send_to", "0");
}

void wct_make_default_iqoption(char *user, char *pass) {
	config_modify(CONFIGDIR, "iqoption", "user", user);
	config_modify(CONFIGDIR, "iqoption", "pass", pass);
}

void wct_clean_default_iqoption() {
	config_modify(CONFIGDIR, "iqoption", "user", "0");
	config_modify(CONFIGDIR, "iqoption", "pass", "0");
}

void wcthelper_normalize (double *array, int len) {
    double normalize_start_factor = array[0];
    for (int i = 1; i < len; i++) {
        array[i - 1] = math_percentchange(normalize_start_factor, array[i]);
    }
}

void wcthelper_denormalize (double *array, int len, double startpoint) {
    if (startpoint == 0.0) {
        log_warn("Startpoint is 0.0, Will Proceed With startpoint = 1.0!\n");
        startpoint = 1.0;
    }
    for (int i = 0; i < len; i++) {
        array[i] = startpoint + (startpoint * (array[i] / 100));
    }
}

void wcthelper_get_sequence (dvec_t *data) {
    if (data->size == 0) {
        log_error("Data is Not Ready, Initialize Your Vector Data!\n");
        wct_exit(WCT_INVALID_DATA);
    }
    wcthelper_normalize(data->data, data->size);
    dvec_pop(data);
}

void wcthelper_get_test_sequence_data (wct_t *wct, cfg_t cfg, dvec_t *dest, dvec_t *source, int idx) {
    int index = idx + 1;
    if (cfg.ready != 1) {
        log_error("Config is Not Initialized, Use config_init()!\n");\
        wct_exit(WCT_INVALID_CONFIG);
    }
    if (wct->meta.ready != 1) {
        log_error("WCT Is Not Initialized, Use wct_load! or wct_import()!\n");
        wct_exit(WCT_NOT_READY);
    }
    if (index + wct->meta.features_len + 1 > source->size) {
        log_error("Index Out Of Bounds For Retrieving Current Pattern!\n");
        wct_exit(WCT_OUT_OF_BOUNDS);
    }
    if (source->size == 0) {
        log_error("Data is Not Ready, Initialize Your Vector Source Data!\n");
        wct_exit(WCT_INVALID_DATA);
    }
    if (dest->ready == 1) dvec_free(dest);
    dvec_init(dest);
    for (int i = index; i < index + wct->meta.features_len + 1; i++) {
        dvec_append(dest, source->data[i]);
    }
    wcthelper_normalize(dest->data, dest->size);
    dvec_pop(dest);
}

typedef struct {
    wct_t *wct;
    cfg_t cfg;
    dvec_t *source;
    int idx;
    int label_step;
} get_test_label_args;

double wcthelper_get_test_sequence_label_base (wct_t *wct, cfg_t cfg, dvec_t *source, int idx, int label_step) {
    int index = idx + 1;
    double ret = 0.0;
    if ((cfg.default_range_end - cfg.default_range_start) == 0) {
        ret = source->data[index + wct->meta.features_len + label_step + 1];
    } else {
        int counter = 0;
        for (int i = index + wct->meta.features_len + cfg.default_range_start; i <= index + wct->meta.features_len + cfg.default_range_end; i += cfg.default_range_step) {
            ret += source->data[i];
            counter++;
        }
        ret = ret / (double)counter;
    }
    ret = math_percentchange(source->data[index], ret);
    return ret;
}

double wcthelper_get_test_sequence_label_wrapper (get_test_label_args a) {
    int a_idx = a.idx ? a.idx : 0;
    a_idx = a.idx + 1;
    if (a.cfg.ready != 1) {
        config_init(&a.cfg);
    }
    if (a.wct->meta.ready != 1) {
        log_error("WCT Is Not Initialized, Use wct_load! or wct_import()!\n");
        wct_exit(WCT_NOT_READY);
    }
    if (a_idx + a.wct->meta.features_len + 1 > a.source->size) {
        log_error("Index Out Of Bounds For Retrieving Current Pattern!\n");
        wct_exit(WCT_OUT_OF_BOUNDS);
    }
    if (a.source->size == 0) {
        log_error("Data is Not Ready, Initialize Your Vector Source Data!\n");
        wct_exit(WCT_INVALID_DATA);
    }
    int a_label_step = a.label_step ? a.label_step : a.cfg.default_range_step - 1;
    if (a_idx + a_label_step + a.wct->meta.features_len + 1 > a.source->size) {
        log_error("Index Step Out Of Bounds For Retrieving Label, Will Set Label_Step to 0!\n");
        a_label_step = 0;
    }

    double ret = wcthelper_get_test_sequence_label_base(a.wct, a.cfg, a.source, a_idx, a_label_step);
    return ret;
}

void wct_optimize (wct_t *wct) {
    if (wct->meta.ready != 1) {
        log_error("WCT Is Not Initialized, Use wct_load! or wct_import()!\n");
        wct_exit(WCT_NOT_READY);
    }

    log_info("Begin Optimizing WCT: %s\n", wct_name(wct));

    DECLARE_TIMER(optimize);
    START_TIMER(optimize);

    progressbar_t pb;
    progressbar_create(&pb, 0, wct->weight.size);

    register unsigned int foundcount = 0;
    double sim = wct->meta.similarity;
    register unsigned int min_found = wct->meta.min_pattern_found;
    int isCorrelated = 0;
    register unsigned int x = 0;
    register unsigned int i = 0;
    ivec_t index;
    ivec_init(&index);

    for (x = 0; x < wct->weight.size; x++) {
        progressbar_update(&pb);
        for (i = 0; i < wct->weight.size; i++) {
            if ((1 - math_manhattan_distance(wct->features.data[x], wct->features.data[i], wct->meta.features_len)) > sim) {
                foundcount++;
            }
            if (foundcount >= min_found) {
                isCorrelated = 1;
                break;
            }
        }
        if (x % 100 == 0) progressbar_print(&pb);
        if (isCorrelated == 0) ivec_append(&index, x);
        foundcount = 0;
        isCorrelated = 0;
    }

    progressbar_done(&pb);

    if (index.size > 0) {
        log_info("Uncorrelated Nodes To Be Optimized: %d\n", index.size);
        ivec_sort_asc(&index);

        log_info("Optimizing Weight... ");
        dvec_filter(&wct->weight, index.data, index.size);
        printf("Done\n");
        log_info("Optimizing Features... ");
        dtab_filter(&wct->features, index.data, index.size);
        printf("Done\n");
        log_info("Optimizing Label... ");
        dtab_filter(&wct->label, index.data, index.size);
        printf("Done\n");
        log_info("Optimizing FeatureIndex... ");
        dvec_filter(&wct->featureindex, index.data, index.size);
        printf("Done\n");
        log_info("Optimizing Dynlabel Value... ");
        ivec_filter(&wct->dynlabel.value, index.data, index.size);
        printf("Done\n");

        wct->meta.num_optimize++;
        wct_save(wct);

        ivec_free(&index);
        STOP_TIMER(optimize);
        log_info("Overall wct_optimize() took: %f\n\n", GET_TIMER(optimize));
    } else {
        log_info("WCT Already Optimized\n");
        ivec_free(&index);
        STOP_TIMER(optimize);
        log_info("Overall wct_optimize() took: %f\n\n", GET_TIMER(optimize));
    }
}

int dataframe_typename(dataframe_t df) {
    if (df.seq.ready == 1 && df.seq_test.ready != 1) return DF_SEQUENCE;
    if (df.seq.ready == 1 && df.seq_test.ready == 1) return DF_SEQUENCE_SPLITTED;
    if (df.x.ready == 1 && df.y.ready == 1 && df.x_test.ready != 1 && df.y_test.ready != 1) return DF_TABLE;
    if (df.x.ready == 1 && df.y.ready == 1 && df.x_test.ready == 1 && df.y_test.ready == 1) return DF_TABLE_SPLITTED;
    return DF_UNDEFINED;
}

void dataframe_init_sequence (dataframe_t *df, dvec_t data) {
    if (df->seq.ready == 1) dvec_free(&df->seq);
    dvec_copy(&data, &df->seq);
}

void dataframe_init_table (dataframe_t *df, dtab_t data, dtab_t label) {
    if (df->x.ready == 1) dtab_free(&df->x);
    if (df->y.ready == 1) dtab_free(&df->y);
    dtab_init(&df->x);
    dtab_init(&df->y);
    for (int r = 0; r < data.row; r++) {
        dtab_add_row(&df->x);
        for (int c = 0; c < data.col[r]; c++) {
            dtab_append_col(&df->x, r, data.data[r][c]);
        }
        dtab_add_row(&df->y);
        for (int c = 0; c < label.col[r]; c++) {
            dtab_append_col(&df->y, r, label.data[r][c]);
        }
    }
}

void dataframe_split_table (dataframe_t *df, int random, double ratio) {
    dtab_t data;
    dtab_t label;
    dtab_init(&data);
    dtab_init(&label);

    if (df->x.ready == 1) {

        for (int r = 0; r < df->x.row; r++) {
            dtab_add_row(&data);
            for (int c = 0; c < df->x.col[r]; c++) {
                dtab_append_col(&data, r, df->x.data[r][c]);
            }
            dtab_add_row(&label);
            for (int c = 0; c < df->y.col[r]; c++) {
                dtab_append_col(&label, r, df->y.data[r][c]);
            }
        }

        dtab_free(&df->x);
        dtab_free(&df->y);

        if (df->x_test.ready == 1) {
            dtab_free(&df->x_test);
            dtab_free(&df->y_test);
        }

    } else {
        log_error("Dataframe is not Loaded! Splitting Canceled!\n");
        return;
    }

    dtab_init(&df->x);
    dtab_init(&df->y);
    dtab_init(&df->x_test);
    dtab_init(&df->y_test);

    int trainlen = floor(ratio * data.row);
    int testlen = data.row - trainlen;
    int idx = 0;
    int counter = 0;

    ivec_t index;
    ivec_init(&index);

    for (int i = 0; i < data.row; i++) {
        ivec_append(&index, i);
    }

    if (random) {
        for (int i = 0; i < data.row; i++) {
            int j = i + rand() % (data.row - i);
            int temp = index.data[i];
            index.data[i] = index.data[j];
            index.data[j] = temp;
        }
    }

    counter = 0;

    // Training Data Splitter
    for (int i = 0; i < trainlen; i++) {
        idx = index.data[i];
        dtab_add_row(&df->x);
        for (int c = 0; c < data.col[idx]; c++) {
            dtab_append_col(&df->x, counter, data.data[idx][c]);
        }
        dtab_add_row(&df->y);
        for (int c = 0; c < label.col[idx]; c++) {
            dtab_append_col(&df->y, counter, label.data[idx][c]);
        }
        counter++;
    }

    counter = 0;

    // Test Data Splitter
    for (int i = trainlen; i < trainlen + testlen; i++) {
        idx = index.data[i];
        dtab_add_row(&df->x_test);
        for (int c = 0; c < data.col[idx]; c++) {
            dtab_append_col(&df->x_test, counter, data.data[idx][c]);
        }
        dtab_add_row(&df->y_test);
        for (int c = 0; c < label.col[idx]; c++) {
            dtab_append_col(&df->y_test, counter, label.data[idx][c]);
        }
        counter++;
    }

    dtab_free(&data);
    dtab_free(&label);
    ivec_free(&index);
}

void dataframe_split_sequence (dataframe_t *df, int random, double ratio) {
    if (random != 0) {
        log_warn("Since it's splitting Sequence data, .random argument will be ignored!\n");
    }

    dvec_t data;
    if (df->seq.ready == 1) {
        dvec_copy(&df->seq, &data);
        dvec_free(&df->seq);
        if (df->seq_test.ready == 1) dvec_free(&df->seq_test);
    }

    dvec_init(&df->seq);
    dvec_init(&df->seq_test);

    int trainlen = floor(ratio * data.size);
    int testlen = data.size - trainlen;

    // Training Data Splitter
    for (int i = 0; i < trainlen; i++) {
        dvec_append(&df->seq, data.data[i]);
    }

    // Test Data Splitter
    for (int i = trainlen; i < trainlen + testlen; i++) {
        dvec_append(&df->seq_test, data.data[i]);
    }

    if (data.ready == 1) {
        dvec_free(&data);
    }
}

void dataframe_train_test_split (dataframe_t *df, int random, double ratio) {
    if (dataframe_typename(*df) == DF_SEQUENCE) dataframe_split_sequence(df, random, ratio);
    else if (dataframe_typename(*df) == DF_TABLE) dataframe_split_table(df, random, ratio);
    else if ((dataframe_typename(*df) == DF_SEQUENCE_SPLITTED) || (dataframe_typename(*df) == DF_TABLE_SPLITTED)) {
        log_warn("Dataframe Already Splitted, Operation Ignored!\n");
        return;
    }
    else {
        log_error("Undefined Dataframe Behaviour\n");
        wct_exit(WCT_ERROR);
    }
}

void dataframe_free (dataframe_t *df) {
    if (df->seq.ready == 1) dvec_free (&df->seq);
    if (df->seq_test.ready == 1) dvec_free (&df->seq_test);
    if (df->x.ready == 1) dtab_free (&df->x);
    if (df->y.ready == 1) dtab_free (&df->y);
    if (df->x_test.ready == 1) dtab_free (&df->x_test);
    if (df->y_test.ready == 1) dtab_free (&df->y_test);
}

// Activation Function
void softmax (dvec_t *input) {
    double expsum = 0.0;
    for (int i = 0; i < input->size; i++) expsum += math_exp(input->data[i]);
    for (int i = 0; i < input->size; i++) {
        input->data[i] = math_exp(input->data[i]) / expsum;
    }
}

void standard (dvec_t *input) {
    double sum = 0;
    for (int i = 0; i < input->size; i++) sum += input->data[i];
    for (int i = 0; i < input->size; i++) input->data[i] = input->data[i] / sum;
}

void sigmoid (dvec_t *input) {
    for (int i = 0; i < input->size; i++) {
        input->data[i] = math_sigmoid(input->data[i]);
    }
}

void standard_and_softmax (dvec_t *input) {
    standard(input);
    softmax(input);
}

void standard_and_sigmoid (dvec_t *input) {
    standard(input);
    sigmoid(input);
}

void sigmoid_and_softmax (dvec_t *input) {
    sigmoid(input);
    softmax(input);
}

void no_activation (dvec_t *input) {
    return;
}

// Error Function
double mean_squared_error (dvec_t x, double y) {
    return math_mean_squared_error(x.data[0], y);
}

dvec_t vectorize_label (int label, int len) {
    dvec_t ret;
    dvec_init(&ret);
    for (int i = 0; i < len; i++) if (label == i) dvec_append(&ret, 1.000); else dvec_append(&ret, 0.000);
    return ret;
}

double cross_entropy (dvec_t x, double y) {
    dvec_t Y;
    Y = vectorize_label(y, x.size);
    double ret = math_cross_entropy(x.data, Y.data, x.size);
    dvec_free(&Y);
    return ret;
}

double error_not_found (dvec_t x, double y) {
    return -1;
}

// acc Function
double acc_binary (double predicted, double label, double last, double error) {
    if (predicted == -1) return -1;
    if (predicted == label) return 1.0; else return 0.0;
}

/*
double acc_binary_risefall (double predicted, double label, double last, double error) {
    if (predicted == -1) return -1;
    if ((last < label && predicted == 0) || (last > label && predicted == 1) || (last == label && predicted == 2))
        return 1.0;
    else return 0.0;
}
*/

double acc_return_error (double predicted, double label, double last, double error) {
    return error;
}

double acc_not_found (double predicted, double label, double last, double error) {
    return -1;
}

// Label Return Function
double label_risefall (double label, double last) {
    if (last < label) return 0.0; else if (last > label) return 1.0; else return 2.0;
}

double label_original (double label, double last) {
    return label;
}

double label_not_found (double label, double last) {
    return -1;
}

// Stochastic Gradient Descent Function
double gradient_function (double prediction, double label, double distance, double weight, double lr) {
    return (2 * (prediction - label) * (-distance) * lr);
}

double gradient_function_alternative (double prediction, double label, double distance, double weight, double lr) {
    return (2 * (prediction - label) * (- (label / (weight * weight))) * lr);
}

// Gradient Descent Operation
void gradient_op_classification (wct_t *wct, metrics_t metric, double lr, double (*gradient_func)()) {
    int label_col = metric.misc.label_col;
    dvec_t label;
    dvec_init(&label);
    label = vectorize_label(metric.true_label, metric.value.size);
    int ix;

    for (register unsigned int i = 0; i < metric.node.index.size; i++) {
        ix = wct->label.data[metric.node.index.data[i]][label_col];
        wct->weight.data[metric.node.index.data[i]] += gradient_func(metric.value.data[ix], label.data[ix], metric.node.kernel.data[i], wct->weight.data[metric.node.index.data[i]], lr); 
    }

    dvec_free(&label);
}

void gradient_op_regression (wct_t *wct, metrics_t metric, double lrx, double (*gradient_func)()) {
    int label_col = metric.misc.label_col;
    double lr = lrx * 1000;

    for (register unsigned int i = 0; i < metric.node.index.size; i++) {
        // wct->weight.data[metric.node.index.data[i]] += gradient_func(metric.value.data[0], wct->label.data[metric.node.index.data[i]][label_col], metric.node.kernel.data[i], wct->weight.data[metric.node.index.data[i]], lr); 
        if (wct->label.data[metric.node.index.data[i]][label_col] >= metric.value.data[0]) {
            wct->weight.data[metric.node.index.data[i]] += gradient_func(metric.value.data[0], metric.true_label, metric.node.kernel.data[i], wct->weight.data[metric.node.index.data[i]], lr); 
        }
        else if (wct->label.data[metric.node.index.data[i]][label_col] < metric.value.data[0]) {
            wct->weight.data[metric.node.index.data[i]] -= gradient_func(metric.value.data[0], metric.true_label, metric.node.kernel.data[i], wct->weight.data[metric.node.index.data[i]], lr); 
        }
    }
}

void gradient_op_risefall (wct_t *wct, metrics_t metric, double lr, double (*gradient_func)()) {
    int label_col = metric.misc.label_col;
    dvec_t label;
    dvec_init(&label);
    label = vectorize_label(metric.true_label, metric.value.size);
    int ix;

    for (register unsigned int i = 0; i < metric.node.index.size; i++) {
        if (wct->label.data[metric.node.index.data[i]][label_col] > wct->features.data[metric.node.index.data[i]][wct->meta.features_len - 1]) {
            wct->weight.data[metric.node.index.data[i]] += gradient_func(metric.value.data[0], label.data[0], metric.node.kernel.data[i], wct->weight.data[metric.node.index.data[i]], lr); 
        }
        else if (wct->label.data[metric.node.index.data[i]][label_col] < wct->features.data[metric.node.index.data[i]][wct->meta.features_len - 1]) {
            wct->weight.data[metric.node.index.data[i]] += gradient_func(metric.value.data[1], label.data[1], metric.node.kernel.data[i], wct->weight.data[metric.node.index.data[i]], lr); 
        }
        else {
            wct->weight.data[metric.node.index.data[i]] += gradient_func(metric.value.data[2], label.data[2], metric.node.kernel.data[i], wct->weight.data[metric.node.index.data[i]], lr); 
        }
    }
    dvec_free(&label);
}

void gradient_op_not_found (wct_t *wct, metrics_t metric, double lr, double (*gradient_func)()) {
    return;
}

// Better Acc Condition
int better_larger (double current_acc, double last_acc) {
    if (current_acc > last_acc) return 1; else return 0;
}

int better_smaller (double current_acc, double last_acc) {
    if (current_acc < last_acc) return 1; else return 0;
}

int better_not_found (double current_acc, double last_acc) {
    return -1;
}

void metrics_free (metrics_t *metric) {
    ivec_free(&metric->node.index);
    dvec_free(&metric->node.kernel);
    dvec_free(&metric->value);
}

int wct_class_count (wct_t *wct, int label_col) {
    if (wct->meta.ready != 1) {
        log_error("WCT Is Not Initialized, Use wct_load! or wct_import()!\n");
        wct_exit(WCT_NOT_READY);
    }
    dvec_t temp;
    dvec_init(&temp);
    for (int i = 0; i < wct->weight.size; i++) {
        if (dvec_search(&temp, wct->label.data[i][label_col]) == -1) dvec_append(&temp, wct->label.data[i][label_col]);
    }
    int ret = temp.size;
    dvec_free(&temp);
    return ret;
}

int wct_class_get (dvec_t value) {
    int ret = 0;
    if (value.size > 1) {
        if (dvec_count(&value, max_d(value.data, value.size)) == 1) ret = dvec_search(&value, max_d(value.data, value.size)); else ret = -1;
    } else ret = -2;
    return ret;
}

double wct_predicted_value_get (dvec_t value) {
    double ret = 0;
    if (value.size > 1) {
        if (dvec_count(&value, max_d(value.data, value.size)) == 1) ret = dvec_search(&value, max_d(value.data, value.size)); else ret = -1;
    } else ret = value.data[0];
    return ret;
}

// Label Encode Function
void onehot_bucket_int (wct_t *wct, int label_col) {
    ivec_t temp;
    ivec_init(&temp);

    for (int i = 0; i < wct->weight.size; i++) {
        if (ivec_search(&temp, wct->label.data[i][label_col]) == -1) {
            ivec_append(&temp, wct->label.data[i][label_col]);
        }
        for (int a = 0; a < temp.size; a++) {
            if (wct->label.data[i][label_col] == temp.data[a]) {
                wct->label.data[i][label_col] = ivec_search(&temp, temp.data[a]);
            }
        }
    }

    ivec_free(&temp);
}

void onehot_bucket_double (wct_t *wct, int label_col) {
    dvec_t temp;
    dvec_init(&temp);

    for (int i = 0; i < wct->weight.size; i++) {
        if (dvec_search(&temp, wct->label.data[i][label_col]) == -1) {
            dvec_append(&temp, wct->label.data[i][label_col]);
        }
        for (int a = 0; a < temp.size; a++) {
            if (wct->label.data[i][label_col] == temp.data[a]) {
                wct->label.data[i][label_col] = dvec_search(&temp, temp.data[a]);
            }
        }
    }

    dvec_free(&temp);
}

void onehot_risefall (wct_t *wct, int label_col) {
    for (int i = 0; i < wct->weight.size; i++) {
        if (wct->label.data[i][label_col] > wct->features.data[i][wct->meta.features_len - 1])
            wct->label.data[i][label_col] = 0;
        else if (wct->label.data[i][label_col] < wct->features.data[i][wct->meta.features_len - 1])
            wct->label.data[i][label_col] = 1;
        else wct->label.data[i][label_col] = 2;
    }
}

typedef struct {
    wct_t *wct;
    void (*encode_function)();
    int label_col;
} label_encode_args;

void wct_label_encode_base (wct_t *wct, void (*encode_function)(), int label_col) {
    if (label_col == -1) {
        int vmax = vecmax(wct->label.col, wct->label.row);
        for (int i = 0; i < vmax; i++) {
            encode_function(wct, i);
        }
    } else {
        encode_function(wct, label_col);
    }
}

void wct_label_encode_wrapper (label_encode_args a) {
    if (a.wct->meta.ready != 1) {
        log_error("WCT Is Not Initialized, Use wct_load! or wct_import()!\n");
        wct_exit(WCT_NOT_READY);
    }
    if (!a.encode_function) {
        log_error("wct_label_encode(): Please Specify Valid .encode_function Argument!\n");
        wct_exit(WCT_INVALID_ARGUMENT);
    }
    int a_label_col;
    if (a.label_col >= 0) a_label_col = a.label_col; else a_label_col = -1;
    wct_label_encode_base(a.wct, a.encode_function, a_label_col);
}

metrics_t wct_not_found_estimator (wct_t *wct, ivec_t index, dvec_t distance_arr, double *input, int input_len, int label_col, void (*activation_function)()) {
    /*
    Weighted Cognition Table Models to Return Metrics of Not Found Node!.
    metrics_t description:
        .node.count => zero;
        .misc.error_function => return -1;
        .value => zero;
        .misc.class => -1;
    */

    metrics_t ret;
    dvec_init(&ret.value);
    dvec_append(&ret.value, -1.0);

    ret.node.count = 0;
    ret.misc.error_function = error_not_found;
    ret.misc.acc_function = acc_not_found;
    ret.misc.better_acc_condition = better_not_found;
    ret.misc.label_return = label_not_found;
    ret.misc.class = -1;
    ret.misc.gradient_operation = gradient_op_not_found;

    return ret;
}

metrics_t wct_classification (wct_t *wct, ivec_t index, dvec_t distance_arr, double *input, int input_len, int label_col, void (*activation_function)()) {
    /*
    Weighted Cognition Table Models to Solve Classification Problems.
    metrics_t description:
        .node.count => similar features in wct Nodes count;
        .misc.error_function => Error function that should be Used for this Estimation
        .value => predicted value;
        .misc.class => Predicted Class;
    */

    int class_count = wct_class_count(wct, label_col);
    int ix = 0;

    metrics_t ret;
    dvec_init(&ret.value);

    while (ret.value.size < class_count) {
        dvec_append(&ret.value, 0);
    }

    double len_predicted = 0.0;

    for (register unsigned int i = 0; i < index.size; i++) {
        ix = wct->label.data[index.data[i]][label_col];
        ret.value.data[ix] += wct->weight.data[index.data[i]]; 
        len_predicted += wct->weight.data[index.data[i]];
    }

    // standard simplified
    for (int i = 0; i < ret.value.size; i++) 
        ret.value.data[i] = ret.value.data[i] / len_predicted;

    // dvec_scaler(&ret.value);
    activation_function(&ret.value);

    ret.node.count = index.size;
    ret.misc.error_function = cross_entropy;
    ret.misc.acc_function = acc_binary;
    ret.misc.better_acc_condition = better_larger;
    ret.misc.label_return = label_original;
    ret.misc.gradient_operation = gradient_op_classification;
    ret.misc.class = wct_class_get(ret.value);

    return ret;
}

metrics_t wct_classification_with_kernel (wct_t *wct, ivec_t index, dvec_t distance_arr, double *input, int input_len, int label_col, void (*activation_function)()) {
    /*
    Weighted Cognition Table Models to Solve Classification Problems, Weight Multiplied by Distance as Kernel.
    metrics_t description:
        .node.count => similar features in wct Nodes count;
        .misc.error_function => Error function that should be Used for this Estimation
        .value => predicted value;
        .misc.class => Predicted Class;
    */

    int class_count = wct_class_count(wct, label_col);
    int ix = 0;

    metrics_t ret;
    dvec_init(&ret.value);

    while (ret.value.size < class_count) {
        dvec_append(&ret.value, 0);
    }

    double len_predicted = 0.0;

    for (register unsigned int i = 0; i < index.size; i++) {
        ix = wct->label.data[index.data[i]][label_col];
        ret.value.data[ix] += wct->weight.data[index.data[i]] * distance_arr.data[i]; 
        len_predicted += wct->weight.data[index.data[i]] * distance_arr.data[i];
    }

    // standard simplified
    for (int i = 0; i < ret.value.size; i++) 
        ret.value.data[i] = ret.value.data[i] / len_predicted;

    // dvec_scaler(&ret.value);
    activation_function(&ret.value);

    ret.node.count = index.size;
    ret.misc.error_function = cross_entropy;
    ret.misc.acc_function = acc_binary;
    ret.misc.better_acc_condition = better_larger;
    ret.misc.label_return = label_original;
    ret.misc.gradient_operation = gradient_op_classification;
    ret.misc.class = wct_class_get(ret.value);

    return ret;
}

metrics_t wct_classification_unweighted (wct_t *wct, ivec_t index, dvec_t distance_arr, double *input, int input_len, int label_col, void (*activation_function)()) {
    /*
    Unweighted Cognition Table Models to Solve Classification Problems.
    metrics_t description:
        .node.count => similar features in wct Nodes count;
        .misc.error_function => Error function that should be Used for this Estimation
        .value => predicted value;
        .misc.class => Predicted Class;
    */

    int class_count = wct_class_count(wct, label_col);
    int ix = 0;

    metrics_t ret;
    dvec_init(&ret.value);

    while (ret.value.size < class_count) {
        dvec_append(&ret.value, 0);
    }

    double len_predicted = 0.0;

    for (register unsigned int i = 0; i < index.size; i++) {
        ix = wct->label.data[index.data[i]][label_col];
        ret.value.data[ix]++; 
        len_predicted++;
    }

    // standard simplified
    for (int i = 0; i < ret.value.size; i++) 
        ret.value.data[i] = ret.value.data[i] / len_predicted;

    // dvec_scaler(&ret.value);
    activation_function(&ret.value);

    ret.node.count = index.size;
    ret.misc.error_function = cross_entropy;
    ret.misc.acc_function = acc_binary;
    ret.misc.better_acc_condition = better_larger;
    ret.misc.label_return = label_original;
    ret.misc.gradient_operation = gradient_op_classification;
    ret.misc.class = wct_class_get(ret.value);

    return ret;
}

metrics_t wct_regression (wct_t *wct, ivec_t index, dvec_t distance_arr, double *input, int input_len, int label_col, void (*activation_function)()) {
    /*
    Weighted Cognition Table Models to Solve Regression Problems.
    metrics_t description:
        .node.count => similar features in wct Nodes count;
        .misc.error_function => Error function that should be Used for this Estimation
        .value => predicted value;
        .misc.class => Always (-2); 
                  Not a Classification Problem!
    */

    double predicted_value = 0.0;
    double len_predicted = 0.0;

    for (register unsigned int i = 0; i < index.size; i++) {
        predicted_value += (wct->label.data[index.data[i]][label_col] * wct->weight.data[index.data[i]]);
        len_predicted += wct->weight.data[index.data[i]];
    }

    double return_value = predicted_value / len_predicted;

    metrics_t ret;
    ret.node.count = index.size;
    ret.misc.error_function = mean_squared_error;
    ret.misc.label_return = label_original;
    ret.misc.acc_function = acc_return_error;
    ret.misc.better_acc_condition = better_smaller;
    ret.misc.gradient_operation = gradient_op_regression;
    dvec_init(&ret.value);
    dvec_append(&ret.value, return_value);

    activation_function(&ret.value);

    ret.misc.class = -2;

    return ret;
}

metrics_t wct_regression_with_kernel (wct_t *wct, ivec_t index, dvec_t distance_arr, double *input, int input_len, int label_col, void (*activation_function)()) {
    /*
    Weighted Cognition Table Models to Solve Regression Problems, Weight Multiplied by Distance as Kernel.
    metrics_t description:
        .node.count => similar features in wct Nodes count;
        .misc.error_function => Error function that should be Used for this Estimation
        .value => predicted value;
        .misc.class => Always (-2); 
                  Not a Classification Problem!
    */

    double predicted_value = 0.0;
    double len_predicted = 0.0;

    for (register unsigned int i = 0; i < index.size; i++) {
        predicted_value += (wct->label.data[index.data[i]][label_col] * wct->weight.data[index.data[i]] * distance_arr.data[i]);
        len_predicted += wct->weight.data[index.data[i]] * distance_arr.data[i];
    }

    double return_value = predicted_value / len_predicted;

    metrics_t ret;
    ret.node.count = index.size;
    ret.misc.error_function = mean_squared_error;
    ret.misc.label_return = label_original;
    ret.misc.acc_function = acc_return_error;
    ret.misc.better_acc_condition = better_smaller;
    ret.misc.gradient_operation = gradient_op_regression;
    dvec_init(&ret.value);
    dvec_append(&ret.value, return_value);

    activation_function(&ret.value);

    ret.misc.class = -2;

    return ret;
}

metrics_t wct_regression_unweighted (wct_t *wct, ivec_t index, dvec_t distance_arr, double *input, int input_len, int label_col, void (*activation_function)()) {
    /*
    Unweighted Cognition Table Models to Solve Regression Problems.
    metrics_t description:
        .node.count => similar features in wct Nodes count;
        .misc.error_function => Error function that should be Used for this Estimation
        .value => predicted value;
        .misc.class => Always (-2); 
                  Not a Classification Problem!
    */

    double predicted_value = 0.0;
    double len_predicted = 0.0;

    for (register unsigned int i = 0; i < index.size; i++) {
        predicted_value += wct->label.data[index.data[i]][label_col];
        len_predicted++;
    }

    double return_value = predicted_value / len_predicted;

    metrics_t ret;
    ret.node.count = index.size;
    ret.misc.error_function = mean_squared_error;
    ret.misc.label_return = label_original;
    ret.misc.acc_function = acc_return_error;
    ret.misc.better_acc_condition = better_smaller;
    ret.misc.gradient_operation = gradient_op_regression;
    dvec_init(&ret.value);
    dvec_append(&ret.value, return_value);

    activation_function(&ret.value);

    ret.misc.class = -2;

    return ret;
}

metrics_t wct_risefall (wct_t *wct, ivec_t index, dvec_t distance_arr, double *input, int input_len, int label_col, void (*activation_function)()) {
    /*
    Weighted Cognition Table Models to Solve Classification Timeseries Problems, Where Possible Outome is either: Rise; Fall; Still.
    metrics_t description:
        .node.count => similar features in wct Nodes count;
        .misc.error_function => Error function that should be Used for this Estimation
        .value => predicted value;
        .misc.class => Consist of four possible outcome:
                    - (-1) => Not Found Similar Features;
                    - 0    => Rise Predicted;
                    - 1    => Fall Predicted;
                    - 2    => Still Predicted 
    */

    int class_count = 3;

    metrics_t ret;
    dvec_init(&ret.value);

    while (ret.value.size < class_count) {
        dvec_append(&ret.value, 0);
    }

    double len_predicted = 0.0;

    for (register unsigned int i = 0; i < index.size; i++) {
        // if (wct->label.data[index.data[i]][label_col] > input[input_len - 1]) ret.value.data[0] += wct->weight.data[index.data[i]]; else if (wct->label.data[index.data[i]][label_col] < input[input_len - 1]) ret.value.data[1] += wct->weight.data[index.data[i]]; else ret.value.data[2] += wct->weight.data[index.data[i]];
        if (wct->label.data[index.data[i]][label_col] > wct->features.data[index.data[i]][wct->meta.features_len - 1]) ret.value.data[0] += wct->weight.data[index.data[i]]; else if (wct->label.data[index.data[i]][label_col] < wct->features.data[index.data[i]][wct->meta.features_len - 1]) ret.value.data[1] += wct->weight.data[index.data[i]]; else ret.value.data[2] += wct->weight.data[index.data[i]];
        len_predicted += wct->weight.data[index.data[i]];
    }

    // standard simplified
    for (int i = 0; i < ret.value.size; i++) 
        ret.value.data[i] = ret.value.data[i] / len_predicted;

    // dvec_scaler(&ret.value);
    activation_function(&ret.value);

    ret.node.count = index.size;
    ret.misc.error_function = cross_entropy;
    ret.misc.acc_function = acc_binary;
    ret.misc.better_acc_condition = better_larger;
    ret.misc.label_return = label_risefall;
    ret.misc.class = wct_class_get(ret.value);
    ret.misc.gradient_operation = gradient_op_risefall;

    return ret;
}

metrics_t wct_risefall_with_kernel (wct_t *wct, ivec_t index, dvec_t distance_arr, double *input, int input_len, int label_col, void (*activation_function)()) {
    /*
    Weighted Cognition Table Models to Solve Classification Timeseries Problems, Where Possible Outome is either: Rise; Fall; Still. Weight Multiplied by Distance as Kernel.
    metrics_t description:
        .node.count => similar features in wct Nodes count;
        .misc.error_function => Error function that should be Used for this Estimation
        .value => predicted value;
        .misc.class => Consist of four possible outcome:
                    - (-1) => Not Found Similar Features;
                    - 0    => Rise Predicted;
                    - 1    => Fall Predicted;
                    - 2    => Still Predicted 
    */

    int class_count = 3;

    metrics_t ret;
    dvec_init(&ret.value);

    while (ret.value.size < class_count) {
        dvec_append(&ret.value, 0);
    }

    double len_predicted = 0.0;

    for (register unsigned int i = 0; i < index.size; i++) {
        // if (wct->label.data[index.data[i]][label_col] > input[input_len - 1]) ret.value.data[0] += wct->weight.data[index.data[i]] * distance_arr.data[i]; else if (wct->label.data[index.data[i]][label_col] < input[input_len - 1]) ret.value.data[1] += wct->weight.data[index.data[i]] * distance_arr.data[i]; else ret.value.data[2] += wct->weight.data[index.data[i]] * distance_arr.data[i];
        if (wct->label.data[index.data[i]][label_col] > wct->features.data[index.data[i]][wct->meta.features_len - 1]) ret.value.data[0] += wct->weight.data[index.data[i]] * distance_arr.data[i]; else if (wct->label.data[index.data[i]][label_col] < wct->features.data[index.data[i]][wct->meta.features_len - 1]) ret.value.data[1] += wct->weight.data[index.data[i]] * distance_arr.data[i]; else ret.value.data[2] += wct->weight.data[index.data[i]] * distance_arr.data[i];
        len_predicted += wct->weight.data[index.data[i]] * distance_arr.data[i];
    }

    // standard simplified
    for (int i = 0; i < ret.value.size; i++) 
        ret.value.data[i] = ret.value.data[i] / len_predicted;

    // dvec_scaler(&ret.value);
    activation_function(&ret.value);

    ret.node.count = index.size;
    ret.misc.error_function = cross_entropy;
    ret.misc.acc_function = acc_binary;
    ret.misc.better_acc_condition = better_larger;
    ret.misc.label_return = label_risefall;
    ret.misc.class = wct_class_get(ret.value);
    ret.misc.gradient_operation = gradient_op_risefall;

    return ret;
}

metrics_t wct_risefall_unweighted (wct_t *wct, ivec_t index, dvec_t distance_arr, double *input, int input_len, int label_col, void (*activation_function)()) {
    /*
    Unweighted Cognition Table Models to Solve Classification Timeseries Problems, Where Possible Outome is either: Rise; Fall; Still.
    metrics_t description:
        .node.count => similar features in wct Nodes count;
        .misc.error_function => Error function that should be Used for this Estimation
        .value => predicted value;
        .misc.class => Consist of four possible outcome:
                    - (-1) => Not Found Similar Features;
                    - 0    => Rise Predicted;
                    - 1    => Fall Predicted;
                    - 2    => Still Predicted 
    */

    int class_count = 3;

    metrics_t ret;
    dvec_init(&ret.value);

    while (ret.value.size < class_count) {
        dvec_append(&ret.value, 0);
    }

    double len_predicted = 0.0;

    for (register unsigned int i = 0; i < index.size; i++) {
        // if (wct->label.data[index.data[i]][label_col] > input[input_len - 1]) ret.value.data[0]++; else if (wct->label.data[index.data[i]][label_col] < input[input_len - 1]) ret.value.data[1]++; else ret.value.data[2]++;
        if (wct->label.data[index.data[i]][label_col] > wct->features.data[index.data[i]][wct->meta.features_len - 1]) ret.value.data[0]++; else if (wct->label.data[index.data[i]][label_col] < wct->features.data[index.data[i]][wct->meta.features_len - 1]) ret.value.data[1]++; else ret.value.data[2]++;
        len_predicted++;
    }

    // standard simplified
    for (int i = 0; i < ret.value.size; i++) 
        ret.value.data[i] = ret.value.data[i] / len_predicted;

    // dvec_scaler(&ret.value);
    activation_function(&ret.value);

    ret.node.count = index.size;
    ret.misc.error_function = cross_entropy;
    ret.misc.acc_function = acc_binary;
    ret.misc.better_acc_condition = better_larger;
    ret.misc.label_return = label_risefall;
    ret.misc.class = wct_class_get(ret.value);
    ret.misc.gradient_operation = gradient_op_risefall;

    return ret;
}

// Distance Function
double wct_manhattan_distance (double *p, double *q, int len) {
    return (1 - math_manhattan_distance(p, q, len));
}

double wct_euclidean_distance (double *p, double *q, int len) {
    double distance = math_euclidean_distance(p, q, len);
    distance = distance ? distance : 0.01;
    return (1.0 / distance);
}

// Estimate Activation Condition:
int by_minimal_similarity (double distance, double sim) {
    return distance > sim;
}

int by_maximal_similarity (double distance, double sim) {
    return distance < sim;
}

int by_zero (double distance, double sim) {
    return distance > 0.0;
}

int by_allnode (double distance, double sim) {
    return 1;
}

typedef struct {
    wct_t *wct;
    cfg_t cfg;
    double *input;
    int input_len;
    double (*distance_function)();
    void (*activation_function)();
    int (*estimate_condition)();
} dynlabel_get_args;

int wct_dynlabel_get_base (wct_t *wct, cfg_t cfg, double *input, int input_len, double (*distance_function)(), void (*activation_function)(), int (*estimate_condition)()) {
    if (input_len != wct->meta.features_len) {
        log_error("Input Lenght is Not Equal to WCT Features Lenght!\n");\
        wct_exit(WCT_INVALID_ARGUMENT);
    }
    if (cfg.ready != 1) {
        log_error("Config is Not Initialized, Use config_init()!\n");\
        wct_exit(WCT_INVALID_CONFIG);
    }

    register unsigned int foundcount = 0;
    double sim = cfg.default_similarity;
    register unsigned int min_found = cfg.default_min_pattern_found;

    ivec_t index;
    dvec_t distance;
    ivec_init(&index);
    dvec_init(&distance);
    register unsigned int x;
    double dist = 0.0;

    for (x = 0; x < wct->weight.size; x++) {
        dist = distance_function(wct->features.data[x], input, input_len);
        if (estimate_condition(dist, sim)) {
            foundcount++;
            ivec_append(&index, x);
            dvec_append(&distance, dist);
        }
    }

    if (foundcount >= min_found) {
        int class_count = wct->dynlabel.index.size;
        int ix = 0;

        dvec_t ret;
        dvec_init(&ret);

        while (ret.size < class_count) {
            dvec_append(&ret, 0);
        }
        double len_predicted = 0.0;

        for (register unsigned int i = 0; i < index.size; i++) {
            ix = wct->dynlabel.value.data[index.data[i]];
            ret.data[ix]++; 
            len_predicted++;
        }

        for (int i = 0; i < ret.size; i++) 
            ret.data[i] = ret.data[i] / len_predicted;

        activation_function(&ret);

        int num_ret = 0;
        if (wct_class_get(ret) != -1) num_ret = wct->dynlabel.index.data[wct_class_get(ret)];

        dvec_free(&ret);
        ivec_free(&index);
        dvec_free(&distance);

        return num_ret;
    } else {
        ivec_free(&index);
        dvec_free(&distance);
        return -1;
    }
}

int wct_dynlabel_get_wrapper (dynlabel_get_args a) {
    if (a.cfg.ready != 1) {
        config_init(&a.cfg);
    }
    int a_input_len = a.input_len ? a.input_len : 0;
    double (*a_distance_function)() = a.distance_function ? a.distance_function : wct_manhattan_distance;
    void (*a_activation_function)() = a.activation_function ? a.activation_function : no_activation;
    int (*a_estimate_condition)() = a.estimate_condition ? a.estimate_condition : by_minimal_similarity;

    if (a.wct->meta.ready != 1) {
        log_error("WCT Is Not Initialized, Use wct_load() or wct_import()!\n");
        wct_exit(WCT_NOT_READY);
    }
    if (a_input_len == 0) {
        log_error("Please Provide Valid .input And .input_len Argument!\n");
        wct_exit(WCT_INVALID_ARGUMENT);
    }

    int ret = wct_dynlabel_get_base(a.wct, a.cfg, a.input, a_input_len, a_distance_function, a_activation_function, a_estimate_condition);
    return ret;
}

typedef struct {
    wct_t *wct;
    cfg_t cfg;
    double *input;
    int input_len;
    int label_col;
    double (*distance_function)();
    metrics_t (*estimate_function)();
    void (*activation_function)();
    int (*estimate_condition)();
    int not_verbose;
} estimate_args;

metrics_t wct_estimate_base (wct_t *wct, cfg_t cfg, double *input, int input_len, int label_col, double (*distance_function)(), metrics_t (*estimate_function)(), void (*activation_function)(), int (*estimate_condition)(), int not_verbose) {
    if (input_len != wct->meta.features_len) {
        log_error("Input Lenght is Not Equal to WCT Features Lenght!\n");\
        wct_exit(WCT_INVALID_ARGUMENT);
    }
    if (cfg.ready != 1) {
        log_error("Config is Not Initialized, Use config_init()!\n");\
        wct_exit(WCT_INVALID_CONFIG);
    }
    DECLARE_TIMER(collect);
    START_TIMER(collect);

    register unsigned int foundcount = 0;
    double sim = cfg.default_similarity;
    register unsigned int min_found = cfg.default_min_pattern_found;

    ivec_t index;
    dvec_t distance;
    ivec_init(&index);
    dvec_init(&distance);
    register unsigned int x;
    double dist = 0.0;

    for (x = 0; x < wct->weight.size; x++) {
        dist = distance_function(wct->features.data[x], input, input_len);
        if (estimate_condition(dist, sim)) {
            foundcount++;
            ivec_append(&index, x);
            dvec_append(&distance, dist);
        }
    }

    if (foundcount >= min_found) {
        if (label_col == AUTOLABEL) label_col = wct_dynlabel_get(wct, cfg, input, input_len, distance_function, activation_function, estimate_condition);
        metrics_t ret = estimate_function(wct, index, distance, input, input_len, label_col, activation_function);
        ret.misc.label_col = label_col;
        ivec_copy(&index, &ret.node.index);
        dvec_copy(&distance, &ret.node.kernel);
               
        ivec_free(&index);
        dvec_free(&distance);

        STOP_TIMER(collect);

        if (not_verbose == 0) {
        	char buff[256];
            sprintf(buff, "wct_estimate(): {Activated Nodes: %d, Predicted_Value: [", ret.node.count);
            for (int i = 0; i < ret.value.size; i++) if (i == ret.value.size - 1) sprintf(buff + strlen(buff), "%f]} - ", ret.value.data[i]); else sprintf(buff + strlen(buff), "%f, ", ret.value.data[i]);
            sprintf(buff + strlen(buff), "[took: %f]", GET_TIMER(collect));
        	log_info("%s\n", buff);
        }     

        return ret;
    } else {
        metrics_t ret = wct_not_found_estimator(wct, index, distance, input, input_len, label_col, activation_function);
        ret.misc.label_col = -1;

        ivec_free(&index);
        dvec_free(&distance);

        STOP_TIMER(collect);

        if (not_verbose == 0) {
        	char buff[32];
        	sprintf(buff, "No Activated Nodes! - ");
        	sprintf(buff + strlen(buff), "[took: %f]", GET_TIMER(collect));
        	log_info("%s\n", buff);
        }      

        return ret;
    }
}

metrics_t wct_estimate_wrapper (estimate_args a) {
    if (a.cfg.ready != 1) {
        config_init(&a.cfg);
    }
    int a_input_len = a.input_len ? a.input_len : 0;
    int a_label_col = a.label_col ? a.label_col : 0;
    double (*a_distance_function)() = a.distance_function ? a.distance_function : wct_manhattan_distance;
    metrics_t (*a_estimate_function)() = a.estimate_function ? a.estimate_function : wct_risefall;
    void (*a_activation_function)() = a.activation_function ? a.activation_function : no_activation;
    int (*a_estimate_condition)() = a.estimate_condition ? a.estimate_condition : by_minimal_similarity;
    int a_not_verbose = a.not_verbose ? a.not_verbose : 0;

    if (a.wct->meta.ready != 1) {
        log_error("WCT Is Not Initialized, Use wct_load() or wct_import()!\n");
        wct_exit(WCT_NOT_READY);
    }
    if (a_input_len == 0) {
        log_error("Please Provide Valid .input And .input_len Argument!\n");
        wct_exit(WCT_INVALID_ARGUMENT);
    }

    metrics_t ret = wct_estimate_base(a.wct, a.cfg, a.input, a_input_len, a_label_col, a_distance_function, a_estimate_function, a_activation_function, a_estimate_condition, a_not_verbose);
    return ret;
}

double cycle_minLR (double maxLR) {
    return maxLR / pow(2.6, 4);
}

double cycle_lr(int e, int total_epoch, double maxLR) {
    double minLR = cycle_minLR(maxLR);
    int cycle = (int) (1 + e / total_epoch);
    double x = fabs(e / (double)(total_epoch / 2) - 2 * cycle + 1);
    double fx = 1.0 - x > 0 ? 1.0 - x : 0;
    double lr = minLR + (maxLR - minLR) * fx;
    return lr;
}

double fix_lr (int e, int total_epoch, double maxLR) {
    return maxLR;
}

typedef struct {
    wct_t *wct;
    cfg_t cfg;
    double *X;
    int X_Len;
    int label_col;
    double (*distance_function)();
    metrics_t (*estimate_function)();
    void (*activation_function)();
    int (*estimate_condition)();
    double (*error_function)();
    double Y;
    int not_verbose;
} evaluate_args;

metrics_t wct_evaluate_base (wct_t *wct, cfg_t cfg, double *X, int X_Len, int label_col, double (*distance_function)(), metrics_t (*estimate_function)(), void (*activation_function)(), int (*estimate_condition)(), double (*error_function)(), double Y, int not_verbose) {
    metrics_t ret = wct_estimate(wct, cfg, X, X_Len, label_col, distance_function, estimate_function, activation_function, estimate_condition, not_verbose);
    Y = ret.misc.label_return(Y, X[X_Len - 1]);
    ret.true_label = Y;
    ret.error = error_function ? error_function(ret.value, Y) : ret.misc.error_function(ret.value, Y);
    ret.acc = ret.misc.acc_function(wct_predicted_value_get(ret.value), Y, X[X_Len-1], ret.error);
    if (not_verbose == 0) log_info("wct_evaluate(): {Error Measure: %f}\n\n", ret.error);
    return ret;
}

metrics_t wct_evaluate_wrapper (evaluate_args a) {
    if (a.cfg.ready != 1) {
        config_init(&a.cfg);
    }
    int a_X_len = a.X_Len ? a.X_Len : 0;
    int a_label_col = a.label_col ? a.label_col : 0;
    double (*a_distance_function)() = a.distance_function ? a.distance_function : wct_manhattan_distance;
    metrics_t (*a_estimate_function)() = a.estimate_function ? a.estimate_function : wct_risefall;
    void (*a_activation_function)() = a.activation_function ? a.activation_function : no_activation;
    double (*a_error_function)() = a.error_function;
    int (*a_estimate_condition)() = a.estimate_condition ? a.estimate_condition : by_minimal_similarity;
    int a_not_verbose = a.not_verbose ? a.not_verbose : 0;

    if (a.wct->meta.ready != 1) {
        log_error("WCT Is Not Initialized, Use wct_load() or wct_import()!\n");
        wct_exit(WCT_NOT_READY);
    }
    if (a_X_len == 0) {
        log_error("Please Provide Valid .X And .X_len Argument!\n");
        wct_exit(WCT_INVALID_ARGUMENT);
    }

    metrics_t ret = wct_evaluate_base(a.wct, a.cfg, a.X, a_X_len, a_label_col, a_distance_function, a_estimate_function, a_activation_function, a_estimate_condition, a_error_function, a.Y, a_not_verbose);
    return ret;
}

typedef struct {
    wct_t *wct;
    ivec_t label_list;
    cfg_t cfg;
    double (*distance_function)();
    metrics_t (*estimate_function)();
    void (*activation_function)();
    int (*estimate_condition)();
    double (*error_function)();
} dynlabel_fit_args;

void wct_dynlabel_fit_base (wct_t *wct, ivec_t label_list, cfg_t cfg, double (*distance_function)(), metrics_t (*estimate_function)(), void (*activation_function)(), int (*estimate_condition)(), double (*error_function)()) {
    dvec_t error_list;
    char info[64];
    int userstop = 0;
    char key;

    progressbar_t bar;
    progressbar_create(&bar, 0, wct->weight.size);

    if (wct->meta.islabelfitted == 1) {
        log_warn("Dynamic Label is Already Fitted, Will Reset The Fit Process\n");
        wct_dynlabel_reset(wct);
    }

    log_info("Beginning Fitting Dynamic Label, Press SHIFT+C (Upper Case C) To Abort!\n");

    for (int i = 0; i < label_list.size; i++) ivec_append(&wct->dynlabel.index, label_list.data[i]);

    for (int i = 0; i < wct->weight.size; i++) {
        progressbar_update(&bar);
        dvec_init(&error_list);

        for (int a = 0; a < label_list.size; a++) {
            double Y = wct->label.data[i][label_list.data[a]];
            metrics_t metric = wct_evaluate(wct, cfg, wct->features.data[i], wct->features.col[i], label_list.data[a], distance_function, estimate_function, activation_function, estimate_condition, error_function, Y, 1);

            dvec_append(&error_list, metric.error);
            metrics_free(&metric);

            if (kbhit()) {
                key = getch();
                if (key == 'C') {
                    userstop = 1;
                    i = wct->weight.size;
                    break;
                }
            }
        }

        if (i == wct->weight.size) break;

        int idx = dvec_search(&error_list, min_d(error_list.data, error_list.size));
        int best_label = label_list.data[idx];
        if (error_list.data[label_list.data[idx]] == -1) best_label = -1;
        sprintf(info, "best_label: %d\terror: %f", best_label, error_list.data[label_list.data[idx]]);

        wct->dynlabel.value.data[i] = ivec_search(&wct->dynlabel.index, best_label);
        dvec_free(&error_list);
        progressbar_print(&bar, info);
    }

    if (userstop == 1) printf("\n");
    char buff[256];

    sprintf(buff, "Dynamic Label Fit Complete, Insight: {(NULL): %d, ", ivec_count(&wct->dynlabel.value, -1));
    for (int i = 0; i < wct->dynlabel.index.size; i++) {
        sprintf(buff + strlen(buff), "(%d): %d", wct->dynlabel.index.data[i], ivec_count(&wct->dynlabel.value, i));
        if (i != wct->dynlabel.index.size - 1) sprintf(buff + strlen(buff), ", "); else sprintf(buff + strlen(buff), "}\n");
    }
    log_info("%s", buff);
    if (userstop == 0) {
        wct_dynlabel_save(wct);
        log_info("Dynamic Label Updated!\n");
        if (wct->meta.islabelfitted != 1) {
            wct->meta.major++;
            wct->meta.minor = 0;
            wct->meta.patch = 0;
        } else {
            wct->meta.patch++;
        }
        wct->meta.islabelfitted = 1;
        wct_meta_save(wct);
    }
}

void wct_dynlabel_fit_wrapper (dynlabel_fit_args a) {
    if (a.cfg.ready != 1) {
        config_init(&a.cfg);
    }
    double (*a_distance_function)() = a.distance_function ? a.distance_function : wct_manhattan_distance;
    metrics_t (*a_estimate_function)() = a.estimate_function ? a.estimate_function : wct_risefall;
    void (*a_activation_function)() = a.activation_function ? a.activation_function : no_activation;
    double (*a_error_function)() = a.error_function;
    int (*a_estimate_condition)() = a.estimate_condition ? a.estimate_condition : by_minimal_similarity;

    if (a.wct->meta.ready != 1) {
        log_error("WCT Is Not Initialized, Use wct_load() or wct_import()!\n");
        wct_exit(WCT_NOT_READY);
    }

    if (a.label_list.ready != 1) {
        log_error("Label List For Dynlabel Fit is Not Valid, Check Your Arguments!\n");
        wct_exit(WCT_INVALID_ARGUMENT);
    }

    wct_dynlabel_fit_base(a.wct, a.label_list, a.cfg, a_distance_function, a_estimate_function, a_activation_function, a_estimate_condition, a_error_function);
}

typedef struct {
    wct_t *wct;
    double weight;
    double *features;
    int features_len;
    double *label;
    int label_len;
    double featureindex;
    double (*distance_function)();
    metrics_t (*estimate_function)();
    void (*activation_function)();
    int (*estimate_condition)();
    double (*error_function)();
} append_args;

int wct_append_base(wct_t *wct, double weight, double *features, int features_len, double *label, int label_len, double featureindex, double (*distance_function)(), metrics_t (*estimate_function)(), void (*activation_function)(), int (*estimate_condition)(), double (*error_function)()) {
    cfg_t cfg;
    config_init(&cfg);

    dvec_t error_list;
    dvec_init(&error_list);

    int dynlabel = 0;

    if (wct->dynlabel.index.size > 0) {
        for (int a = 0; a < wct->dynlabel.index.size; a++) {
            double Y = label[wct->dynlabel.index.data[a]];
            metrics_t metric = wct_evaluate(wct, cfg, features, features_len, wct->dynlabel.index.data[a], distance_function, estimate_function, activation_function, estimate_condition, error_function, Y, 1);

            dvec_append(&error_list, metric.error);
            metrics_free(&metric);
        }

        int idx = dvec_search(&error_list, min_d(error_list.data, error_list.size));
        int best_label = wct->dynlabel.index.data[idx];
        if (error_list.data[wct->dynlabel.index.data[idx]] == -1) best_label = -1;
        dynlabel = ivec_search(&wct->dynlabel.index, best_label);
    }

    if (dynlabel == -1) {
        dvec_free(&error_list);
        return -1;
    } else {
        ivec_append(&wct->dynlabel.value, dynlabel);
        dvec_append(&wct->weight, weight);
        dtab_append_row(&wct->features, features, features_len);
        dtab_append_row(&wct->label, label, label_len);
        dvec_append(&wct->featureindex, featureindex);
        dvec_free(&error_list);
        return 1;
    }
}

int wct_append_wrapper(append_args a) {
    double a_weight = a.weight ? a.weight : wct_pointone_init();
    int a_features_len = a.features_len ? a.features_len : 0;
    int a_label_len = a.label_len ? a.label_len : 0;
    double a_featureindex = a.featureindex ? a.featureindex : 0.0;
    double (*a_distance_function)() = a.distance_function ? a.distance_function : wct_manhattan_distance;
    metrics_t (*a_estimate_function)() = a.estimate_function ? a.estimate_function : wct_risefall;
    void (*a_activation_function)() = a.activation_function ? a.activation_function : no_activation;
    double (*a_error_function)() = a.error_function;
    int (*a_estimate_condition)() = a.estimate_condition ? a.estimate_condition : by_minimal_similarity;

    if (a.wct->meta.ready != 1) {
        log_error("WCT Is Not Initialized, Use wct_load() or wct_import()!\n");
        wct_exit(WCT_NOT_READY);
    }
    if (a_features_len == 0) {
        log_error("Please Provide Valid .features And .features_len Argument!\n");
        wct_exit(WCT_INVALID_ARGUMENT);
    }
    if (a_label_len == 0) {
        log_error("Please Provide Valid .label And .label_len Argument!\n");
        wct_exit(WCT_INVALID_ARGUMENT);
    }

    int ret = wct_append_base(a.wct, a_weight, a.features, a_features_len, a.label, a_label_len, a_featureindex, a_distance_function, a_estimate_function, a_activation_function, a_estimate_condition, a_error_function);
    return ret;
}

typedef struct {
    wct_t *wct;
    double *input;
    int input_len;
    double (*distance_function)();
    int (*estimate_condition)();
    double similarity;
    int min_found;
} discriminator_args;

int wct_discriminator_base (wct_t *wct, double *input, int input_len, double (*distance_function)(), int (*estimate_condition)(), double similarity, int min_found) {
    if (wct->meta.ready != 1) {
        log_error("WCT Is Not Initialized, Use wct_load! or wct_import()!\n");
        wct_exit(WCT_NOT_READY);
    }
    DECLARE_TIMER(discriminator);
    START_TIMER(discriminator);

    register unsigned int foundcount = 0;
    int isCorrelated = 0;
    register unsigned int x = 0;
    double dist = 0.0;

    for (x = 0; x < wct->weight.size; x++) {
        dist = distance_function(wct->features.data[x], input, input_len);
        if (estimate_condition(dist, similarity)) {
            foundcount++;
        }
        if (foundcount >= min_found) {
            isCorrelated = 1;
            break;
        }
    }

    STOP_TIMER(discriminator);
    log_info("wct_discriminator(): {Value: %d} - [took: %f]\n", isCorrelated, GET_TIMER(discriminator));

    return isCorrelated;
}

int wct_discriminator_wrapper (discriminator_args a) {
    int a_input_len = a.input_len ? a.input_len : 0;
    double (*a_distance_function)() = a.distance_function ? a.distance_function : wct_manhattan_distance;
    int (*a_estimate_condition)() = a.estimate_condition ? a.estimate_condition : by_minimal_similarity;
    double a_similarity = a.similarity ? a.similarity : 0.80;
    int a_min_found = a.min_found ? a.min_found : 2;

    if (a.wct->meta.ready != 1) {
        log_error("WCT Is Not Initialized, Use wct_load() or wct_import()!\n");
        wct_exit(WCT_NOT_READY);
    }
    if (a_input_len == 0) {
        log_error("Please Provide Valid .input And .input_len Argument!\n");
        wct_exit(WCT_INVALID_ARGUMENT);
    }

    int ret = wct_discriminator_base(a.wct, a.input, a_input_len, a_distance_function, a_estimate_condition, a_similarity, a_min_found);
    return ret;
}

double test_op_sequence (wct_t *wct, cfg_t cfg, dataframe_t *df, int labelcol, int step, double (*distance_function)(), metrics_t (*estimate_function)(), void (*activation_function)(), int (*estimate_condition)(), double (*error_function)(), int batch) {
    dvec_t data;
    if (dataframe_typename(*df) == DF_SEQUENCE) {
        log_warn("Dataframe Sequence is Not Splitted, Will Be Using All Data Instead!\n");
        dvec_copy(&df->seq, &data);
    } else if (dataframe_typename(*df) == DF_SEQUENCE_SPLITTED) {
        dvec_copy(&df->seq_test, &data);
    }

    if (labelcol > wct->label.col[0]) {
        log_error("Label Index is Out Of Bounds, Max Label is: %d\n", wct->label.col[0]);
        wct_exit(WCT_OUT_OF_BOUNDS);
    }

    int limit = data.size - wct->meta.features_len - 3;
    int test_len = step ? step : limit;
    if (test_len > limit) test_len = limit;

    int batch_size = batch ? batch : test_len;
    if (batch_size > test_len) batch_size = test_len;

    metrics_t metric;
    double Y = 0.0;
    double total_error = 0.0;
    double total_acc = 0.0;
    double counter = 0.0;

    double batch_error = 0.0;
    double batch_acc = 0.0;
    double batch_counter = 0.0;
    int label_col = labelcol;

    dvec_t test_data;
    dvec_init(&test_data);

    char info[32];
    int e = 0;
    int b = 0;

    while (e < test_len) {
        progressbar_t bar;

        if (batch > 0) {
            progressbar_create(&bar, 0, batch);
            batch_error = 0.0;
            batch_acc = 0.0;
            batch_counter = 0.0;

            printf("Batch: %d / %d\n", b, (int)(test_len / batch));
        } else {
            progressbar_create(&bar, 0, test_len);
        }

        for (int i = 0; i < batch_size; i++) {
            progressbar_update(&bar);

            wcthelper_get_test_sequence_data(wct, cfg, &test_data, &data, e);
            if (labelcol == AUTOLABEL) label_col = wct_dynlabel_get(wct, cfg, test_data.data, test_data.size, distance_function, activation_function, estimate_condition);
            Y = wcthelper_get_test_sequence_label(wct, cfg, &data, e, label_col);
            
            metric = wct_evaluate(wct, cfg, test_data.data, test_data.size, label_col, distance_function, estimate_function, activation_function, estimate_condition, error_function, Y, 1);
            if (metric.error != -1) {
                total_error += metric.error;
                batch_error += metric.error;
            }
            if (metric.acc != -1) {
                total_acc += metric.acc;
                batch_acc += metric.acc;
                counter++;
                batch_counter++;
            }

            sprintf(info, "loss: %f\tacc: %f", batch_error, batch_acc / batch_counter);
            progressbar_print(&bar, info);
            metrics_free(&metric);

            e++;
            if (e >= test_len) {
                if (bar.current != bar.end) progressbar_done(&bar, info);
                break;
            }
        }

        b++;
    }

    wct->meta.last_wr = (total_acc / counter);
    meta_save(&wct->meta);

    log_info("Sequence Test Complete, loss: %f\tacc: %f\n", total_error, total_acc / counter);
    metrics_free(&metric);
    dvec_free(&data);
    dvec_free(&test_data);

    return (total_acc / counter);
}

double test_op_table (wct_t *wct, cfg_t cfg, dataframe_t *df, int labelcol, int step, double (*distance_function)(), metrics_t (*estimate_function)(), void (*activation_function)(), int (*estimate_condition)(), double (*error_function)(), int batch) {
    dtab_t data;
    dtab_t label;
    if (dataframe_typename(*df) == DF_TABLE) {
        log_warn("Dataframe Table is Not Splitted, Will Be Using All Data Instead!\n");
        dtab_copy(&df->x, &data);
        dtab_copy(&df->y, &label);
    } else if (dataframe_typename(*df) == DF_TABLE_SPLITTED) {
        dtab_copy(&df->x_test, &data);
        dtab_copy(&df->y_test, &label);
    }

    if (labelcol > wct->label.col[0]) {
        log_error("Label Index is Out Of Bounds, Max Label is: %d\n", wct->label.col[0]);
        wct_exit(WCT_OUT_OF_BOUNDS);
    }

    int limit = data.row;
    int test_len = step ? step : limit;
    if (test_len > limit) test_len = limit;

    int batch_size = batch ? batch : test_len;
    if (batch_size > test_len) batch_size = test_len;

    metrics_t metric;
    double Y = 0.0;
    double total_error = 0.0;
    double total_acc = 0.0;
    double counter = 0.0;

    double batch_error = 0.0;
    double batch_acc = 0.0;
    double batch_counter = 0.0;
    int label_col = labelcol;

    char info[32];
    int e = 0;
    int b = 0;

    while (e < test_len) {
        progressbar_t bar;

        if (batch > 0) {
            progressbar_create(&bar, 0, batch);
            batch_error = 0.0;
            batch_acc = 0.0;
            batch_counter = 0.0;

            printf("Batch: %d / %d\n", b, (int)(test_len / batch));
        } else {
            progressbar_create(&bar, 0, test_len);
        }

        for (int i = 0; i < batch_size; i++) {
            progressbar_update(&bar);

            if (labelcol == AUTOLABEL) label_col = wct_dynlabel_get(wct, cfg, data.data[e], data.col[e], distance_function, activation_function, estimate_condition);
            Y = label.data[e][label_col];

            metric = wct_evaluate(wct, cfg, data.data[e], data.col[e], label_col, distance_function, estimate_function, activation_function, estimate_condition, error_function, Y, 1);
            if (metric.error != -1) {
                total_error += metric.error;
                batch_error += metric.error;
            }
            if (metric.acc != -1) {
                total_acc += metric.acc;
                batch_acc += metric.acc;
                counter++;
                batch_counter++;
            }

            sprintf(info, "loss: %f\tacc: %f", batch_error, batch_acc / batch_counter);
            progressbar_print(&bar, info);
            metrics_free(&metric);

            e++;
            if (e >= test_len) {
                if (bar.current != bar.end) progressbar_done(&bar, info);
                break;
            }
        }
        
        b++;
    }

    wct->meta.last_wr = (total_acc / counter);
    meta_save(&wct->meta);

    log_info("Table Test Complete, loss: %f\tacc: %f\n", total_error, total_acc / counter);
    metrics_free(&metric);
    dtab_free(&data);
    dtab_free(&label);

    return (total_acc / counter);
}

typedef struct {
    wct_t *wct;
    cfg_t cfg;
    dataframe_t *df;
    int label_col;
    int step;
    double (*distance_function)();
    metrics_t (*estimate_function)();
    void (*activation_function)();
    int (*estimate_condition)();
    double (*error_function)();
    int batch;
} test_args;

double wct_test_base (wct_t *wct, cfg_t cfg, dataframe_t *df, int label_col, int step, double (*distance_function)(), metrics_t (*estimate_function)(), void (*activation_function)(), int (*estimate_condition)(), double (*error_function)(), int batch) {
    DECLARE_TIMER(test);
    START_TIMER(test);

    double ret;
    if ((dataframe_typename(*df) == DF_TABLE) || (dataframe_typename(*df) == DF_TABLE_SPLITTED)) {
        ret = test_op_table(wct, cfg, df, label_col, step, distance_function, estimate_function, activation_function, estimate_condition, error_function, batch);
    } else if ((dataframe_typename(*df) == DF_SEQUENCE) || (dataframe_typename(*df) == DF_SEQUENCE_SPLITTED)) {
        ret = test_op_sequence(wct, cfg, df, label_col, step, distance_function, estimate_function, activation_function, estimate_condition, error_function, batch);
    } else {
        log_error("Undefined Dataframe Behaviour, wct_test() Cancelled!\n");
        wct_exit(WCT_ERROR);
    }

    STOP_TIMER(test);
    log_info("wct_test() - [took: %f]\n", GET_TIMER(test));

    return ret;
}

double wct_test_wrapper (test_args a) {
    if (a.wct->meta.ready != 1) {
        log_error("WCT Is Not Initialized, Use wct_load() or wct_import()!\n");
        wct_exit(WCT_NOT_READY);
    }
    if (a.cfg.ready != 1) {
        config_init(&a.cfg);
    }
    int a_label_col = a.label_col ? a.label_col : 0;
    int a_step = a.step ? a.step : 0;
    double (*a_distance_function)() = a.distance_function ? a.distance_function : wct_manhattan_distance;
    metrics_t (*a_estimate_function)() = a.estimate_function ? a.estimate_function : wct_risefall;
    void (*a_activation_function)() = a.activation_function ? a.activation_function : no_activation;
    int (*a_estimate_condition)() = a.estimate_condition ? a.estimate_condition : by_minimal_similarity;
    double (*a_error_function)() = a.error_function;
    int a_batch = a.batch ? a.batch : 0;

    double ret;
    ret = wct_test_base(a.wct, a.cfg, a.df, a_label_col, a_step, a_distance_function, a_estimate_function, a_activation_function, a_estimate_condition, a_error_function, a_batch);
    return ret;
}

double fit_op_sequence (wct_t *wct, cfg_t cfg, dataframe_t *df, int labelcol, int batch, int epoch, int iteration, double (*lr_func)(), double (*gradient_func)(), double (*distance_function)(), metrics_t (*estimate_function)(), void (*activation_function)(), int (*estimate_condition)(), double (*error_function)()) {
    dvec_t data;
    dvec_copy(&df->seq, &data);

    if (labelcol > wct->label.col[0]) {
        log_error("Label Index is Out Of Bounds, Max Label is: %d\n", wct->label.col[0]);
        wct_exit(WCT_OUT_OF_BOUNDS);
    }

    log_info("Beginning Fit Sequence Operation, Press SHIFT+C (Upper Case C) To Abort!\n");

    int limit = data.size - wct->meta.features_len - 3;
    int fit_iteration = iteration ? iteration : limit;
    if (fit_iteration > limit) fit_iteration = limit;

    int batch_size = batch ? batch : fit_iteration;
    if (batch_size > fit_iteration) batch_size = fit_iteration;

    metrics_t metric;
    double Y = 0.0;
    double total_error = 0.0;
    double total_acc = 0.0;
    double counter = 0.0;

    double iterate_error = 0.0;
    double iterate_acc = 0.0;
    double iterate_counter = 0.0;
    int userstop = 0;
    int label_col = labelcol;

    int bar_divider = fit_iteration / 300;
    bar_divider = bar_divider ? bar_divider : 1;

    double lr = 0.0;
    int (*better_acc_condition)();
    better_acc_condition = better_not_found;

    dvec_t fit_data;
    dvec_init(&fit_data);

    char info[32];
    char epoch_c[16];
    char key;

    if (!epoch) sprintf(epoch_c, "%d", epoch); else strcpy(epoch_c, "INFINITE");

    int e = 1;
    int x = epoch ? -1 : 0;
    int i = 0;
    int index = 0;

    if (epoch) epoch = fit_iteration;

    while (e) {
        if (x >= epoch) break;

        progressbar_t bar;
        progressbar_create(&bar, 0, fit_iteration);

        lr = lr_func(index, epoch, cfg.default_learning_rate);
        printf("Epoch: %d / %s (lr = %f)\n", index, epoch_c, lr);

        iterate_counter = 0.0;
        iterate_acc = 0.0;
        iterate_error = 0.0;

        while (i < fit_iteration) {
            for (int b = 0; b < batch_size; b++) {
                progressbar_update(&bar);

                wcthelper_get_test_sequence_data(wct, cfg, &fit_data, &data, i);
                if (labelcol == AUTOLABEL) label_col = wct_dynlabel_get(wct, cfg, fit_data.data, fit_data.size, distance_function, activation_function, estimate_condition);
                Y = wcthelper_get_test_sequence_label(wct, cfg, &data, i, label_col);
                metric = wct_evaluate(wct, cfg, fit_data.data, fit_data.size, label_col, distance_function, estimate_function, activation_function, estimate_condition, error_function, Y, 1);
                if (metric.misc.better_acc_condition != better_not_found) better_acc_condition = metric.misc.better_acc_condition;

                if ((better_acc_condition(1.0, 0.0) == wct->meta.last_wr) && (wct->meta.num_trained > 0)) {
                    log_info("PERFECT FIT!");
                    userstop = 1;
                    e = 0;
                    x = epoch;
                    i = fit_iteration;
                    break;
                }

                metric.misc.gradient_operation(wct, metric, lr, gradient_func);
                if (metric.error != -1) iterate_error += metric.error;
                if (metric.acc != -1) {
                    iterate_acc += metric.acc;
                    iterate_counter++;
                }

                sprintf(info, "loss: %f\tacc: %f", iterate_error, iterate_acc / iterate_counter);
                /*if ((fit_iteration % bar_divider) == 0)*/ progressbar_print(&bar, info);

                metrics_free(&metric);

                if (kbhit()) {
                    key = getch();
                    if (key == 'C') {
                        userstop = 1;
                        e = 0;
                        x = epoch;
                        i = fit_iteration;
                        break;
                    }
                }

                i++;
                if (i >= fit_iteration) break;
            }

            if (userstop == 0) {
                if (better_acc_condition((iterate_acc / iterate_counter), wct->meta.last_wr)) {
                    wct->meta.last_train = time(NULL);
                    // wct->meta.last_wr = iterate_acc / iterate_counter;
                    wct_weight_save(wct);
                    wct_meta_save(wct);
                    // log_info("WEIGHT UPDATED AFTER BATCH!\n");
                }
            }

        }

        if (userstop == 0) {
            wct_weight_scaler(wct);
            wct->meta.num_trained++;

            total_error = iterate_error;
            total_acc = iterate_acc;
            counter = iterate_counter;

            if ((better_acc_condition((iterate_acc / iterate_counter), wct->meta.last_wr)) || (wct->meta.last_wr == 0.0)) {
                wct->meta.last_train = time(NULL);
                wct->meta.last_wr = total_acc / counter;
                wct_weight_save(wct);
                wct_meta_save(wct);
                log_info("WEIGHT UPDATED AFTER EPOCH!\n");
            }

            if (bar.current != bar.end) progressbar_done(&bar, info);
            wct->meta.patch++;
            i = 0;
            index++;
            if(!epoch) x++;
        }
    }

    printf("\n");
    log_info("Sequence fit Complete, WCT acc: %f\n", wct->meta.last_wr);
    dvec_free(&data);
    dvec_free(&fit_data);
    metrics_free(&metric);

    return (total_acc / counter);
}

double fit_op_table (wct_t *wct, cfg_t cfg, dataframe_t *df, int labelcol, int batch, int epoch, int iteration, double (*lr_func)(), double (*gradient_func)(), double (*distance_function)(), metrics_t (*estimate_function)(), void (*activation_function)(), int (*estimate_condition)(), double (*error_function)()) {
    dtab_t data;
    dtab_t label;
    dtab_copy(&df->x, &data);
    dtab_copy(&df->y, &label);

    if (labelcol > wct->label.col[0]) {
        log_error("Label Index is Out Of Bounds, Max Label is: %d\n", wct->label.col[0]);
        wct_exit(WCT_OUT_OF_BOUNDS);
    }

    log_info("Beginning Fit Table Operation, Press SHIFT+C (Upper Case C) To Abort!\n");

    int limit = data.row;
    int fit_iteration = iteration ? iteration : limit;
    if (fit_iteration > limit) fit_iteration = limit;

    int batch_size = batch ? batch : fit_iteration;
    if (batch_size > fit_iteration) batch_size = fit_iteration;

    metrics_t metric;
    double Y = 0.0;
    double total_error = 0.0;
    double total_acc = 0.0;
    double counter = 0.0;

    double iterate_error = 0.0;
    double iterate_acc = 0.0;
    double iterate_counter = 0.0;
    int userstop = 0;
    int label_col = labelcol;

    int bar_divider = fit_iteration / 300;
    bar_divider = bar_divider ? bar_divider : 1;

    double lr = 0.0;
    int (*better_acc_condition)();
    better_acc_condition = better_not_found;

    char info[32];
    char epoch_c[16];
    char key;

    if (!epoch) sprintf(epoch_c, "%d", epoch); else strcpy(epoch_c, "INFINITE");

    int e = 1;
    int x = epoch ? -1 : 0;
    int i = 0;
    int index = 0;

    if (epoch) epoch = fit_iteration;

    while (e) {
        if (x >= epoch) break;

        progressbar_t bar;
        progressbar_create(&bar, 0, fit_iteration);

        lr = lr_func(index, epoch, cfg.default_learning_rate);
        printf("Epoch: %d / %s (lr = %f)\n", index, epoch_c, lr);

        iterate_counter = 0.0;
        iterate_acc = 0.0;
        iterate_error = 0.0;

        while (i < fit_iteration) {
            for (int b = 0; b < batch_size; b++) {
                progressbar_update(&bar);

                if (labelcol == AUTOLABEL) label_col = wct_dynlabel_get(wct, cfg, data.data[i], data.col[i], distance_function, activation_function, estimate_condition);
                Y = label.data[i][label_col];

                metric = wct_evaluate(wct, cfg, data.data[i], data.col[i], label_col, distance_function, estimate_function, activation_function, estimate_condition, error_function, Y, 1);
                if (metric.misc.better_acc_condition != better_not_found) better_acc_condition = metric.misc.better_acc_condition;

                if ((better_acc_condition(1.0, 0.0) == wct->meta.last_wr) && (wct->meta.num_trained > 0)) {
                    log_info("PERFECT FIT!");
                    userstop = 1;
                    e = 0;
                    x = epoch;
                    i = fit_iteration;
                    break;
                }

                metric.misc.gradient_operation(wct, metric, lr, gradient_func);
                if (metric.error != -1) iterate_error += metric.error;
                if (metric.acc != -1) {
                    iterate_acc += metric.acc;
                    iterate_counter++;
                }

                sprintf(info, "loss: %f\tacc: %f", iterate_error, iterate_acc / iterate_counter);
                /*if ((fit_iteration % bar_divider) == 0)*/ progressbar_print(&bar, info);

                metrics_free(&metric);

                if (kbhit()) {
                    key = getch();
                    if (key == 'C') {
                        userstop = 1;
                        e = 0;
                        x = epoch;
                        i = fit_iteration;
                        break;
                    }
                }

                i++;
                if (i >= fit_iteration) break;
            }

            if (userstop == 0) {
                if (better_acc_condition((iterate_acc / iterate_counter), wct->meta.last_wr)) {
                    wct->meta.last_train = time(NULL);
                    // wct->meta.last_wr = iterate_acc / iterate_counter;
                    wct_weight_save(wct);
                    wct_meta_save(wct);
                    // log_info("WEIGHT UPDATED AFTER BATCH!\n");
                }
            }

        }

        if (userstop == 0) {
            wct_weight_scaler(wct);
            wct->meta.num_trained++;

            total_error = iterate_error;
            total_acc = iterate_acc;
            counter = iterate_counter;

            if ((better_acc_condition((iterate_acc / iterate_counter), wct->meta.last_wr)) || (wct->meta.last_wr == 0.0)) {
                wct->meta.last_train = time(NULL);
                wct->meta.last_wr = total_acc / counter;
                wct_weight_save(wct);
                wct_meta_save(wct);
                log_info("WEIGHT UPDATED AFTER EPOCH!\n");
            }

            if (bar.current != bar.end) progressbar_done(&bar, info);
            wct->meta.patch++;
            i = 0;
            index++;
            if(!epoch) x++;
        }
    }

    printf("\n");
    log_info("Table fit Complete, WCT acc: %f\n", wct->meta.last_wr);
    dtab_free(&data);
    dtab_free(&label);
    metrics_free(&metric);

    return (total_acc / counter);
}

typedef struct {
    wct_t *wct;
    cfg_t cfg;
    dataframe_t *df;
    int label_col;
    int batch;
    int epoch;
    int iteration;
    double (*lr_func)();
    double (*gradient_func)();
    double (*distance_function)();
    metrics_t (*estimate_function)();
    void (*activation_function)();
    int (*estimate_condition)();
    double (*error_function)();
} fit_args;

double wct_fit_base (wct_t *wct, cfg_t cfg, dataframe_t *df, int label_col, int batch, int epoch, int iteration, double (*lr_func)(), double (*gradient_func)(), double (*distance_function)(), metrics_t (*estimate_function)(), void (*activation_function)(), int (*estimate_condition)(), double (*error_function)()) {
    DECLARE_TIMER(fit);
    START_TIMER(fit);

    double ret;
    if ((dataframe_typename(*df) == DF_TABLE) || (dataframe_typename(*df) == DF_TABLE_SPLITTED)) {
        ret = fit_op_table(wct, cfg, df, label_col, batch, epoch, iteration, lr_func, gradient_func, distance_function, estimate_function, activation_function, estimate_condition, error_function);
    } else if ((dataframe_typename(*df) == DF_SEQUENCE) || (dataframe_typename(*df) == DF_SEQUENCE_SPLITTED)) {
        ret = fit_op_sequence(wct, cfg, df, label_col, batch, epoch, iteration, lr_func, gradient_func, distance_function, estimate_function, activation_function, estimate_condition, error_function);
    } else {
        log_error("Undefined Dataframe Behaviour, wct_fit() Cancelled!\n");
        wct_exit(WCT_ERROR);
    }

    STOP_TIMER(fit);
    log_info("wct_fit() - [took: %f]\n", GET_TIMER(fit));

    return ret;
}

double wct_fit_wrapper (fit_args a) {
    if (a.wct->meta.ready != 1) {
        log_error("WCT Is Not Initialized, Use wct_load() or wct_import()!\n");
        wct_exit(WCT_NOT_READY);
    }
    if (a.cfg.ready != 1) {
        config_init(&a.cfg);
    }
    int a_label_col = a.label_col ? a.label_col : 0;
    int a_batch = a.batch ? a.batch : 0;
    int a_epoch = a.epoch ? a.epoch : 1;
    int a_iteration = a.iteration ? a.iteration : 0;
    double (*a_lr_func)() = a.lr_func ? a.lr_func : cycle_lr;
    double (*a_gradient_func)() = a.gradient_func ? a.gradient_func : gradient_function;
    double (*a_distance_function)() = a.distance_function ? a.distance_function : wct_manhattan_distance;
    metrics_t (*a_estimate_function)() = a.estimate_function ? a.estimate_function : wct_risefall;
    void (*a_activation_function)() = a.activation_function ? a.activation_function : no_activation;
    int (*a_estimate_condition)() = a.estimate_condition ? a.estimate_condition : by_minimal_similarity;
    double (*a_error_function)() = a.error_function;

    double ret;
    ret = wct_fit_base(a.wct, a.cfg, a.df, a_label_col, a_batch, a_epoch, a_iteration, a_lr_func, a_gradient_func, a_distance_function, a_estimate_function, a_activation_function, a_estimate_condition, a_error_function);
    return ret;
}

typedef struct {
    wct_t *wct;
    cfg_t cfg;
    double *X;
    int X_len;
    int label_col;
    double lr;
    double (*gradient_func)();
    double (*distance_function)();
    metrics_t (*estimate_function)();
    void (*activation_function)();
    int (*estimate_condition)();
    double (*error_function)();
    double Y;
} feed_args;

void wct_feed_base (wct_t *wct, cfg_t cfg, double *X, int X_len, int label_col, double lr, double (*gradient_func)(), double (*distance_function)(), metrics_t (*estimate_function)(), void (*activation_function)(), int (*estimate_condition)(), double (*error_function)(), double Y) {
    metrics_t metric;
    metric = wct_evaluate(wct, cfg, X, X_len, label_col, distance_function, estimate_function, activation_function, estimate_condition, error_function, Y, 1);
    metric.misc.gradient_operation(wct, metric, lr, gradient_func);
    metrics_free(&metric);
}

void wct_feed_wrapper (feed_args a) {
    if (a.wct->meta.ready != 1) {
        log_error("WCT Is Not Initialized, Use wct_load() or wct_import()!\n");
        wct_exit(WCT_NOT_READY);
    }
    if (a.cfg.ready != 1) {
        config_init(&a.cfg);
    }

    int a_X_len = a.X_len ? a.X_len : 0;
    int a_label_col = a.label_col ? a.label_col : 0;
    double a_lr = a.lr ? a.lr : a.cfg.default_learning_rate;
    double (*a_gradient_func)() = a.gradient_func ? a.gradient_func : gradient_function;
    double (*a_distance_function)() = a.distance_function ? a.distance_function : wct_manhattan_distance;
    metrics_t (*a_estimate_function)() = a.estimate_function ? a.estimate_function : wct_risefall;
    void (*a_activation_function)() = a.activation_function ? a.activation_function : no_activation;
    int (*a_estimate_condition)() = a.estimate_condition ? a.estimate_condition : by_minimal_similarity;
    double (*a_error_function)() = a.error_function;

    if (a_X_len == 0) {
        log_error("Please Provide Valid .X And .X_len Argument!\n");
        wct_exit(WCT_INVALID_ARGUMENT);
    }

    wct_feed_base(a.wct, a.cfg, a.X, a_X_len, a_label_col, a_lr, a_gradient_func, a_distance_function, a_estimate_function, a_activation_function, a_estimate_condition, a_error_function, a.Y);
}

typedef struct {
    wct_t *wct;
    cfg_t cfg;
    dvec_t *input;
    int forecast_len;
    double (*distance_function)();
    void (*activation_function)();
    int (*estimate_condition)();
} forecast_args;

void wct_forecast_base (wct_t *wct, cfg_t cfg, dvec_t *input, int forecast_len, double (*distance_function)(), void (*activation_function)(), int (*estimate_condition)()) {
    if (input->size != wct->meta.features_len) {
        log_error("input->data Lenght is Not Equal to WCT Features Lenght!\n");\
        wct_exit(WCT_INVALID_ARGUMENT);
    }
    if (cfg.ready != 1) {
        log_error("Config is Not Initialized, Use config_init()!\n");\
        wct_exit(WCT_INVALID_CONFIG);
    }

    register unsigned int foundcount = 0;
    double sim = cfg.default_similarity;
    register unsigned int min_found = cfg.default_min_pattern_found;

    register unsigned int x;
    double dist = 0.0;
    int label_col = 0;

    dvec_t dest;

    for (int i = 0; i < forecast_len; i++) {
        dvec_init(&dest);

        for (int x = 0; x < wct->meta.features_len; x++) {
            dvec_append(&dest, input->data[x + i]);
        }

        metrics_t ret = wct_estimate(wct, cfg, dest.data, dest.size, label_col, distance_function, wct_regression, activation_function, estimate_condition, 1);
        if (ret.misc.acc_function == acc_not_found) {
            metrics_t ret2 = wct_estimate(wct, cfg, dest.data, dest.size, label_col, wct_euclidean_distance, wct_regression_with_kernel, activation_function, by_allnode, 1);
            dvec_append(input, wct_predicted_value_get(ret2.value));
            metrics_free(&ret2);
        } else {
            dvec_append(input, wct_predicted_value_get(ret.value));
        }
        metrics_free(&ret);

        dvec_free(&dest);
    }
}

void wct_forecast_wrapper (forecast_args a) {
    if (a.wct->meta.ready != 1) {
        log_error("WCT Is Not Initialized, Use wct_load() or wct_import()!\n");
        wct_exit(WCT_NOT_READY);
    }
    if (a.cfg.ready != 1) {
        config_init(&a.cfg);
    }
    if (a.input->size == 0) {
        log_error("Please Provide Valid input dvec_t Argument!\n");
        wct_exit(WCT_INVALID_ARGUMENT);
    }

    int a_forecast_len = a.forecast_len ? a.forecast_len : 1;
    double (*a_distance_function)() = a.distance_function ? a.distance_function : wct_manhattan_distance;
    void (*a_activation_function)() = a.activation_function ? a.activation_function : no_activation;
    int (*a_estimate_condition)() = a.estimate_condition ? a.estimate_condition : by_minimal_similarity;

    wct_forecast_base(a.wct, a.cfg, a.input, a_forecast_len, a_distance_function, a_activation_function, a_estimate_condition);
}

int wct_server(int port) {
    from_wct_server = 1;
    log_info("Running wct_server() unit tests");
    EWSUnitTestsRun();
    log_info("Unit tests passed. Starting wct_server() server");
    serverInit(&server);
    log_info("wct_server() server started on port: %d", port);
    acceptConnectionsUntilStoppedFromEverywhereIPv4(&server, port);
    serverDeInit(&server);
    return WCT_OK;
}

static THREAD_RETURN_TYPE STDCALL_ON_WIN32 stopAcceptingConnections(void* u) {
    serverStop(&server);
    return (THREAD_RETURN_TYPE) NULL;
}

struct Response* createResponseForRequest(const struct Request* request, struct Connection* connection) {    
    if (request->path == strstr(request->path, "/stop")) {
        pthread_t stopThread;
        pthread_create(&stopThread, NULL, &stopAcceptingConnections, connection->server);
        pthread_detach(stopThread);
        log_info("wct_server() stopped");
        return responseAllocWithFormat(200, "OK", "application/json", "{\"status\": 200, \"response\": \"%s\"}" , "exit");
    }

    if (request->path == strstr(request->path, "/exit")) {
        log_info("wct_server() exitting");
        wct_exit(WCT_OK);
    }

    if (request->path == strstr(request->path, "/api/v1/wct/op/estimate")) {
        if (!from_wct_server) return responseAllocWithFormat(400, "Bad Request", "application/json", "{\"status\": 400, \"response\": \"%s\"}" , "request made outside wct_server()");
        char* seq = strdupDecodeGETParam("seq=", request, "");
        char* col = strdupDecodeGETParam("col=", request, "");
        if (strcmp(seq, "") == 0) {
            struct Response* response = responseAllocWithFormat(400, "Bad Request", "application/json", "%s" , "{\"status\": 400, \"response\": \"seq is empty\"}");
            return response;
        }

        int label = 0;
        if (strcmp(col, "") == 0) label = AUTOLABEL; else sscanf(col, "%d", &label);

        dvec_t data;
        dvec_from_string(&data, seq);
        wcthelper_get_sequence(&data);
        free(seq);
        free(col);

        metrics_t metric = wct_estimate(&GLOBAL.wct, GLOBAL.cfg, data.data, data.size, label, DEFAULT_DISTANCE_FUNCTION, DEFAULT_ESTIMATION_FUNCTION, DEFAULT_ACTIVATION_FUNCTION, DEFAULT_ESTIMATION_CONDITION, 1);
        char value[128];
        for (int i = 0; i < metric.value.size; i++) if (i == 0) sprintf(value, "[%f, ", metric.value.data[i]); else if (i == metric.value.size - 1) sprintf(value + strlen(value), "%f]", metric.value.data[i]); else sprintf(value + strlen(value), "%f, ", metric.value.data[i]);

        char jsonResponse[512];
        sprintf(jsonResponse, "{\"status\" : %d, \"response\" : {\"value\": %s, \"class\" : %d, \"col\" : %d} }",
                200,
                value,
                metric.misc.class,
                metric.misc.label_col);

        dvec_free(&data);
        metrics_free(&metric);
        struct Response* response = responseAllocWithFormat(200, "OK", "application/json", "%s" , jsonResponse);
        return response;
    }

    if (request->path == strstr(request->path, "/api/v1/wct/op/evaluate")) {
        if (!from_wct_server) return responseAllocWithFormat(400, "Bad Request", "application/json", "{\"status\": 400, \"response\": \"%s\"}" , "request made outside wct_server()");
        char* seq = strdupDecodeGETParam("seq=", request, "");
        char* col = strdupDecodeGETParam("col=", request, "");
        char* y = strdupDecodeGETParam("y=", request, "");
        if (strcmp(seq, "") == 0) {
            struct Response* response = responseAllocWithFormat(400, "Bad Request", "application/json", "%s" , "{\"status\": 400, \"response\": \"seq is empty\"}");
            return response;
        }
        if (strcmp(y, "") == 0) {
            struct Response* response = responseAllocWithFormat(400, "Bad Request", "application/json", "%s" , "{\"status\": 400, \"response\": \"y (label) is empty\"}");
            return response;
        }

        int label = 0;
        double y_label = 0.0;
        if (strcmp(col, "") == 0) label = AUTOLABEL; else sscanf(col, "%d", &label);
        sscanf(y, "%lf", &y_label);

        dvec_t data;
        dvec_from_string(&data, seq);
        y_label = math_percentchange(data.data[0], y_label);
        wcthelper_get_sequence(&data);
        free(seq);
        free(col);

        metrics_t metric = wct_evaluate(&GLOBAL.wct, GLOBAL.cfg, data.data, data.size, label, DEFAULT_DISTANCE_FUNCTION, DEFAULT_ESTIMATION_FUNCTION, DEFAULT_ACTIVATION_FUNCTION, DEFAULT_ESTIMATION_CONDITION, NULL, y_label, 1);
        char value[128];
        for (int i = 0; i < metric.value.size; i++) if (i == 0) sprintf(value, "[%f, ", metric.value.data[i]); else if (i == metric.value.size - 1) sprintf(value + strlen(value), "%f]", metric.value.data[i]); else sprintf(value + strlen(value), "%f, ", metric.value.data[i]);

        char jsonResponse[512];
        sprintf(jsonResponse, "{\"status\" : %d, \"response\" : {\"value\": %s, \"class\" : %d, \"col\" : %d, \"acc\": %f, \"error\": %f} }",
                200,
                value,
                metric.misc.class,
                metric.misc.label_col,
                metric.acc,
                metric.error);

        dvec_free(&data);
        metrics_free(&metric);
        struct Response* response = responseAllocWithFormat(200, "OK", "application/json", "%s" , jsonResponse);
        return response;
    }

    if (request->path == strstr(request->path, "/api/v1/wct/op/forecast")) {
        if (!from_wct_server) return responseAllocWithFormat(400, "Bad Request", "application/json", "{\"status\": 400, \"response\": \"%s\"}" , "request made outside wct_server()");
        char *seq = strdupDecodeGETParam("seq=", request, "");
        char *len = strdupDecodeGETParam("len=", request, "");
        if (strcmp(seq, "") == 0) {
            struct Response* response = responseAllocWithFormat(400, "Bad Request", "application/json", "%s" , "{\"status\": 400, \"response\": \"seq is empty\"}");
            return response;
        }
        if (strcmp(len, "") == 0) {
            struct Response* response = responseAllocWithFormat(400, "Bad Request", "application/json", "%s" , "{\"status\": 400, \"response\": \"len is empty\"}");
            return response;
        }

        dvec_t data;
        dvec_from_string(&data, seq);
        double startpoint = data.data[0];
        int forecast_len = 0;
        sscanf(len, "%d", &forecast_len);

        char denormalize_base_seq[512];
        for (int i = 1; i < data.size; i++) {
            char buff[16];
            if (i == data.size - 1) sprintf(buff, "%f", data.data[i]); else sprintf(buff, "%f, ", data.data[i]);
            strcat(denormalize_base_seq, buff);
        }

        wcthelper_get_sequence(&data);

        char normalized_base_seq[512];
        for (int i = 0; i < data.size; i++) {
            char buff[16];
            if (i == data.size - 1) sprintf(buff, "%f", data.data[i]); else sprintf(buff, "%f, ", data.data[i]);
            strcat(normalized_base_seq, buff);
        }

        wct_forecast(&GLOBAL.wct, GLOBAL.cfg, &data, forecast_len, DEFAULT_DISTANCE_FUNCTION, DEFAULT_ACTIVATION_FUNCTION, DEFAULT_ESTIMATION_CONDITION);

        char normalized_result_seq[1024];
        for (int i = data.size - forecast_len; i < data.size; i++) {
            char buff[16];
            if (i == data.size - 1) sprintf(buff, "%f", data.data[i]); else sprintf(buff, "%f, ", data.data[i]);
            strcat(normalized_result_seq, buff);
        }

        wcthelper_denormalize(data.data, data.size, startpoint);

        char denormalize_result_seq[1024];
        for (int i = data.size - forecast_len; i < data.size; i++) {
            char buff[16];
            if (i == data.size - 1) sprintf(buff, "%f", data.data[i]); else sprintf(buff, "%f, ", data.data[i]);
            strcat(denormalize_result_seq, buff);
        }

        char jsonResponse[4096];
        sprintf(jsonResponse, "{\"status\" : %d, \"response\" : {\"input\": {\"normalized\": [%s], \"denormalized\": [%s]}, \"output\": {\"normalized\": [%s], \"denormalized\": [%s]}, \"merged\": {\"normalized\": [%s, %s], \"denormalized\": [%s, %s]}}}",
                200,
                normalized_base_seq,
                denormalize_base_seq,
                normalized_result_seq,
                denormalize_result_seq,
                normalized_base_seq,
                normalized_result_seq,
                denormalize_base_seq,
                denormalize_result_seq);

        dvec_free(&data);
        free(seq);
        free(len);

        struct Response* response = responseAllocWithFormat(200, "OK", "application/json", "%s" , jsonResponse);
        return response;
    }

    if (request->path == strstr(request->path, "/api/v1/wct/op/append")) {
        if (!from_wct_server) return responseAllocWithFormat(400, "Bad Request", "application/json", "{\"status\": 400, \"response\": \"%s\"}" , "request made outside wct_server()");
        char* weight = strdupDecodeGETParam("weight=", request, "");
        char* features = strdupDecodeGETParam("features=", request, "");
        char* label = strdupDecodeGETParam("label=", request, "");
        if (strcmp(weight, "") == 0) strcpy(weight, "0.1");
        if (strcmp(features, "") == 0) {
            struct Response* response = responseAllocWithFormat(400, "Bad Request", "application/json", "%s" , "{\"status\": 400, \"response\": \"features is empty\"}");
            return response;
        }
        if (strcmp(label, "") == 0) {
            struct Response* response = responseAllocWithFormat(400, "Bad Request", "application/json", "%s" , "{\"status\": 400, \"response\": \"label is empty\"}");
            return response;
        }

        dvec_t feat_vec;
        dvec_t label_vec;

        dvec_from_string(&feat_vec, features);
        dvec_from_string(&label_vec, label);

        if (feat_vec.size != GLOBAL.wct.features.col[0] + 1) {
            struct Response* response = responseAllocWithFormat(400, "Bad Request", "application/json", "{\"status\": 400, \"response\": \"features size not valid - size must be: %d\"}" , GLOBAL.wct.features.col[0] + 1);
            dvec_free(&feat_vec);
            dvec_free(&label_vec);
            return response;
        }
        if (label_vec.size != GLOBAL.wct.label.col[0]) {
            struct Response* response = responseAllocWithFormat(400, "Bad Request", "application/json", "{\"status\": 400, \"response\": \"label size not valid - size must be: %d\"}" , GLOBAL.wct.label.col[0]);
            dvec_free(&feat_vec);
            dvec_free(&label_vec);
            return response;
        }

        double factor_data = feat_vec.data[0];
        double weight_data = 0.0;
        sscanf(weight, "%lf", &weight_data);

        wcthelper_get_sequence(&feat_vec);
        for (int i = 0; i < label_vec.size; i++)
            label_vec.data[i] = math_percentchange(factor_data, label_vec.data[i]);

        free(weight);
        free(features);
        free(label);

        int ret = wct_append(&GLOBAL.wct, weight_data, feat_vec.data, feat_vec.size, label_vec.data, label_vec.size, factor_data, DEFAULT_DISTANCE_FUNCTION, DEFAULT_ESTIMATION_FUNCTION, DEFAULT_ACTIVATION_FUNCTION, DEFAULT_ESTIMATION_CONDITION, NULL);

        if (ret == -1) {

            struct Response* response = responseAllocWithFormat(400, "Bad Request", "application/json", "%s" , "{\"status\": 400, \"response\": \"features are not qualified to be appended\"}");
            dvec_free(&feat_vec);
            dvec_free(&label_vec);
            return response;

        } else {

            char jsonResponse[1024];
            char feat_value[256];
            for (int i = 0; i < GLOBAL.wct.features.col[GLOBAL.wct.features.row - 1]; i++) if (i == 0) sprintf(feat_value, "[%f, ", GLOBAL.wct.features.data[GLOBAL.wct.features.row - 1][i]); else if (i == GLOBAL.wct.features.col[GLOBAL.wct.features.row - 1] - 1) sprintf(feat_value + strlen(feat_value), "%f]", GLOBAL.wct.features.data[GLOBAL.wct.features.row - 1][i]); else sprintf(feat_value + strlen(feat_value), "%f, ", GLOBAL.wct.features.data[GLOBAL.wct.features.row - 1][i]);

            char label_value[256];
            for (int i = 0; i < GLOBAL.wct.label.col[GLOBAL.wct.label.row - 1]; i++) if (i == 0) sprintf(label_value, "[%f, ", GLOBAL.wct.label.data[GLOBAL.wct.label.row - 1][i]); else if (i == GLOBAL.wct.label.col[GLOBAL.wct.label.row - 1] - 1) sprintf(label_value + strlen(label_value), "%f]", GLOBAL.wct.label.data[GLOBAL.wct.label.row - 1][i]); else sprintf(label_value + strlen(label_value), "%f, ", GLOBAL.wct.label.data[GLOBAL.wct.label.row - 1][i]);

            sprintf(jsonResponse, "{\"status\" : %d, \"response\" : {\"index\": %d, \"dynlabel_value\": %d, \"featureindex\": %f, \"weight\": %f, \"features\" : %s, \"label\" : %s} }",
                    200,
                    GLOBAL.wct.weight.size - 1,
                    GLOBAL.wct.dynlabel.value.data[GLOBAL.wct.weight.size - 1],
                    GLOBAL.wct.featureindex.data[GLOBAL.wct.weight.size - 1],
                    GLOBAL.wct.weight.data[GLOBAL.wct.weight.size - 1],
                    feat_value,
                    label_value);

            dvec_free(&feat_vec);
            dvec_free(&label_vec);

            struct Response* response = responseAllocWithFormat(200, "OK", "application/json", "%s" , jsonResponse);
            return response;

        }
    }

    if (request->path == strstr(request->path, "/api/v1/wct/config/get")) {
        if (!from_wct_server) return responseAllocWithFormat(400, "Bad Request", "application/json", "{\"status\": 400, \"response\": \"%s\"}" , "request made outside wct_server()");
        char* section = strdupDecodeGETParam("section=", request, "default");
        char* key = strdupDecodeGETParam("key=", request, "");
        if (strcmp(key, "") == 0) {
            struct Response* response = responseAllocWithFormat(400, "Bad Request", "application/json", "%s" , "{\"status\": 400, \"response\": \"key is empty\"}");
            return response;
        }

        if (config_get(CONFIGDIR, section, key) == NULL) {
            struct Response* response = responseAllocWithFormat(404, "Not Found", "application/json", "%s" , "{\"status\": 404, \"response\": \"config and/or section not found\"}");
            return response;
        }

        char jsonResponse[512];
        sprintf(jsonResponse, "{\"status\" : %d, \"response\" : {\"section\": \"%s\", \"key\": \"%s\", \"value\" : %s} }",
                200,
                section,
                key,
                config_get(CONFIGDIR, section, key));

        free(key);

        struct Response* response = responseAllocWithFormat(200, "OK", "application/json", "%s" , jsonResponse);
        return response;
    }

    if (request->path == strstr(request->path, "/api/v1/wct/config/set")) {
        if (!from_wct_server) return responseAllocWithFormat(400, "Bad Request", "application/json", "{\"status\": 400, \"response\": \"%s\"}" , "request made outside wct_server()");
        char* section = strdupDecodeGETParam("section=", request, "default");
        char* key = strdupDecodeGETParam("key=", request, "");
        char* value = strdupDecodeGETParam("value=", request, "");
        if (strcmp(key, "") == 0) {
            struct Response* response = responseAllocWithFormat(400, "Bad Request", "application/json", "%s" , "{\"status\": 400, \"response\": \"key is empty\"}");
            return response;
        }
        if (strcmp(value, "") == 0) {
            struct Response* response = responseAllocWithFormat(400, "Bad Request", "application/json", "%s" , "{\"status\": 400, \"response\": \"value is empty\"}");
            return response;
        }

        if (config_get(CONFIGDIR, section, key) == NULL) {
            struct Response* response = responseAllocWithFormat(404, "Not Found", "application/json", "%s" , "{\"status\": 404, \"response\": \"config and/or section not found\"}");
            return response;
        }
        config_modify(CONFIGDIR, section, key, value);

        char jsonResponse[512];
        sprintf(jsonResponse, "{\"status\" : %d, \"response\" : {\"msg\": \"%s\", \"section\": \"%s\", \"key\": \"%s\", \"value\" : %s} }",
                200,
                "set config success",
                section,
                key,
                config_get(CONFIGDIR, section, key));

        free(key);

        struct Response* response = responseAllocWithFormat(200, "OK", "application/json", "%s" , jsonResponse);
        return response;
    }

    if (request->path == strstr(request->path, "/api/v1/wct/config")) {
        if (!from_wct_server) return responseAllocWithFormat(400, "Bad Request", "application/json", "{\"status\": 400, \"response\": \"%s\"}" , "request made outside wct_server()");
        char jsonResponse[1024];
        char *configJson = config_json();
        sprintf(jsonResponse, "{\"status\" : %d, \"response\" : %s }",
                200,
                configJson);

        struct Response* response = responseAllocWithFormat(200, "OK", "application/json", "%s" , jsonResponse);
        free(configJson);
        return response;
    }

    if (request->path == strstr(request->path, "/api/v1/wct/info")) {
        if (!from_wct_server) return responseAllocWithFormat(400, "Bad Request", "application/json", "{\"status\": 400, \"response\": \"%s\"}" , "request made outside wct_server()");
        char jsonResponse[1024];

        char type[16];
        switch(GLOBAL.wct.meta.type) {
            case CUSTOM:
                strcpy(type, "CUSTOM");
                break;
            case CLASSIFICATION:
                strcpy(type, "CLASSIFICATION");
                break;
            case REGRESSION:
                strcpy(type, "REGRESSION");
                break;
            case RISEFALL:
                strcpy(type, "RISEFALL");
                break;
            default:
                strcpy(type, "UNDEFINED");
                break;
        }
        char last_train_buffer[80];
        strftime(last_train_buffer, sizeof(last_train_buffer), "%Y.%m.%d %H:%M", localtime((long int *)&GLOBAL.wct.meta.last_train));
        
        sprintf(jsonResponse, "{\"status\" : %d, \"response\" : {\"type\": \"%s\", \"version\": \"%d.%d.%d\", \"num_optimize\": %d, \"num_trained\" : %d, \"last_wr\": %f, \"last_train\": \"%s\", \"core\": \"%s\", \"label_maxlen\": %d, \"islabelfitted\": %d, \"islabelencoded\": %d, \"asset_code\": \"%s\", \"timeseries_shift\": %d, \"range_step\": %d, \"range_start\": %d, \"range_end\": %d, \"features_len\": %d, \"similarity\": %f, \"min_pattern_found\": %d, \"min_prediction_rate\": %f, \"node_count\": %d} }",
                200,
                type,
                GLOBAL.wct.meta.major,
                GLOBAL.wct.meta.minor,
                GLOBAL.wct.meta.patch,
                GLOBAL.wct.meta.num_optimize,
                GLOBAL.wct.meta.num_trained,
                GLOBAL.wct.meta.last_wr,
                last_train_buffer,
                GLOBAL.wct.meta.core,
                GLOBAL.wct.meta.label_maxlen,
                GLOBAL.wct.meta.islabelfitted,
                GLOBAL.wct.meta.islabelencoded,
                GLOBAL.wct.meta.asset_code,
                GLOBAL.wct.meta.timeseries_shift,
                GLOBAL.wct.meta.range_step,
                GLOBAL.wct.meta.range_start,
                GLOBAL.wct.meta.range_end,
                GLOBAL.wct.meta.features_len,
                GLOBAL.wct.meta.similarity,
                GLOBAL.wct.meta.min_pattern_found,
                GLOBAL.wct.meta.min_prediction_rate,
                GLOBAL.wct.weight.size);

        struct Response* response = responseAllocWithFormat(200, "OK", "application/json", "%s" , jsonResponse);
        return response;
    }

    return responseAllocWithFormat(404, "Not Found", "application/json", "{\"status\": 404, \"response\": \"%s %s\"}" , request->path, "Not Found");
}
