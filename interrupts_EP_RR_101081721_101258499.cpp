/**
 * @file interrupts.cpp
 * @author SamyKaddour,Tharsan Sivathasan 
 * @brief template main.cpp file for Assignment 3 Part 1 of SYSC4001
 * 
 */

#include"interrupts_101081721_101258499.hpp"

void EP_RR_scheduler(std::vector<PCB> &ready_queue) {
    std::sort( 
                ready_queue.begin(),
                ready_queue.end(),
                []( const PCB &a, const PCB &b ){
                    if(a.priority == b.priority){
                        return a.arrival_time < b.arrival_time; //fifo for priority
                    }
                    return a.priority < b.priority; //smaller has the higher priority
                } 
            );
}

struct WaitingProcess{
    PCB process;
    unsigned int remaining_io;
};

std::tuple<std::string /* add std::string for bonus mark */ > run_simulation(std::vector<PCB> list_processes) {

    std::vector<PCB> ready_queue;   //The ready queue of processes
    std::vector<WaitingProcess> wait_queue; //The wait queue of processes
    std::vector<PCB> job_list;      //A list to keep track of all the processes. This is similar
                                    //to the "Process, Arrival time, Burst time" table that you
                                    //see in questions. You don't need to use it, I put it here
                                    //to make the code easier :).

    unsigned int current_time = 0;
    bool cpu_idle = true;
    unsigned int time_slice_used = 0;
    PCB running;

    //Initialize an empty running process
    idle_CPU(running);

    std::string execution_status;

    //make the output table (the header row)
    execution_status = print_exec_header();

    //Loop while till there are no ready or waiting processes.
    //This is the main reason I have job_list, you don't have to use it.
    while(!all_process_terminated(job_list) || job_list.empty()) {

        //Inside this loop, there are three things you must do:
        // 1) Populate the ready queue with processes as they arrive
        // 2) Manage the wait queue
        // 3) Schedule processes from the ready queue

        //Population of ready queue is given to you as an example.
        //Go through the list of proceeses
        for(auto &process : list_processes) {
            if(process.arrival_time == current_time) {//check if the AT = current time
                //if so, assign memory and put the process into the ready queue
                assign_memory(process);

                process.state = READY;  //Set the process state to READY
                ready_queue.push_back(process); //Add the process to the ready queue
                job_list.push_back(process); //Add it to the list of processes

                execution_status += print_exec_status(current_time, process.PID, NEW, READY);
            }
        }

        ///////////////////////MANAGE WAIT QUEUE/////////////////////////
        //This mainly involves keeping track of how long a process must remain in the ready queue
        for (auto i = wait_queue.begin(); i != wait_queue.end();){
            i->remaining_io--; //lowering countdown

            if (i -> remaining_io == 0){
                //this means I/O time reached zero, and its finished
                states old_state = i->process.state;
                i->process.state = READY;//changing the state to ready

                for (auto &p : job_list){ //added this for counting ready time after io return -> this is for metrics portion
                    if (p.PID == i->process.PID){
                        p.state = READY;
                        break;
                    }
                }

                ready_queue.push_back(i->process); //adding it to ready queue
                execution_status += print_exec_status(current_time, i->process.PID,old_state,READY); //logging the state change

                i = wait_queue.erase(i); //removing from the wait queue                
            }else{
                i ++; //move on  to next process if its not done
            }
        }

        /////////////////////////////////////////////////////////////////

        //////////////////////////SCHEDULER//////////////////////////////
        if(cpu_idle == false && ! ready_queue.empty()){ //CPU is running, and higher priority process is ready
            EP_RR_scheduler(ready_queue); //sort by the priority

            PCB &best = ready_queue.front(); //the next process (top)
            if (best.priority < running.priority){ //we peempt current running process

                states old_state = running.state;
                running.state = READY; 
                execution_status += print_exec_status(current_time,running.PID,old_state,READY); //logging state change

                for (auto &p : job_list) {
                    if (p.PID == running.PID) {
                        p.state = READY;
                        break;
                    }
                }
                
                ready_queue.push_back(running); //back of the queue
                running = best;
                ready_queue.erase(ready_queue.begin());
                states old_state2 = running.state;
                running.state = RUNNING;
                time_slice_used = 0;

                execution_status += print_exec_status(current_time,running.PID,old_state2,RUNNING); //logging state change

                for (auto &p : job_list) {
                    if (p.PID == running.PID) {
                        p.state = RUNNING;
                        break;
                    }
                }
            }
        } 
        if(cpu_idle == true && ! ready_queue.empty()){ //CPU is free
            EP_RR_scheduler(ready_queue); //sort by the priority

            PCB next = ready_queue.front(); //the next process (top)
            ready_queue.erase(ready_queue.begin());

            states old_state = next.state;
            next.state = RUNNING; 
            
            for (auto &p : job_list) {
                if (p.PID == next.PID) {
                    p.state = RUNNING;
                    break;
                }
            }

            running = next;
            cpu_idle = false; //cpu is not idle anymore
            time_slice_used = 0;

            execution_status += print_exec_status(current_time,running.PID,old_state,RUNNING); //logging state change
        } 

        if (cpu_idle == false){ //CPU is busy
            running.remaining_time --; //reduce CPU time
            running.time_until_next_io --; //reduce io timer
            time_slice_used++;

            if (running.remaining_time == 0){ //finished running
                states old_state = running.state;
                running.state = TERMINATED;
                execution_status += print_exec_status(current_time,running.PID,old_state,TERMINATED); //logging state change
                
                for (auto &p : job_list){ //here we are updating joblist, finding the pcb and marking as terminated
                    if (p.PID == running.PID){
                        p.state = TERMINATED;
                        p.completion_time = current_time; //saving finish time (for bonus)
                        break;
                    }
                }
                free_memory(running); //release from memory

                cpu_idle = true;
                idle_CPU(running);
            }
            else if (running.time_until_next_io == 0){ //checking for an io
                states old_state = running.state;
                running.state = WAITING;
                execution_status += print_exec_status(current_time,running.PID,old_state,WAITING); //logging state change

                //ADDING NEXT BLOCK FOR BONUS PORTION///////
                for (auto &p : job_list) { //trakcing time since the last io req for processes -> using this for the response time metric
                    if (p.PID == running.PID) {
                        if (p.last_io_req_time != 0) {  
                            unsigned int interval = current_time - p.last_io_req_time;
                            p.sum_io_intervals += interval;
                            p.i_count += 1;  
                        }
                        p.last_io_req_time = current_time;
                        break;
                    }
                }
                ///////////////////////////////////////////

                wait_queue.push_back({running, running.io_duration}); //adding to io queue
                running.time_until_next_io = running.io_freq; //timer is resetting (io timer)
                cpu_idle = true;
                idle_CPU(running);
            }
            else if(time_slice_used == 100){ //when quantum is over, its back of the queue
                states old_state = running.state;
                running.state = READY;
                execution_status += print_exec_status(current_time,running.PID,old_state,READY); //logging state change

                for (auto &p : job_list) {
                    if (p.PID == running.PID) {
                        p.state = READY;
                        break;
                    }
                }

                ready_queue.push_back(running);
                cpu_idle = true;
                idle_CPU(running);
            }
        }     
        /////////////////////////////////////////////////////////////////

        for (auto &p : job_list){ //go through every process and add one ms to waiting time if in queue
            if (p.state == READY){
                p.total_waiting_time ++;
            }
        }



        current_time++;

    }

    /////////////////////BLOCK FOR METRIC BONUS MARK ///////////////////////////

    unsigned int total_turnaround = 0;
    unsigned int total_wait = 0;
    unsigned int total_procs = job_list.size();
    unsigned int total_io_interval_sum = 0;
    unsigned int total_io_interval_count = 0;
    unsigned int finish_time_max = 0;

    for (auto &p : job_list){
        //Ccollecting all final stats by going through each process
        unsigned int turnaround = p.completion_time - p.arrival_time;
        total_turnaround += turnaround;
        total_wait += p.total_waiting_time;

        if (p.completion_time > finish_time_max){
            finish_time_max = p.completion_time;
        }

        total_io_interval_sum += p.sum_io_intervals;
        total_io_interval_count += p.i_count;
    }

    double avg_turnaround = (double)total_turnaround / total_procs;
    double avg_wait = (double)total_wait / total_procs;
    double throughput = (double) total_procs / finish_time_max;

    double avg_response_time = 0.0;
    if (total_io_interval_count > 0){
        avg_response_time = (double) total_io_interval_sum / total_io_interval_count;
    }

    //outputting metrics
    execution_status += "\nMETRICS:\n";
    execution_status += "Avg Turnaround: " + std::to_string(avg_turnaround) + "\n";
    execution_status += "Avg Waiting: " + std::to_string(avg_wait) + "\n";
    execution_status += "Throughput: " + std::to_string(throughput) + "\n";
    execution_status += "Avg Resp Time: " + std::to_string(avg_response_time) + "\n";

    
    //Close the output table
    execution_status += print_exec_footer();

    return std::make_tuple(execution_status);
}


int main(int argc, char** argv) {

    //Get the input file from the user
    if(argc != 2) {
        std::cout << "ERROR!\nExpected 1 argument, received " << argc - 1 << std::endl;
        std::cout << "To run the program, do: ./interrutps <your_input_file.txt>" << std::endl;
        return -1;
    }

    //Open the input file
    auto file_name = argv[1];
    std::ifstream input_file;
    input_file.open(file_name);

    //Ensure that the file actually opens
    if (!input_file.is_open()) {
        std::cerr << "Error: Unable to open file: " << file_name << std::endl;
        return -1;
    }

    //Parse the entire input file and populate a vector of PCBs.
    //To do so, the add_process() helper function is used (see include file).
    std::string line;
    std::vector<PCB> list_process;
    while(std::getline(input_file, line)) {
        auto input_tokens = split_delim(line, ", ");
        auto new_process = add_process(input_tokens);
        list_process.push_back(new_process);
    }
    input_file.close();

    //With the list of processes, run the simulation
    auto [exec] = run_simulation(list_process);

    write_output(exec, "execution.txt");

    return 0;
}