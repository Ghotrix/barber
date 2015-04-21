#include <stdio.h>
#include <sys/types.h>
#include <sys/times.h>
#include <string.h>
#include <pthread.h>
#include <stdlib.h>
#include <unistd.h>

#define MAX_RAND 3000000
#define MICRO_SLEEP 100 /* Time interval 1/1000 msec */
#define CLIENTS_NUM 16

typedef struct {
	double waiting_time;
	int client_num;
	int is_ready;
	void *barber;
} _client;

typedef struct {
	int sleeping;
	int chaires_number;
	int time_to_wait;
	int queue_length;
	_client *clients;
} _barber;

void usage();
int init_data(int argc, char *argv[], _barber *data);

void *barber_f(void *arg);
void *client_f(void *arg);

const char *get_time();

pthread_mutex_t barber_mutex = PTHREAD_MUTEX_INITIALIZER,
				queue_mutex = PTHREAD_MUTEX_INITIALIZER, 
				sleeping_mutex = PTHREAD_MUTEX_INITIALIZER, 
				client_mutex[CLIENTS_NUM];

int main(int argc, char * argv[]) {
	char buf[9];
	int ret = 0;
	pthread_t barber_thread;
	pthread_t client_threads[CLIENTS_NUM];
	_barber barber;
	_client	clients[CLIENTS_NUM];

	
	barber.chaires_number = 0;
	barber.time_to_wait = 0;
	barber.queue_length = 0;
	barber.sleeping = 1;
	printf("%s Barber is sleeping.\n", get_time(buf));
	barber.clients = clients;

	ret = init_data(argc, argv, &barber);
	if (ret < 0)
		return ret;

	srand(time(NULL));
	
	for (int i = 0; i < CLIENTS_NUM; i++) {
		clients[i].client_num = i + 1;
		clients[i].waiting_time = 0;
		clients[i].is_ready = 0;
		clients[i].barber = &barber;
		pthread_mutex_init(&client_mutex[i], NULL);// = PTHREAD_MUTEX_INITIALIZER;

		pthread_create(&client_threads[i], NULL, client_f, (void *)&clients[i]);
	}

	pthread_create(&barber_thread, NULL, barber_f, (void *)&barber);

	pthread_join(barber_thread, NULL);

	return 0;
}

void *client_f(void *arg)
{
	char buf[9];

	_client *client = (_client *)arg;
	_barber *barber = client->barber;

	usleep(MAX_RAND * client->client_num + rand() % (MAX_RAND - 2000000));
	
	for (;;) {
		pthread_yield();
		
		pthread_mutex_lock(&client_mutex[client->client_num]);
		pthread_mutex_lock(&queue_mutex); 
		printf("%s Client %d comes to barbershop. Queue length: %d\n", get_time(buf), client->client_num, barber->queue_length);
		pthread_mutex_unlock(&queue_mutex);
		
		int ret;
		struct tms start_t, current_t;
		int is_leaving = 0, is_waiting = 0;
		for (;;) {
			ret = pthread_mutex_trylock(&barber_mutex);
			if (ret != 0 && is_waiting == 0) {
				times(&start_t);
				pthread_mutex_lock(&queue_mutex);
				if (barber->queue_length == barber->chaires_number) {
					printf("%s There are no free chairs. Client %d is leaving.\n", get_time(buf), client->client_num);
					is_leaving = 1;
					break;
				}
				else {
					barber->queue_length++;
					printf("%s Client %d sits at chair. Queue length: %d\n", get_time(buf), client->client_num, barber->queue_length);
					is_waiting = 1;
				};
				pthread_mutex_unlock(&queue_mutex);
			}
			else if (ret == 0) {
				if (is_waiting) {
					pthread_mutex_lock(&queue_mutex);
					barber->queue_length--;
					pthread_mutex_unlock(&queue_mutex);
				}
				pthread_mutex_lock(&sleeping_mutex);
				if (barber->sleeping) {
					printf("%s Client %d waking up barber.\n", get_time(buf), client->client_num);
					barber->sleeping = 0;
				}
				pthread_mutex_unlock(&sleeping_mutex);
				client->is_ready = 1;
				break;
			}
		
			times(&current_t);
			//pthread_mutex_lock(&time_mutex[client->client_num - 1]);
			client->waiting_time = (current_t.tms_utime - start_t.tms_utime) + (current_t.tms_stime - start_t.tms_stime);
			
			if (client->waiting_time / sysconf(_SC_CLK_TCK) > barber->time_to_wait) {
				pthread_mutex_lock(&queue_mutex);
				barber->queue_length--;
				printf("%s Client %d don't want to wait anymore, he`s leaving. Queue length: %d\n", get_time(buf), client->client_num, barber->queue_length);
				pthread_mutex_unlock(&queue_mutex);
				is_leaving = 1;
			}
			//pthread_mutex_unlock(&time_mutex[client->client_num - 1]);

			if (is_leaving) {
				pthread_mutex_unlock(&client_mutex[client->client_num]);
				break;
			}
		}

		//pthread_mutex_lock(&time_mutex[client->client_num - 1]);
		client->waiting_time = 0;
		//pthread_mutex_unlock(&time_mutex[client->client_num - 1]);

		usleep(rand() % (MAX_RAND * CLIENTS_NUM));
	}
}

