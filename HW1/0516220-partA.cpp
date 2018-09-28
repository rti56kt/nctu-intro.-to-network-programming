#include<pthread.h>
#include<iostream>
#include<cstdlib>
#include<sys/types.h>
#include<unistd.h>
#include<time.h>
#include<string.h>
#include<fstream>
#include<list>

using namespace std;

typedef struct{
	int id;
    int arrive_time;
    int play_round;
    int rest_time;
    int total_round;
	int next_play_time = 0;
	int round_counter = 0;
} customer_data;

struct {
	pthread_mutex_t	mutex;
	int	now_player;
} claw_machine;

struct {
	pthread_mutex_t	mutex;
	pthread_mutex_t	cout_lock;
	pthread_mutex_t	thread_delay_lock;
	pthread_cond_t child_thread;
	pthread_cond_t thread_delay;
} thread_controller;

int G, G_counter, cus_no_prize;
int time_counter = 0;
int thread_counter1 = 0;
int thread_counter2 = 0;
int thread_counter3 = 0;
list<int> min_next_play_time;
list<int>::iterator it;

void *customer(customer_data *cus_address);

int main(int argc, char *argv[]){
	// ifstream fin(argv[1]);
    int total_customer;
	
	claw_machine.now_player = -1;

	// fin >> G >> total_customer;
	cin >> G >> total_customer;
	cus_no_prize = total_customer;

	pthread_t tid[total_customer];
	pthread_attr_t attr[total_customer];
	
	(void)	pthread_mutex_init(&claw_machine.mutex, NULL);
	(void)	pthread_mutex_init(&thread_controller.mutex, NULL);
	(void)	pthread_mutex_init(&thread_controller.cout_lock, NULL);
	(void)	pthread_mutex_init(&thread_controller.thread_delay_lock, NULL);
	(void)	pthread_cond_init(&thread_controller.child_thread, NULL);
	(void)	pthread_cond_init(&thread_controller.thread_delay, NULL);

    customer_data cus[total_customer];
	for(int i = 0; i < total_customer; i++){
		cus[i].id = i;
        // fin >> cus[i].arrive_time >> cus[i].play_round >> cus[i].rest_time >> cus[i].total_round;
		cin >> cus[i].arrive_time >> cus[i].play_round >> cus[i].rest_time >> cus[i].total_round;
        cus[i].next_play_time = cus[i].arrive_time;
    }
	
	it = min_next_play_time.begin();
	for(int i = 0; i < total_customer; i++){
		if(min_next_play_time.empty()){
			min_next_play_time.push_back(cus[i].next_play_time);
		}else{
			for(int j = 1; j <= min_next_play_time.size(); j++){
				if(cus[i].next_play_time >= *it){
					if(j == min_next_play_time.size()){
						min_next_play_time.push_back(cus[i].next_play_time);
						break;
					}else it++;
				}else{
					min_next_play_time.insert(it, cus[i].next_play_time);
                    break;
				}
			}
		}
	}

	for(int i = 0; i < total_customer; i++){
		(void)	pthread_attr_init(&attr[i]);		
		(void) 	pthread_attr_setdetachstate(&attr[i], PTHREAD_CREATE_DETACHED);
		pthread_create(&tid[i], &attr[i], (void *(*)(void *))customer, &cus[i]);
	}
    // fin.close();

	G_counter = 0;
	
	while(cus_no_prize){
		if(thread_counter1 == cus_no_prize){
			thread_counter1 = 0;
			while(1){
				if(!pthread_mutex_trylock(&thread_controller.thread_delay_lock)){
					(void) pthread_mutex_unlock(&thread_controller.thread_delay_lock);
					(void) pthread_cond_broadcast(&thread_controller.thread_delay);
					break;
				}
			}
		}
		if(thread_counter2 == cus_no_prize){
			thread_counter2 = 0;
			while(1){
				if(!pthread_mutex_trylock(&thread_controller.thread_delay_lock)){
					(void) pthread_mutex_unlock(&thread_controller.thread_delay_lock);
					(void) pthread_cond_broadcast(&thread_controller.thread_delay);
					break;
				}
			}
		}
		if(thread_counter3 == cus_no_prize){ // All child thread are done
			if(claw_machine.now_player != -1) G_counter++;
			else G_counter = 0;

			time_counter++;
			thread_counter3 = 0;
			while(1){
				if(!pthread_mutex_trylock(&thread_controller.mutex)){
					(void) pthread_mutex_unlock(&thread_controller.mutex);
					(void) pthread_cond_broadcast(&thread_controller.child_thread);
					break;
				}
			}
		}
	}
	return 0;
}

