#include<iostream>
#include<cstdlib>
#include<fstream>
#include<list>

using namespace std;

typedef struct{
    int arrive_time;
    int play_round;
    int rest_time;
    int total_round;
    int next_play_time = 0;
    int round_counter = 0;
} customer;

int main(int argc, char *argv[]){
    ifstream fin(argv[1]);
    int G, total_customer;
    list<int> cus_in_line;
    list<int>::iterator it;

    fin >> G >> total_customer;
    
    customer cus[total_customer];

    for(int i = 0; i < total_customer; i++){ // Read input
        fin >> cus[i].arrive_time >> cus[i].play_round >> cus[i].rest_time >> cus[i].total_round;
        cus[i].next_play_time = cus[i].arrive_time;
        cus_in_line.push_back(i); // Push everyone in queue
    }
    fin.close();

    int cus_no_prize = total_customer;
    int G_counter = 0;
    int now_player = -1;
    int time_counter = 0;
    
    while(cus_no_prize){
        if(now_player != -1){
            G_counter++;
            cus[now_player].round_counter++;
        }else G_counter = 0;

        /* Finish Part */
        if(now_player != -1){
            if(cus[now_player].round_counter == cus[now_player].total_round || G_counter == G){ // Get prize
                cus_no_prize--;
                cout << time_counter << " " << now_player+1 << " " << "finish playing YES" << endl;
                G_counter = 0;
                now_player = -1;
            }else if(cus[now_player].round_counter%cus[now_player].play_round == 0 && cus[now_player].round_counter > 0){ // Didn't get prize
                cout << time_counter << " " << now_player+1 << " " << "finish playing NO" << endl;
                cus[now_player].next_play_time = time_counter + cus[now_player].rest_time;
                
                // Back into queue
                if(cus_in_line.size() == 0){ // If queue is empty
                    cus_in_line.push_back(now_player);
                }else{ // If queue is not empty
                    it = cus_in_line.begin();
                    for(int i = 1; i <= cus_in_line.size(); i++){ // Compare to everyone's next play time(or arrive time) in the queue
                        if(cus[now_player].next_play_time == cus[*it].arrive_time){ // Case that next play time equals to someone's arrive time
                            cus_in_line.insert(it, now_player);
                            break;
                        }else if(i == cus_in_line.size() && cus[now_player].next_play_time >= cus[*it].next_play_time){ // Case that next play time equals to the last guy's next play time
                            cus_in_line.push_back(now_player);
                            break;
                        }else if(cus[now_player].next_play_time < cus[*it].next_play_time){ // Case that next play time is smaller than someone's next play time in the queue
                            cus_in_line.insert(it, now_player);
                            break;
                        }
                        it++;
                    }
                }

                now_player = -1;
            }
        }
        
        /* Wait Part */
        it = cus_in_line.begin();
        for(int i = 1; i <= cus_in_line.size(); i++){
            if(cus[*it].next_play_time == time_counter){
                if(now_player != -1){ // Someone is playing now
                    cout << time_counter << " " << (*it)+1 << " " << "wait in line" << endl;
                }else{ // No one is playing, but the first guy in the queue is going to play
                    if(i != 1){ // People who wait behind the first guy still have to wait
                        cout << time_counter << " " << (*it)+1 << " " << "wait in line" << endl;
                    }
                }
            }else if(cus[*it].next_play_time > time_counter){ // If next play time is bigger than current time means that is future events, just break the loop
                break;
            }
            it++;
        }

        /* Start Part */
        if(!cus_in_line.empty()){ // If there's someone in the queue
            if(now_player == -1 && cus[cus_in_line.front()].next_play_time <= time_counter){ // If no one is playing now
                now_player = cus_in_line.front();
                cus_in_line.pop_front();
                cout << time_counter << " " << now_player+1 << " " << "start playing" << endl;
            }
        }

        time_counter++;
    }
}