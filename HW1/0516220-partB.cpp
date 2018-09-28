#include<pthread.h>
#include<iostream>
#include<cstdlib>
#include<unistd.h>
#include<random>
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
} m1, m2;

struct {
	pthread_mutex_t	mutex;
	pthread_mutex_t update_list;
	pthread_mutex_t	g_lock;
	pthread_mutex_t	compete_machine;
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
int thread_counter4 = 0;
list<int> min_next_play_time;
list<int>::iterator it;

void *customer(customer_data *cus_address);

int main(int argc, char *argv[]){
	// ifstream fin(argv[1]);
    int total_customer;
	
	m1.now_player = -1;
    m2.now_player = -1;

	// fin >> G >> total_customer;
	cin >> G >> total_customer;
	cus_no_prize = total_customer;

	pthread_t tid[total_customer];
	pthread_attr_t attr[total_customer];
	
	(void)	pthread_mutex_init(&m1.mutex, NULL);
	(void)	pthread_mutex_init(&m2.mutex, NULL);
	(void)	pthread_mutex_init(&thread_controller.mutex, NULL);
	(void)	pthread_mutex_init(&thread_controller.update_list, NULL);
	(void)	pthread_mutex_init(&thread_controller.g_lock, NULL);
	(void)	pthread_mutex_init(&thread_controller.compete_machine, NULL);
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
		if(thread_counter3 == cus_no_prize){
			thread_counter3 = 0;
			while(1){
				if(!pthread_mutex_trylock(&thread_controller.thread_delay_lock)){
					(void) pthread_mutex_unlock(&thread_controller.thread_delay_lock);
					(void) pthread_cond_broadcast(&thread_controller.thread_delay);
					break;
				}
			}
		}
		if(thread_counter4 == cus_no_prize){ // All child thread are done
			if(m1.now_player == -1 && m2.now_player == -1) G_counter = 0;

			time_counter++;
			thread_counter4 = 0;

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
		if(m1.now_player == cus.id || m2.now_player == cus.id){
			cus.round_counter++;
			/* Finish Part */
			int *now_player_pointer;
			int machine_id = 0;
			if(m1.now_player == cus.id){
				now_player_pointer = &m1.now_player;
				machine_id = 1;
			}else if(m2.now_player == cus.id){
				now_player_pointer = &m2.now_player;
				machine_id = 2;
			}
			
			int try_lock_rv;
			try_lock_rv = pthread_mutex_lock(&thread_controller.g_lock);
			G_counter++;
			if(G_counter == G){
				if(try_lock_rv == 0){
					(void) pthread_mutex_lock(&thread_controller.cout_lock);
					cout << time_counter << " " << (*now_player_pointer)+1 << " " << "finish playing YES #" << machine_id << endl;
					(void) pthread_mutex_unlock(&thread_controller.cout_lock);
					G_counter = 0;
					*now_player_pointer = -1;
					cus_no_prize--;
					if(machine_id == 1) (void) pthread_mutex_unlock(&m1.mutex);
					else if(machine_id == 2) (void) pthread_mutex_unlock(&m2.mutex);
					(void) pthread_mutex_unlock(&thread_controller.g_lock);
					pthread_exit(0);
				}
			}else (void) pthread_mutex_unlock(&thread_controller.g_lock);

			if(cus.round_counter == cus.total_round){ // Get prize
				(void) pthread_mutex_lock(&thread_controller.cout_lock);
                cout << time_counter << " " << (*now_player_pointer)+1 << " " << "finish playing YES #" << machine_id << endl;
				(void) pthread_mutex_unlock(&thread_controller.cout_lock);
                G_counter = 0;
                *now_player_pointer = -1;
                cus_no_prize--;
				if(machine_id == 1) (void) pthread_mutex_unlock(&m1.mutex);
				else if(machine_id == 2) (void) pthread_mutex_unlock(&m2.mutex);
				pthread_exit(0);
			}else if((cus.round_counter % cus.play_round == 0) && cus.round_counter > 0){
				(void) pthread_mutex_lock(&thread_controller.cout_lock);
				cout << time_counter << " " << (*now_player_pointer)+1 << " " << "finish playing NO #" << machine_id << endl;
				(void) pthread_mutex_unlock(&thread_controller.cout_lock);
                cus.next_play_time = time_counter + cus.rest_time;
				*now_player_pointer = -1;
				if(machine_id == 1) (void) pthread_mutex_unlock(&m1.mutex);
				else if(machine_id == 2) (void) pthread_mutex_unlock(&m2.mutex);

				(void) pthread_mutex_lock(&thread_controller.update_list);
				if(min_next_play_time.size() == 0){ // If queue is empty
                    min_next_play_time.push_back(cus.next_play_time);
				}else{
					it = min_next_play_time.begin();
					for(int i = 1; i <= min_next_play_time.size(); i++){
						if(cus.next_play_time == *it){
							min_next_play_time.insert(it, cus.next_play_time);
							break;
						}else if(i == min_next_play_time.size() && cus.next_play_time >= *it){
							min_next_play_time.push_back(cus.next_play_time);
							break;
						}else if(cus.next_play_time < *it){
							min_next_play_time.insert(it, cus.next_play_time);
							break;
						}
						it++;
					}
				}
				(void) pthread_mutex_unlock(&thread_controller.update_list);
			}
			(void) pthread_mutex_lock(&thread_controller.thread_delay_lock);
			thread_counter1++;
			(void) pthread_cond_wait(&thread_controller.thread_delay, &thread_controller.thread_delay_lock);
			(void) pthread_mutex_unlock(&thread_controller.thread_delay_lock);

			(void) pthread_mutex_lock(&thread_controller.thread_delay_lock);
			thread_counter2++;
			(void) pthread_cond_wait(&thread_controller.thread_delay, &thread_controller.thread_delay_lock);
			(void) pthread_mutex_unlock(&thread_controller.thread_delay_lock);

			(void) pthread_mutex_lock(&thread_controller.thread_delay_lock);
			thread_counter3++;
			(void) pthread_cond_wait(&thread_controller.thread_delay, &thread_controller.thread_delay_lock);
			(void) pthread_mutex_unlock(&thread_controller.thread_delay_lock);
		}else{
			(void) pthread_mutex_lock(&thread_controller.thread_delay_lock);
			thread_counter1++;
			(void) pthread_cond_wait(&thread_controller.thread_delay, &thread_controller.thread_delay_lock);
			(void) pthread_mutex_unlock(&thread_controller.thread_delay_lock);
			if(m1.now_player == -1 && m2.now_player == -1){
				int try_lock_rv1, try_lock_rv2;
				if(cus.next_play_time <= time_counter && cus.next_play_time == min_next_play_time.front()){
					if(!pthread_mutex_trylock(&thread_controller.compete_machine)){
						random_device rd; // random generator seed
						default_random_engine rand_gen( rd() ); // generate random number
						uniform_int_distribution<int> uni_dis_rand_m(1, 2); // make sure that random number is a uniform distribution between 1 and 2
						int random_m = uni_dis_rand_m(rand_gen);
						
						if(random_m == 1){
							(void) pthread_mutex_lock(&m1.mutex);
							m1.now_player = cus.id;
							min_next_play_time.pop_front();
							(void) pthread_mutex_lock(&thread_controller.cout_lock);
							cout << time_counter << " " << cus.id+1 << " " << "start playing #1" << endl;
							(void) pthread_mutex_unlock(&thread_controller.cout_lock);
						}else if(random_m == 2){
							(void) pthread_mutex_lock(&m2.mutex);
							m2.now_player = cus.id;
							min_next_play_time.pop_front();
							(void) pthread_mutex_lock(&thread_controller.cout_lock);
							cout << time_counter << " " << cus.id+1 << " " << "start playing #2" << endl;
							(void) pthread_mutex_unlock(&thread_controller.cout_lock);
						}
						(void) pthread_mutex_unlock(&thread_controller.compete_machine);
					}
				}
			}

			(void) pthread_mutex_lock(&thread_controller.thread_delay_lock);
			thread_counter2++;
			(void) pthread_cond_wait(&thread_controller.thread_delay, &thread_controller.thread_delay_lock);
			(void) pthread_mutex_unlock(&thread_controller.thread_delay_lock);
			if(((m1.now_player == -1) ^ (m2.now_player == -1)) && (m1.now_player != cus.id && m2.now_player != cus.id)){
				int *now_player_pointer;
				int empty_machine_id = 0;
				if(m1.now_player == -1){
					now_player_pointer = &m1.now_player;
					empty_machine_id = 1;
				}else if(m2.now_player == -1){
					now_player_pointer = &m2.now_player;
					empty_machine_id = 2;
				}
				
				int try_lock_rv;
				if(cus.next_play_time <= time_counter && cus.next_play_time == min_next_play_time.front()){
					if(empty_machine_id == 1) try_lock_rv = pthread_mutex_trylock(&m1.mutex);
					else if(empty_machine_id == 2) try_lock_rv = pthread_mutex_trylock(&m2.mutex);
					if(try_lock_rv == 0){
						*now_player_pointer = cus.id;
						min_next_play_time.pop_front();
						(void) pthread_mutex_lock(&thread_controller.cout_lock);
						cout << time_counter << " " << cus.id+1 << " " << "start playing #" << empty_machine_id << endl;
						(void) pthread_mutex_unlock(&thread_controller.cout_lock);
					}
				}
			}

			(void) pthread_mutex_lock(&thread_controller.thread_delay_lock);
			thread_counter3++;
			(void) pthread_cond_wait(&thread_controller.thread_delay, &thread_controller.thread_delay_lock);
			(void) pthread_mutex_unlock(&thread_controller.thread_delay_lock);
			if(m1.now_player != -1 && m1.now_player != cus.id && m2.now_player != -1 && m2.now_player != cus.id){
				if(cus.next_play_time == time_counter){
					(void) pthread_mutex_lock(&thread_controller.cout_lock);
					cout << time_counter << " " << cus.id+1 << " " << "wait in line" << endl;
					(void) pthread_mutex_unlock(&thread_controller.cout_lock);
				}
			}
		}
		
		(void) pthread_mutex_lock(&thread_controller.mutex);
		thread_counter4++;
		(void) pthread_cond_wait(&thread_controller.child_thread, &thread_controller.mutex);
		(void) pthread_mutex_unlock(&thread_controller.mutex);
	}
}