void *customer(customer_data *cus_address){
	customer_data cus = *cus_address;
	while(1){
		if(claw_machine.now_player == cus.id){
			cus.round_counter++;
			/* Finish Part */
			if(cus.round_counter == cus.total_round || G_counter == G){ // Get prize
				(void) pthread_mutex_lock(&thread_controller.cout_lock);
                cout << time_counter << " " << claw_machine.now_player+1 << " " << "finish playing YES" << endl;
				(void) pthread_mutex_unlock(&thread_controller.cout_lock);
                G_counter = 0;
                claw_machine.now_player = -1;
                cus_no_prize--;
				(void) pthread_mutex_unlock(&claw_machine.mutex);
				pthread_exit(0);
			}else if(cus.round_counter%cus.play_round == 0 && cus.round_counter > 0){ // Did not get prize
				(void) pthread_mutex_lock(&thread_controller.cout_lock);
				cout << time_counter << " " << claw_machine.now_player+1 << " " << "finish playing NO" << endl;
				(void) pthread_mutex_unlock(&thread_controller.cout_lock);
                cus.next_play_time = time_counter + cus.rest_time;
				claw_machine.now_player = -1;
				(void) pthread_mutex_unlock(&claw_machine.mutex);

				if(min_next_play_time.size() == 0){ // If queue is empty
                    min_next_play_time.push_back(cus.next_play_time);
				}else{
					it = min_next_play_time.begin();
					for(int i = 1; i <= min_next_play_time.size(); i++){
						if(cus.next_play_time == *it){ // If next play time equals to another one in queue
							min_next_play_time.insert(it, cus.next_play_time);
							break;
						}else if(i == min_next_play_time.size() && cus.next_play_time >= *it){ // If next play time is bigger than others in queue
							min_next_play_time.push_back(cus.next_play_time);
							break;
						}else if(cus.next_play_time < *it){ // If next play time is smaller than others in queue
							min_next_play_time.insert(it, cus.next_play_time);
							break;
						}
						it++;
					}
				}
			}
			(void) pthread_mutex_lock(&thread_controller.thread_delay_lock);
			thread_counter1++;
			(void) pthread_cond_wait(&thread_controller.thread_delay, &thread_controller.thread_delay_lock);
			(void) pthread_mutex_unlock(&thread_controller.thread_delay_lock);

			(void) pthread_mutex_lock(&thread_controller.thread_delay_lock);
			thread_counter2++;
			(void) pthread_cond_wait(&thread_controller.thread_delay, &thread_controller.thread_delay_lock);
			(void) pthread_mutex_unlock(&thread_controller.thread_delay_lock);
		}else{
			/* Start Part */
			(void) pthread_mutex_lock(&thread_controller.thread_delay_lock);
			thread_counter1++;
			(void) pthread_cond_wait(&thread_controller.thread_delay, &thread_controller.thread_delay_lock); // Wait for now player update
			(void) pthread_mutex_unlock(&thread_controller.thread_delay_lock);
			if(claw_machine.now_player == -1){
				int try_lock_rv;
				if(cus.next_play_time <= time_counter && cus.next_play_time == min_next_play_time.front()){
					try_lock_rv = pthread_mutex_trylock(&claw_machine.mutex);

					if(try_lock_rv == 0){
						claw_machine.now_player = cus.id;
						min_next_play_time.pop_front();
						(void) pthread_mutex_lock(&thread_controller.cout_lock);
						cout << time_counter << " " << cus.id+1 << " " << "start playing" << endl;
						(void) pthread_mutex_unlock(&thread_controller.cout_lock);
					}
				}
			}
			/* Wait Part */
			(void) pthread_mutex_lock(&thread_controller.thread_delay_lock);
			thread_counter2++;
			(void) pthread_cond_wait(&thread_controller.thread_delay, &thread_controller.thread_delay_lock); // Wait for now player update
			(void) pthread_mutex_unlock(&thread_controller.thread_delay_lock);
			if(claw_machine.now_player != -1 && claw_machine.now_player != cus.id){
				if(cus.next_play_time == time_counter){
					(void) pthread_mutex_lock(&thread_controller.cout_lock);
					cout << time_counter << " " << cus.id+1 << " " << "wait in line" << endl;
					(void) pthread_mutex_unlock(&thread_controller.cout_lock);
				}
			}
		}
		
		(void) pthread_mutex_lock(&thread_controller.mutex);
		thread_counter3++;
		(void) pthread_cond_wait(&thread_controller.child_thread, &thread_controller.mutex); // Wait for all threads finish this round
		(void) pthread_mutex_unlock(&thread_controller.mutex);
	}
}