void *barber_f(void *arg)
{
	char buf[9];
	_barber *barber = (_barber *)arg;
	for (;;) {
		for (int i = 0; i < CLIENTS_NUM; i++) {
			if (barber->clients[i].is_ready) {
				barber->clients[i].is_ready = 0;
				pthread_mutex_lock(&queue_mutex);
				printf("%s Client %d sits for haircut. Queue length: %d\n", get_time(buf), i + 1, barber->queue_length);
				pthread_mutex_unlock(&queue_mutex);
				usleep(rand() % MAX_RAND);
				printf("%s Client %d had it's haircut and leaving barbershop.\n", get_time(buf), i + 1);
				pthread_mutex_unlock(&client_mutex[i]);
				pthread_mutex_unlock(&barber_mutex);

				pthread_mutex_lock(&queue_mutex);
				pthread_mutex_lock(&sleeping_mutex);
				if (barber->queue_length == 0) {
					barber->sleeping = 1;
					printf("%s Barber is sleeping.\n", get_time(buf));
				}
				pthread_mutex_unlock(&sleeping_mutex);
				pthread_mutex_unlock(&queue_mutex);
			}
		}
	}
	
	pthread_exit((void *) 0);
}

int init_data(int argc, char *argv[], _barber *data)
{
	if (argc < 5) {
		usage();

		return -1;
	}

	for (int i = 0; i < argc; i++) {
		if (strcmp(argv[i], "-t") == 0) {
			sscanf(argv[i+1], "%d", &data->time_to_wait);
		} else if ( strcmp(argv[i], "-n") == 0) {
			sscanf(argv[i+1], "%d", &data->chaires_number);
		}
	}

	if (data->time_to_wait == 0 || data->chaires_number == 0) {
		usage();

		return -1;
	}
}

void usage()
{
	printf("Usage: ./barber -t 1 -n 2\n");
	printf("\t-n\tnumber of chaires, must be greater than 0\n");
	printf("\t-t\ttime of clients waiting before he leaves, also greater than 0\n");
}
/* Useful comment.*/
const char *get_time(char *buf)
{
	struct timespec _tp;
	struct tm *_tm = calloc(1, sizeof(struct tm));
	clock_gettime(0, &_tp);
	localtime_r(&_tp.tv_sec, _tm);
	sprintf(buf, "%02d:%02d:%02d.%03d", _tm->tm_hour, _tm->tm_min, _tm->tm_sec, _tp.tv_nsec / 1000000);
	free(_tm);
	return buf;
}
