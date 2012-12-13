#include <stdio.h>
#include <sys/types.h>
#include <string.h>
#include <pthread.h>
#include <stdlib.h>
#include <unistd.h>

#define MAX_RAND 3000000
#define MICRO_SLEEP 100000 /* Time interval 1 msec */

typedef struct {
	int client_number;
} _client;

typedef struct {
	int sleeping;
	int chaires_number;
	int time_to_wait;
	int queue_length;
	_client *client;
} barbershop;

void usage();
int init_data(int argc, char *argv[], barbershop *data);
void *barber_cuts(void *arg);
const char *get_time();

pthread_mutex_t barber_mutex, queue_mutex, sleeping_mutex, time_mutex;

int main(int argc, char * argv[]) {
	int ret = 0;
	pthread_t thread;
	barbershop data;
	
	data.chaires_number = 0;
	data.time_to_wait = 0;
	data.queue_length = 0;
	data.sleeping = 1;

	ret = init_data(argc, argv, &data);
	if (ret < 0)
		return ret;

	srand(time(NULL));
	
	int client_counter = 0;
	for (;;) {
		_client *client = malloc(sizeof(_client));
		client->client_number = ++client_counter;
		data.client = client;
		pthread_create(&thread, NULL, barber_cuts, (void *)&data);
		usleep(rand() % (MAX_RAND - 1000000));
	}
	
	return 0;
}

void *barber_cuts(void *arg)
{
	char buf[9];
	barbershop *barb = (barbershop *)arg;
	int ret = 0;
	int client_number = barb->client->client_number;
	pthread_mutex_lock(&sleeping_mutex);
	if (barb->sleeping)
		printf("%s Barber is sleeping.\n", get_time(buf));
	pthread_mutex_unlock(&sleeping_mutex);

	pthread_mutex_lock(&queue_mutex); 
		printf("%s Client %d comes to barbershop. Queue length: %d\n", get_time(buf), client_number, barb->queue_length);
	pthread_mutex_unlock(&queue_mutex);

	int time = 0;
	int client_waiting = 0;
	for (;;) {
		ret = pthread_mutex_trylock(&barber_mutex);
		if (ret != 0 && client_waiting == 0) {
			pthread_mutex_lock(&queue_mutex);
			if (barb->queue_length == barb->chaires_number) {
				printf("There are no free chairs. Client %d is leaving.\n", client_number);
				free(barb->client);
				pthread_exit((void *) 1);
			}
			else {
				barb->queue_length++;
				client_waiting = 1;
				printf("Client %d sits at chair. Queue length: %d\n", client_number, barb->queue_length);
			};
			pthread_mutex_unlock(&queue_mutex);
		}
		else if (ret == 0) {
			//pthread_mutex_unlock(&barber_mutex);
			break;
		}
		
		if (time >= barb->time_to_wait) {
			pthread_mutex_lock(&queue_mutex);
				barb->queue_length--;
				printf("Client %d don't want to wait anymore, he`s leaving. Queue length: %d\n", client_number, barb->queue_length);
			pthread_mutex_unlock(&queue_mutex);
			free(barb->client);
			pthread_exit((void *) 1);
		}

		time += MICRO_SLEEP;
		usleep(MICRO_SLEEP);
	}
	
	pthread_mutex_lock(&sleeping_mutex);
	if (barb->sleeping) {
		printf("%s Client %d waking up barber.\n", get_time(buf), client_number);
		barb->sleeping = 0;
	}
	pthread_mutex_unlock(&sleeping_mutex);
	
	pthread_mutex_lock(&queue_mutex);
	//pthread_mutex_lock(&barber_mutex);
	printf("%s Client %d sits for haircut. Queue length: %d\n", get_time(buf), client_number, barb->queue_length);
	usleep(rand() % MAX_RAND);
	pthread_mutex_lock(&sleeping_mutex);
	printf("%s Client %d had it's haircut and leaving barbershop.\n", get_time(buf), client_number);
	pthread_mutex_unlock(&barber_mutex);
	pthread_mutex_unlock(&queue_mutex);
	
	pthread_mutex_lock(&queue_mutex); 
		if (barb->queue_length == 0) {
			barb->sleeping = 1;
		}
	pthread_mutex_unlock(&sleeping_mutex);
	pthread_mutex_unlock(&queue_mutex);

	free(barb->client);
	pthread_exit((void *) 0);
}

int init_data(int argc, char *argv[], barbershop *data)
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
